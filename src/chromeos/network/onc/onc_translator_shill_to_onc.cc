// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <utility>

#include "base/json/json_reader.h"
#include "base/json/json_string_value_serializer.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/strings/string_util.h"
#include "base/values.h"
#include "chromeos/components/onc/onc_signature.h"
#include "chromeos/components/onc/onc_utils.h"
#include "chromeos/network/network_profile_handler.h"
#include "chromeos/network/network_state.h"
#include "chromeos/network/network_util.h"
#include "chromeos/network/onc/network_onc_utils.h"
#include "chromeos/network/onc/onc_translation_tables.h"
#include "chromeos/network/onc/onc_translator.h"
#include "chromeos/network/shill_property_util.h"
#include "components/device_event_log/device_event_log.h"
#include "components/onc/onc_constants.h"
#include "third_party/cros_system_api/dbus/service_constants.h"
#include "third_party/cros_system_api/dbus/shill/dbus-constants.h"

namespace chromeos {
namespace onc {

namespace {

// Converts a VPN string to a base::Value of the given |type|. If the
// conversion fails, returns a default value for |type|.
base::Value ConvertVpnStringToValue(const std::string& str,
                                    base::Value::Type type) {
  if (type == base::Value::Type::STRING)
    return base::Value(str);

  absl::optional<base::Value> value = base::JSONReader::Read(str);
  if (!value || value->type() != type)
    return base::Value(type);

  return std::move(*value);
}

// Returns the string value of |key| from |dict| if found, or the empty string
// otherwise.
std::string FindStringKeyOrEmpty(const base::Value* dict,
                                 base::StringPiece key) {
  const std::string* value = dict->FindStringKey(key);
  return value ? *value : std::string();
}

// If the network is configured with an installed certificate, a PKCS11 id
// will be set which is provided for the UI to display certificate
// information. Returns true if the PKCS11 id is available and set.
bool SetPKCS11Id(const base::Value* shill_dictionary,
                 const char* cert_id_property,
                 const char* cert_slot_property,
                 base::Value* onc_object) {
  const std::string* shill_cert_id =
      shill_dictionary->FindStringKey(cert_id_property);
  if (!shill_cert_id || shill_cert_id->empty()) {
    return false;
  }

  const std::string* shill_slot =
      shill_dictionary->FindStringKey(cert_slot_property);
  std::string pkcs11_id;
  if (shill_slot && !shill_slot->empty()) {
    pkcs11_id = *shill_slot + ":" + *shill_cert_id;
  } else {
    pkcs11_id = *shill_cert_id;
  }

  onc_object->SetKey(::onc::client_cert::kClientCertType,
                     base::Value(::onc::client_cert::kPKCS11Id));
  onc_object->SetKey(::onc::client_cert::kClientCertPKCS11Id,
                     base::Value(pkcs11_id));
  return true;
}

// This class implements the translation of properties from the given
// |shill_dictionary| to a new ONC object of signature |onc_signature|. Using
// recursive calls to CreateTranslatedONCObject of new instances, nested
// objects are translated.
class ShillToONCTranslator {
 public:
  ShillToONCTranslator(const base::Value& shill_dictionary,
                       ::onc::ONCSource onc_source,
                       const OncValueSignature& onc_signature,
                       const NetworkState* network_state)
      : shill_dictionary_(&shill_dictionary),
        onc_source_(onc_source),
        onc_signature_(&onc_signature),
        network_state_(network_state) {
    field_translation_table_ = GetFieldTranslationTable(onc_signature);
  }

  ShillToONCTranslator(const base::Value& shill_dictionary,
                       ::onc::ONCSource onc_source,
                       const OncValueSignature& onc_signature,
                       const FieldTranslationEntry* field_translation_table,
                       const NetworkState* network_state)
      : shill_dictionary_(&shill_dictionary),
        onc_source_(onc_source),
        onc_signature_(&onc_signature),
        field_translation_table_(field_translation_table),
        network_state_(network_state) {}

  ShillToONCTranslator(const ShillToONCTranslator&) = delete;
  ShillToONCTranslator& operator=(const ShillToONCTranslator&) = delete;

  // Translates the associated Shill dictionary and creates an ONC object of
  // the given signature.
  std::unique_ptr<base::DictionaryValue> CreateTranslatedONCObject();

 private:
  void TranslateEthernet();
  void TranslateOpenVPN();
  void TranslateIPsec();
  void TranslateL2TP();
  void TranslateThirdPartyVPN();
  void TranslateVPN();
  void TranslateWiFiWithState();
  void TranslateCellularWithState();
  void TranslateCellularDevice();
  void TranslateNetworkWithState();
  void TranslateIPConfig();
  void TranslateSavedOrStaticIPConfig();
  void TranslateSavedIPConfig();
  void TranslateStaticIPConfig();
  void TranslateEap();

  // Creates an ONC object from |dictionary| according to the signature
  // associated to |onc_field_name| and adds it to |onc_object_| at
  // |onc_field_name|.
  void TranslateAndAddNestedObject(const std::string& onc_field_name,
                                   const base::Value& dictionary);

  // Creates an ONC object from |shill_dictionary_| according to the signature
  // associated to |onc_field_name| and adds it to |onc_object_| at
  // |onc_field_name|.
  void TranslateAndAddNestedObject(const std::string& onc_field_name);

  // Sets |onc_field_name| in dictionary |onc_dictionary_name| in
  // |onc_object_| to |value| if the dictionary exists.
  void SetNestedOncValue(const std::string& onc_dictionary_name,
                         const std::string& onc_field_name,
                         const base::Value& value);

  // Translates a list of nested objects and adds the list to |onc_object_| at
  // |onc_field_name|. If there are errors while parsing individual objects or
  // if the resulting list contains no entries, the result will not be added
  // to |onc_object_|.
  void TranslateAndAddListOfObjects(const std::string& onc_field_name,
                                    const base::Value& list);

  // Applies function CopyProperty to each field of |value_signature| and its
  // base signatures.
  void CopyPropertiesAccordingToSignature(
      const OncValueSignature* value_signature);

  // Applies function CopyProperty to each field of |onc_signature_| and its
  // base signatures.
  void CopyPropertiesAccordingToSignature();

  // If |shill_property_name| is defined in |field_signature|, copies this
  // entry from |shill_dictionary_| to |onc_object_| if it exists.
  void CopyProperty(const OncFieldSignature* field_signature);

  // Applies defaults to fields according to |onc_signature_|.
  void SetDefaultsAccordingToSignature();

  // Applies defaults to fields according to |value_signature|.
  void SetDefaultsAccordingToSignature(
      const OncValueSignature* value_signature);

  // If existent, translates the entry at |shill_property_name| in
  // |shill_dictionary_| using |table|. It is an error if no matching table
  // entry is found. Writes the result as entry at |onc_field_name| in
  // |onc_object_|.
  void TranslateWithTableAndSet(const std::string& shill_property_name,
                                const StringTranslationEntry table[],
                                const std::string& onc_field_name);

  // Returns the name of the Shill service provided in |shill_dictionary_|
  // for debugging.
  std::string GetName();

  const base::Value* shill_dictionary_;
  ::onc::ONCSource onc_source_;
  const OncValueSignature* onc_signature_;
  const FieldTranslationEntry* field_translation_table_;
  std::unique_ptr<base::Value> onc_object_;
  const NetworkState* network_state_;
};

std::unique_ptr<base::DictionaryValue>
ShillToONCTranslator::CreateTranslatedONCObject() {
  onc_object_ = std::make_unique<base::Value>(base::Value::Type::DICTIONARY);
  if (onc_signature_ == &kNetworkWithStateSignature) {
    TranslateNetworkWithState();
  } else if (onc_signature_ == &kEthernetSignature) {
    TranslateEthernet();
  } else if (onc_signature_ == &kVPNSignature) {
    TranslateVPN();
  } else if (onc_signature_ == &kOpenVPNSignature) {
    TranslateOpenVPN();
  } else if (onc_signature_ == &kIPsecSignature) {
    TranslateIPsec();
  } else if (onc_signature_ == &kL2TPSignature) {
    TranslateL2TP();
  } else if (onc_signature_ == &kThirdPartyVPNSignature) {
    TranslateThirdPartyVPN();
  } else if (onc_signature_ == &kWiFiWithStateSignature) {
    TranslateWiFiWithState();
  } else if (onc_signature_ == &kCellularWithStateSignature) {
    if (field_translation_table_ == kCellularDeviceTable)
      TranslateCellularDevice();
    else
      TranslateCellularWithState();
  } else if (onc_signature_ == &kIPConfigSignature) {
    TranslateIPConfig();
  } else if (onc_signature_ == &kSavedIPConfigSignature) {
    TranslateSavedIPConfig();
  } else if (onc_signature_ == &kStaticIPConfigSignature) {
    TranslateStaticIPConfig();
  } else if (onc_signature_ == &kEAPSignature) {
    TranslateEap();
  } else {
    CopyPropertiesAccordingToSignature();
  }

  SetDefaultsAccordingToSignature();

  return base::DictionaryValue::From(std::move(onc_object_));
}

void ShillToONCTranslator::TranslateEthernet() {
  const std::string* shill_network_type =
      shill_dictionary_->FindStringKey(shill::kTypeProperty);
  const char* onc_auth = ::onc::ethernet::kAuthenticationNone;
  if (shill_network_type && *shill_network_type == shill::kTypeEthernetEap)
    onc_auth = ::onc::ethernet::k8021X;
  onc_object_->SetKey(::onc::ethernet::kAuthentication, base::Value(onc_auth));

  if (shill_network_type && *shill_network_type == shill::kTypeEthernetEap)
    TranslateAndAddNestedObject(::onc::ethernet::kEAP);
}

void ShillToONCTranslator::TranslateOpenVPN() {
  if (shill_dictionary_->FindKey(shill::kOpenVPNVerifyX509NameProperty))
    TranslateAndAddNestedObject(::onc::openvpn::kVerifyX509);

  // Shill supports only one RemoteCertKU but ONC requires a list. If
  // existing, wraps the value into a list.
  const std::string* certKU =
      shill_dictionary_->FindStringKey(shill::kOpenVPNRemoteCertKUProperty);
  if (certKU) {
    base::Value certKUs(base::Value::Type::LIST);
    certKUs.Append(base::Value(*certKU));
    onc_object_->SetKey(::onc::openvpn::kRemoteCertKU, std::move(certKUs));
  }

  SetPKCS11Id(shill_dictionary_, shill::kOpenVPNClientCertIdProperty, "",
              onc_object_.get());

  TranslateWithTableAndSet(shill::kOpenVPNCompressProperty,
                           kOpenVpnCompressionAlgorithmTable,
                           ::onc::openvpn::kCompressionAlgorithm);

  for (const OncFieldSignature* field_signature = onc_signature_->fields;
       field_signature->onc_field_name != NULL; ++field_signature) {
    const std::string& onc_field_name = field_signature->onc_field_name;
    if (onc_field_name == ::onc::openvpn::kRemoteCertKU ||
        onc_field_name == ::onc::openvpn::kServerCAPEMs) {
      CopyProperty(field_signature);
      continue;
    }

    std::string shill_property_name;
    if (!field_translation_table_ ||
        !GetShillPropertyName(field_signature->onc_field_name,
                              field_translation_table_, &shill_property_name)) {
      continue;
    }
    const base::Value* shill_value =
        shill_dictionary_->FindKey(shill_property_name);
    if (!shill_value) {
      continue;
    }

    if (shill_value->is_string()) {
      // Shill wants all Provider/VPN fields to be strings. Translates these
      // strings back to the correct ONC type.
      base::Value translated = ConvertVpnStringToValue(
          shill_value->GetString(), field_signature->value_signature->onc_type);

      if (translated.is_none()) {
        NET_LOG(ERROR) << "Shill property '" << shill_property_name
                       << "' with value " << *shill_value
                       << " couldn't be converted to base::Value::Type "
                       << field_signature->value_signature->onc_type << ": "
                       << GetName();
      } else {
        onc_object_->SetKey(onc_field_name, std::move(translated));
      }
    } else {
      NET_LOG(ERROR) << "Shill property '" << shill_property_name
                     << "' has value " << *shill_value
                     << ", but expected a string: " << GetName();
    }
  }
}

void ShillToONCTranslator::TranslateIPsec() {
  CopyPropertiesAccordingToSignature();
  if (shill_dictionary_->FindKey(shill::kL2tpIpsecXauthUserProperty))
    TranslateAndAddNestedObject(::onc::ipsec::kXAUTH);

  std::string authentication_type;
  if (SetPKCS11Id(shill_dictionary_, shill::kL2tpIpsecClientCertIdProperty,
                  shill::kL2tpIpsecClientCertSlotProperty, onc_object_.get())) {
    authentication_type = ::onc::ipsec::kCert;
  } else {
    authentication_type = ::onc::ipsec::kPSK;
  }
  onc_object_->SetKey(::onc::ipsec::kAuthenticationType,
                      base::Value(authentication_type));
}

void ShillToONCTranslator::TranslateL2TP() {
  CopyPropertiesAccordingToSignature();

  const base::Value* lcp_echo_disabled =
      shill_dictionary_->FindKey(shill::kL2tpIpsecLcpEchoDisabledProperty);
  if (lcp_echo_disabled && lcp_echo_disabled->is_string()) {
    base::Value lcp_echo_disabled_value = ConvertVpnStringToValue(
        lcp_echo_disabled->GetString(), base::Value::Type::BOOLEAN);
    onc_object_->SetKey(::onc::l2tp::kLcpEchoDisabled,
                        std::move(lcp_echo_disabled_value));
  }
}

void ShillToONCTranslator::TranslateThirdPartyVPN() {
  CopyPropertiesAccordingToSignature();

  // For third-party VPNs, |shill::kProviderHostProperty| is used to store the
  // provider's extension ID.
  const std::string* shill_extension_id =
      shill_dictionary_->FindStringKey(shill::kHostProperty);
  onc_object_->SetKey(
      ::onc::third_party_vpn::kExtensionID,
      base::Value(shill_extension_id ? *shill_extension_id : std::string()));
}

void ShillToONCTranslator::TranslateVPN() {
  CopyPropertiesAccordingToSignature();

  // Parse Shill Provider dictionary. Note, this may not exist, e.g. if we are
  // just translating network state in
  // network_util::TranslateNetworkStateToONC.
  const base::Value* provider =
      shill_dictionary_->FindDictKey(shill::kProviderProperty);
  if (!provider) {
    return;
  }
  std::string shill_provider_type =
      FindStringKeyOrEmpty(provider, shill::kTypeProperty);
  std::string onc_provider_type;
  if (!TranslateStringToONC(kVPNTypeTable, shill_provider_type,
                            &onc_provider_type)) {
    return;
  }
  onc_object_->SetKey(::onc::vpn::kType, base::Value(onc_provider_type));
  const std::string* shill_provider_host =
      provider->FindStringKey(shill::kHostProperty);
  if (onc_provider_type != ::onc::vpn::kThirdPartyVpn && shill_provider_host) {
    onc_object_->SetKey(::onc::vpn::kHost, base::Value(*shill_provider_host));
  }

  // Translate the nested dictionary.
  std::string provider_type_dictionary;
  if (onc_provider_type == ::onc::vpn::kTypeL2TP_IPsec) {
    TranslateAndAddNestedObject(::onc::vpn::kIPsec, *provider);
    TranslateAndAddNestedObject(::onc::vpn::kL2TP, *provider);
    provider_type_dictionary = ::onc::vpn::kIPsec;
  } else {
    TranslateAndAddNestedObject(onc_provider_type, *provider);
    provider_type_dictionary = onc_provider_type;
  }

  absl::optional<bool> save_credentials =
      shill_dictionary_->FindBoolKey(shill::kSaveCredentialsProperty);
  if (onc_provider_type != ::onc::vpn::kThirdPartyVpn &&
      onc_provider_type != ::onc::vpn::kArcVpn && save_credentials) {
    SetNestedOncValue(provider_type_dictionary, ::onc::vpn::kSaveCredentials,
                      base::Value(*save_credentials));
  }
}

void ShillToONCTranslator::TranslateWiFiWithState() {
  const std::string* shill_security =
      shill_dictionary_->FindStringKey(shill::kSecurityClassProperty);
  const std::string* shill_key_mgmt =
      shill_dictionary_->FindStringKey(shill::kEapKeyMgmtProperty);
  if (shill_security && *shill_security == shill::kSecurityWep &&
      shill_key_mgmt && *shill_key_mgmt == shill::kKeyManagementIEEE8021X) {
    onc_object_->SetKey(::onc::wifi::kSecurity,
                        base::Value(::onc::wifi::kWEP_8021X));
  } else {
    TranslateWithTableAndSet(shill::kSecurityClassProperty, kWiFiSecurityTable,
                             ::onc::wifi::kSecurity);
  }

  bool unknown_encoding = true;
  std::string ssid = shill_property_util::GetSSIDFromProperties(
      *shill_dictionary_, false /* verbose_logging */, &unknown_encoding);
  if (!unknown_encoding && !ssid.empty())
    onc_object_->SetKey(::onc::wifi::kSSID, base::Value(ssid));

  absl::optional<bool> link_monitor_disable =
      shill_dictionary_->FindBoolKey(shill::kLinkMonitorDisableProperty);
  if (link_monitor_disable) {
    onc_object_->SetKey(::onc::wifi::kAllowGatewayARPPolling,
                        base::Value(!*link_monitor_disable));
  }

  CopyPropertiesAccordingToSignature();
  TranslateAndAddNestedObject(::onc::wifi::kEAP);
}

void ShillToONCTranslator::TranslateCellularWithState() {
  CopyPropertiesAccordingToSignature();
  TranslateWithTableAndSet(shill::kActivationStateProperty,
                           kActivationStateTable,
                           ::onc::cellular::kActivationState);
  TranslateWithTableAndSet(shill::kNetworkTechnologyProperty,
                           kNetworkTechnologyTable,
                           ::onc::cellular::kNetworkTechnology);
  const base::Value* dictionary =
      shill_dictionary_->FindDictKey(shill::kServingOperatorProperty);
  if (dictionary) {
    TranslateAndAddNestedObject(::onc::cellular::kServingOperator, *dictionary);
  }
  dictionary = shill_dictionary_->FindDictKey(shill::kCellularApnProperty);
  if (dictionary) {
    TranslateAndAddNestedObject(::onc::cellular::kAPN, *dictionary);
  }
  dictionary =
      shill_dictionary_->FindDictKey(shill::kCellularLastGoodApnProperty);
  if (dictionary) {
    TranslateAndAddNestedObject(::onc::cellular::kLastGoodAPN, *dictionary);
  }
  dictionary = shill_dictionary_->FindDictKey(shill::kPaymentPortalProperty);
  if (dictionary) {
    TranslateAndAddNestedObject(::onc::cellular::kPaymentPortal, *dictionary);
  }

  const base::Value* device_dictionary =
      shill_dictionary_->FindDictKey(shill::kDeviceProperty);
  bool requires_roaming = false;
  bool scanning = false;
  if (device_dictionary) {
    // Merge the Device dictionary with this one (Cellular) using the
    // CellularDevice signature.
    ShillToONCTranslator nested_translator(
        *device_dictionary, onc_source_, kCellularWithStateSignature,
        kCellularDeviceTable, network_state_);
    std::unique_ptr<base::DictionaryValue> nested_object =
        nested_translator.CreateTranslatedONCObject();
    onc_object_->MergeDictionary(nested_object.get());

    // Both the Scanning property and the ProviderRequiresRoaming property are
    // retrieved from the Device dictionary, but only if this is the active
    // SIM, meaning that the service ICCID matches the device ICCID.
    const std::string* service_iccid =
        onc_object_->FindStringKey(::onc::cellular::kICCID);
    if (service_iccid) {
      const std::string* device_iccid =
          device_dictionary->FindStringKey(shill::kIccidProperty);
      if (device_iccid && *service_iccid == *device_iccid) {
        requires_roaming =
            device_dictionary
                ->FindBoolKey(shill::kProviderRequiresRoamingProperty)
                .value_or(false);
        scanning = device_dictionary->FindBoolKey(shill::kScanningProperty)
                       .value_or(false);
      }
    }
  }
  if (requires_roaming) {
    onc_object_->SetKey(::onc::cellular::kRoamingState,
                        base::Value(::onc::cellular::kRoamingRequired));
  } else {
    TranslateWithTableAndSet(shill::kRoamingStateProperty, kRoamingStateTable,
                             ::onc::cellular::kRoamingState);
  }
  onc_object_->SetKey(::onc::cellular::kScanning, base::Value(scanning));
}

void ShillToONCTranslator::TranslateCellularDevice() {
  CopyPropertiesAccordingToSignature();
  const base::Value* shill_sim_lock_status =
      shill_dictionary_->FindDictKey(shill::kSIMLockStatusProperty);
  if (shill_sim_lock_status) {
    TranslateAndAddNestedObject(::onc::cellular::kSIMLockStatus,
                                *shill_sim_lock_status);
  }
  const base::Value* shill_home_provider =
      shill_dictionary_->FindDictKey(shill::kHomeProviderProperty);
  if (shill_home_provider) {
    TranslateAndAddNestedObject(::onc::cellular::kHomeProvider,
                                *shill_home_provider);
  }
  const base::Value* shill_apns =
      shill_dictionary_->FindListKey(shill::kCellularApnListProperty);
  if (shill_apns) {
    TranslateAndAddListOfObjects(::onc::cellular::kAPNList, *shill_apns);
  }
  const base::Value* shill_found_networks =
      shill_dictionary_->FindListKey(shill::kFoundNetworksProperty);
  if (shill_found_networks) {
    TranslateAndAddListOfObjects(::onc::cellular::kFoundNetworks,
                                 *shill_found_networks);
  }
}

void ShillToONCTranslator::TranslateNetworkWithState() {
  CopyPropertiesAccordingToSignature();

  std::string shill_network_type =
      FindStringKeyOrEmpty(shill_dictionary_, shill::kTypeProperty);
  std::string onc_network_type = ::onc::network_type::kEthernet;
  if (shill_network_type != shill::kTypeEthernet &&
      shill_network_type != shill::kTypeEthernetEap) {
    TranslateStringToONC(kNetworkTypeTable, shill_network_type,
                         &onc_network_type);
  }
  // Translate nested Cellular, WiFi, etc. properties.
  if (!onc_network_type.empty()) {
    onc_object_->SetKey(::onc::network_config::kType,
                        base::Value(onc_network_type));
    TranslateAndAddNestedObject(onc_network_type);
  }

  // Since Name is a read only field in Shill unless it's a VPN, it is copied
  // here, but not when going the other direction (if it's not a VPN).
  std::string name =
      FindStringKeyOrEmpty(shill_dictionary_, shill::kNameProperty);
  onc_object_->SetKey(::onc::network_config::kName, base::Value(name));

  // Limit ONC state to "NotConnected", "Connected", or "Connecting".
  const std::string* state =
      shill_dictionary_->FindStringKey(shill::kStateProperty);
  if (state) {
    std::string onc_state = ::onc::connection_state::kNotConnected;
    if (NetworkState::StateIsConnected(*state)) {
      onc_state = ::onc::connection_state::kConnected;
    } else if (NetworkState::StateIsConnecting(*state)) {
      onc_state = ::onc::connection_state::kConnecting;
    }
    onc_object_->SetKey(::onc::network_config::kConnectionState,
                        base::Value(onc_state));
  }

  if (network_state_) {
    // Only visible networks set RestrictedConnectivity, and only if true.
    if (network_state_->IsCaptivePortal()) {
      onc_object_->SetKey(::onc::network_config::kRestrictedConnectivity,
                          base::Value(true));
    }
    // Only visible networks set ErrorState, and only if not empty.
    if (!network_state_->GetError().empty()) {
      onc_object_->SetKey(::onc::network_config::kErrorState,
                          base::Value(network_state_->GetError()));
    }
  }

  const std::string* profile_path =
      shill_dictionary_->FindStringKey(shill::kProfileProperty);
  if (onc_source_ != ::onc::ONC_SOURCE_UNKNOWN && profile_path) {
    std::string source;
    if (onc_source_ == ::onc::ONC_SOURCE_DEVICE_POLICY)
      source = ::onc::network_config::kSourceDevicePolicy;
    else if (onc_source_ == ::onc::ONC_SOURCE_USER_POLICY)
      source = ::onc::network_config::kSourceUserPolicy;
    else if (*profile_path == NetworkProfileHandler::GetSharedProfilePath())
      source = ::onc::network_config::kSourceDevice;
    else if (!profile_path->empty())
      source = ::onc::network_config::kSourceUser;
    else
      source = ::onc::network_config::kSourceNone;
    onc_object_->SetKey(::onc::network_config::kSource, base::Value(source));
  }

  // Use a human-readable aa:bb format for any hardware MAC address. Note:
  // this property is provided by the caller but is not part of the Shill
  // Service properties (it is copied from the Device properties).
  const std::string* address =
      shill_dictionary_->FindStringKey(shill::kAddressProperty);
  if (address) {
    onc_object_->SetKey(
        ::onc::network_config::kMacAddress,
        base::Value(network_util::FormattedMacAddress(*address)));
  }

  // Shill's Service has an IPConfig property (note the singular), not an
  // IPConfigs property. However, we require the caller of the translation to
  // patch the Shill dictionary before passing it to the translator.
  const base::Value* shill_ipconfigs =
      shill_dictionary_->FindListKey(shill::kIPConfigsProperty);
  if (shill_ipconfigs) {
    TranslateAndAddListOfObjects(::onc::network_config::kIPConfigs,
                                 *shill_ipconfigs);
  }

  const base::Value* saved_ipconfig =
      shill_dictionary_->FindDictKey(shill::kSavedIPConfigProperty);
  if (saved_ipconfig) {
    TranslateAndAddNestedObject(::onc::network_config::kSavedIPConfig,
                                *saved_ipconfig);
  }

  // Translate the StaticIPConfig object and set the IP config types.
  const base::Value* static_ipconfig =
      shill_dictionary_->FindDictKey(shill::kStaticIPConfigProperty);
  if (static_ipconfig) {
    const std::string* ip_address =
        static_ipconfig->FindStringKey(shill::kAddressProperty);
    if (ip_address && !ip_address->empty()) {
      onc_object_->SetKey(
          ::onc::network_config::kIPAddressConfigType,
          base::Value(::onc::network_config::kIPConfigTypeStatic));
    }
    const base::Value* name_servers =
        static_ipconfig->FindListKey(shill::kNameServersProperty);
    if (name_servers && !name_servers->GetList().empty()) {
      onc_object_->SetKey(
          ::onc::network_config::kNameServersConfigType,
          base::Value(::onc::network_config::kIPConfigTypeStatic));
    }
    if ((ip_address && !ip_address->empty()) ||
        (name_servers && !name_servers->GetList().empty())) {
      TranslateAndAddNestedObject(::onc::network_config::kStaticIPConfig,
                                  *static_ipconfig);
    }
  }

  const std::string* proxy_config_str =
      shill_dictionary_->FindStringKey(shill::kProxyConfigProperty);
  if (proxy_config_str && !proxy_config_str->empty()) {
    base::Value proxy_config_value = ReadDictionaryFromJson(*proxy_config_str);
    if (!proxy_config_value.is_none()) {
      base::Value proxy_settings =
          ConvertProxyConfigToOncProxySettings(proxy_config_value);
      if (!proxy_settings.is_none()) {
        onc_object_->SetKey(::onc::network_config::kProxySettings,
                            std::move(proxy_settings));
      }
    }
  }

  absl::optional<double> traffic_counter_reset_time =
      shill_dictionary_->FindDoubleKey(shill::kTrafficCounterResetTimeProperty);
  if (traffic_counter_reset_time.has_value()) {
    onc_object_->SetKey(::onc::network_config::kTrafficCounterResetTime,
                        base::Value(traffic_counter_reset_time.value()));
  }
}

void ShillToONCTranslator::TranslateIPConfig() {
  CopyPropertiesAccordingToSignature();
  std::string shill_ip_method =
      FindStringKeyOrEmpty(shill_dictionary_, shill::kMethodProperty);
  std::string type;
  if (shill_ip_method == shill::kTypeIPv4 ||
      shill_ip_method == shill::kTypeDHCP) {
    type = ::onc::ipconfig::kIPv4;
  } else if (shill_ip_method == shill::kTypeIPv6 ||
             shill_ip_method == shill::kTypeDHCP6) {
    type = ::onc::ipconfig::kIPv6;
  } else {
    return;  // Ignore unhandled IPConfig types, e.g. bootp, zeroconf, ppp
  }

  onc_object_->SetKey(::onc::ipconfig::kType, base::Value(type));
}

void ShillToONCTranslator::TranslateSavedOrStaticIPConfig() {
  CopyPropertiesAccordingToSignature();

  // Static and Saved IPConfig in Shill are always of type IPv4. Set this type
  // in ONC, but not if the object would be empty except the type.
  if (!onc_object_->DictEmpty()) {
    onc_object_->SetKey(::onc::ipconfig::kType,
                        base::Value(::onc::ipconfig::kIPv4));
  }
}

void ShillToONCTranslator::TranslateSavedIPConfig() {
  TranslateSavedOrStaticIPConfig();
}

void ShillToONCTranslator::TranslateStaticIPConfig() {
  TranslateSavedOrStaticIPConfig();
}

void ShillToONCTranslator::TranslateEap() {
  CopyPropertiesAccordingToSignature();

  // Translate EAP Outer and Inner values if EAP.EAP exists and is not empty.
  const std::string* shill_eap_method =
      shill_dictionary_->FindStringKey(shill::kEapMethodProperty);
  if (shill_eap_method && !shill_eap_method->empty()) {
    TranslateWithTableAndSet(shill::kEapMethodProperty, kEAPOuterTable,
                             ::onc::eap::kOuter);
    const std::string* shill_phase2_auth =
        shill_dictionary_->FindStringKey(shill::kEapPhase2AuthProperty);
    if (shill_phase2_auth && !shill_phase2_auth->empty()) {
      TranslateWithTableAndSet(shill::kEapPhase2AuthProperty,
                               kEAP_TTLS_InnerTable, ::onc::eap::kInner);
    }
  }

  const std::string* shill_cert_id =
      shill_dictionary_->FindStringKey(shill::kEapCertIdProperty);
  if (shill_cert_id) {
    onc_object_->SetKey(::onc::client_cert::kClientCertType,
                        base::Value(::onc::client_cert::kPKCS11Id));
    // Note: shill::kEapCertIdProperty is already in the format slot:key_id.
    // Note: shill::kEapKeyIdProperty has the same value as
    //       shill::kEapCertIdProperty and is ignored.
    onc_object_->SetKey(::onc::client_cert::kClientCertPKCS11Id,
                        base::Value(*shill_cert_id));
  }

  bool use_login_password =
      shill_dictionary_->FindBoolKey(shill::kEapUseLoginPasswordProperty)
          .value_or(false);
  if (use_login_password) {
    onc_object_->SetKey(
        ::onc::eap::kPassword,
        base::Value(::onc::substitutes::kPasswordPlaceholderVerbatim));
  }

  // Set shill::kEapSubjectAlternativeNameMatchProperty to the serialized form
  // of the subject alternative name match list of dictionaries.
  const base::Value* subject_alternative_name_match =
      shill_dictionary_->FindListKey(
          shill::kEapSubjectAlternativeNameMatchProperty);

  if (subject_alternative_name_match) {
    base::Value deserialized_dicts(base::Value::Type::LIST);
    std::string error_msg;
    for (const base::Value& san : subject_alternative_name_match->GetList()) {
      JSONStringValueDeserializer deserializer(san.GetString());
      auto deserialized_dict =
          deserializer.Deserialize(/*error_code=*/nullptr, &error_msg);
      if (!deserialized_dict) {
        LOG(ERROR) << "failed to deserialize " << san << " with error "
                   << error_msg;
        continue;
      }
      deserialized_dicts.Append(std::move(*deserialized_dict));
    }
    onc_object_->SetKey(::onc::eap::kSubjectAlternativeNameMatch,
                        std::move(deserialized_dicts));
  }
}

void ShillToONCTranslator::TranslateAndAddNestedObject(
    const std::string& onc_field_name) {
  TranslateAndAddNestedObject(onc_field_name, *shill_dictionary_);
}

void ShillToONCTranslator::TranslateAndAddNestedObject(
    const std::string& onc_field_name,
    const base::Value& dictionary) {
  const OncFieldSignature* field_signature =
      GetFieldSignature(*onc_signature_, onc_field_name);
  if (!field_signature) {
    NET_LOG(ERROR) << "Unable to find signature for field: " << onc_field_name;
    return;
  }
  ShillToONCTranslator nested_translator(dictionary, onc_source_,
                                         *field_signature->value_signature,
                                         network_state_);
  std::unique_ptr<base::DictionaryValue> nested_object =
      nested_translator.CreateTranslatedONCObject();
  if (nested_object->DictEmpty())
    return;
  onc_object_->SetKey(onc_field_name, std::move(*nested_object));
}

void ShillToONCTranslator::SetNestedOncValue(
    const std::string& onc_dictionary_name,
    const std::string& onc_field_name,
    const base::Value& value) {
  onc_object_->SetPath({onc_dictionary_name, onc_field_name}, value.Clone());
}

void ShillToONCTranslator::TranslateAndAddListOfObjects(
    const std::string& onc_field_name,
    const base::Value& list) {
  const OncFieldSignature* field_signature =
      GetFieldSignature(*onc_signature_, onc_field_name);
  if (field_signature->value_signature->onc_type != base::Value::Type::LIST) {
    NET_LOG(ERROR) << "ONC Field name: '" << onc_field_name << "' has type '"
                   << field_signature->value_signature->onc_type
                   << "', expected: base::Value::Type::LIST: " << GetName();
    return;
  }
  DCHECK(field_signature->value_signature->onc_array_entry_signature);
  base::Value result(base::Value::Type::LIST);
  for (const auto& it : list.GetList()) {
    if (!it.is_dict())
      continue;
    ShillToONCTranslator nested_translator(
        it, onc_source_,
        *field_signature->value_signature->onc_array_entry_signature,
        network_state_);
    std::unique_ptr<base::DictionaryValue> nested_object =
        nested_translator.CreateTranslatedONCObject();
    // If the nested object couldn't be parsed, simply omit it.
    if (nested_object->DictEmpty())
      continue;
    result.Append(std::move(*nested_object));
  }
  // If there are no entries in the list, there is no need to expose this
  // field.
  if (result.GetList().empty())
    return;
  onc_object_->SetKey(onc_field_name, std::move(result));
}

void ShillToONCTranslator::CopyPropertiesAccordingToSignature() {
  CopyPropertiesAccordingToSignature(onc_signature_);
}

void ShillToONCTranslator::CopyPropertiesAccordingToSignature(
    const OncValueSignature* value_signature) {
  if (value_signature->base_signature)
    CopyPropertiesAccordingToSignature(value_signature->base_signature);
  if (!value_signature->fields)
    return;
  for (const OncFieldSignature* field_signature = value_signature->fields;
       field_signature->onc_field_name != NULL; ++field_signature) {
    CopyProperty(field_signature);
  }
}

void ShillToONCTranslator::CopyProperty(
    const OncFieldSignature* field_signature) {
  std::string shill_property_name;
  if (!field_translation_table_ ||
      !GetShillPropertyName(field_signature->onc_field_name,
                            field_translation_table_, &shill_property_name)) {
    return;
  }
  const base::Value* shill_value =
      shill_dictionary_->FindKey(shill_property_name);
  if (!shill_value) {
    return;
  }

  if (shill_value->type() != field_signature->value_signature->onc_type) {
    NET_LOG(ERROR) << "Shill property '" << shill_property_name
                   << "' with value " << *shill_value
                   << " has base::Value::Type " << shill_value->type()
                   << " but ONC field '" << field_signature->onc_field_name
                   << "' requires type "
                   << field_signature->value_signature->onc_type << ": "
                   << GetName();
    return;
  }

  onc_object_->SetKey(field_signature->onc_field_name, shill_value->Clone());
}

void ShillToONCTranslator::SetDefaultsAccordingToSignature() {
  SetDefaultsAccordingToSignature(onc_signature_);
}

void ShillToONCTranslator::SetDefaultsAccordingToSignature(
    const OncValueSignature* value_signature) {
  if (value_signature->base_signature)
    SetDefaultsAccordingToSignature(value_signature->base_signature);
  if (!value_signature->fields)
    return;
  for (const OncFieldSignature* field_signature = value_signature->fields;
       field_signature->onc_field_name != nullptr; ++field_signature) {
    if (!field_signature->default_value_setter)
      continue;
    if (onc_object_->FindKey(field_signature->onc_field_name))
      continue;
    onc_object_->SetKey(field_signature->onc_field_name,
                        field_signature->default_value_setter());
  }
}

void ShillToONCTranslator::TranslateWithTableAndSet(
    const std::string& shill_property_name,
    const StringTranslationEntry table[],
    const std::string& onc_field_name) {
  const std::string* shill_value =
      shill_dictionary_->FindStringKey(shill_property_name);
  if (!shill_value || shill_value->empty()) {
    return;
  }
  std::string onc_value;
  if (TranslateStringToONC(table, *shill_value, &onc_value)) {
    onc_object_->SetKey(onc_field_name, base::Value(onc_value));
    return;
  }
  NET_LOG(ERROR) << "Shill property '" << shill_property_name << "' with value "
                 << *shill_value
                 << " couldn't be translated to ONC: " << GetName();
}

std::string ShillToONCTranslator::GetName() {
  DCHECK(shill_dictionary_);
  const std::string* name =
      shill_dictionary_->FindStringKey(shill::kNameProperty);
  return name ? *name : std::string();
}

}  // namespace

std::unique_ptr<base::DictionaryValue> TranslateShillServiceToONCPart(
    const base::Value& shill_dictionary,
    ::onc::ONCSource onc_source,
    const OncValueSignature* onc_signature,
    const NetworkState* network_state) {
  CHECK(onc_signature != NULL);

  ShillToONCTranslator translator(shill_dictionary, onc_source, *onc_signature,
                                  network_state);
  return translator.CreateTranslatedONCObject();
}

}  // namespace onc
}  // namespace chromeos
