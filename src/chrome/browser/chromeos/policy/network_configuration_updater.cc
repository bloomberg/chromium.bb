// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/network_configuration_updater.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/logging.h"
#include "base/values.h"
#include "chromeos/network/onc/onc_utils.h"
#include "components/policy/core/common/policy_map.h"
#include "components/policy/policy_constants.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_util_nss.h"

using chromeos::onc::OncParsedCertificates;

namespace policy {

namespace {

// A predicate used for filtering server or authority certificates.
using ServerOrAuthorityCertPredicate = base::RepeatingCallback<bool(
    const OncParsedCertificates::ServerOrAuthorityCertificate& cert)>;

net::CertificateList GetFilteredCertificateListFromOnc(
    const std::vector<OncParsedCertificates::ServerOrAuthorityCertificate>&
        server_or_authority_certificates,
    ServerOrAuthorityCertPredicate predicate) {
  net::CertificateList certificates;
  for (const auto& server_or_authority_cert :
       server_or_authority_certificates) {
    if (predicate.Run(server_or_authority_cert))
      certificates.push_back(server_or_authority_cert.certificate());
  }
  return certificates;
}

}  // namespace

NetworkConfigurationUpdater::~NetworkConfigurationUpdater() {
  policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, this);
}

void NetworkConfigurationUpdater::OnPolicyUpdated(const PolicyNamespace& ns,
                                                  const PolicyMap& previous,
                                                  const PolicyMap& current) {
  // Ignore this call. Policy changes are already observed by the registrar.
}

void NetworkConfigurationUpdater::OnPolicyServiceInitialized(
    PolicyDomain domain) {
  if (domain != POLICY_DOMAIN_CHROME)
    return;

  if (policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME)) {
    VLOG(1) << LogHeader() << " initialized.";
    policy_service_->RemoveObserver(POLICY_DOMAIN_CHROME, this);
    ApplyPolicy();
  }
}

void NetworkConfigurationUpdater::AddPolicyProvidedCertsObserver(
    chromeos::PolicyCertificateProvider::Observer* observer) {
  observer_list_.AddObserver(observer);
}

void NetworkConfigurationUpdater::RemovePolicyProvidedCertsObserver(
    chromeos::PolicyCertificateProvider::Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

net::CertificateList
NetworkConfigurationUpdater::GetAllServerAndAuthorityCertificates() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetFilteredCertificateListFromOnc(
      certs_->server_or_authority_certificates(),
      base::BindRepeating(
          [](const OncParsedCertificates::ServerOrAuthorityCertificate& cert) {
            return true;
          }));
}

net::CertificateList NetworkConfigurationUpdater::GetAllAuthorityCertificates()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return GetFilteredCertificateListFromOnc(
      certs_->server_or_authority_certificates(),
      base::BindRepeating(
          [](const OncParsedCertificates::ServerOrAuthorityCertificate& cert) {
            return cert.type() ==
                   OncParsedCertificates::ServerOrAuthorityCertificate::Type::
                       kAuthority;
          }));
}

net::CertificateList NetworkConfigurationUpdater::GetWebTrustedCertificates()
    const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!allow_trusted_certificates_from_policy_)
    return net::CertificateList();

  return GetFilteredCertificateListFromOnc(
      certs_->server_or_authority_certificates(),
      base::BindRepeating(
          [](const OncParsedCertificates::ServerOrAuthorityCertificate& cert) {
            return cert.web_trust_requested();
          }));
}

net::CertificateList
NetworkConfigurationUpdater::GetCertificatesWithoutWebTrust() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!allow_trusted_certificates_from_policy_)
    return GetAllServerAndAuthorityCertificates();

  return GetFilteredCertificateListFromOnc(
      certs_->server_or_authority_certificates(),
      base::BindRepeating(
          [](const OncParsedCertificates::ServerOrAuthorityCertificate& cert) {
            return !cert.web_trust_requested();
          }));
}

NetworkConfigurationUpdater::NetworkConfigurationUpdater(
    onc::ONCSource onc_source,
    std::string policy_key,
    bool allow_trusted_certs_from_policy,
    PolicyService* policy_service,
    chromeos::ManagedNetworkConfigurationHandler* network_config_handler)
    : onc_source_(onc_source),
      network_config_handler_(network_config_handler),
      policy_key_(policy_key),
      allow_trusted_certificates_from_policy_(allow_trusted_certs_from_policy),
      policy_change_registrar_(
          policy_service,
          PolicyNamespace(POLICY_DOMAIN_CHROME, std::string())),
      policy_service_(policy_service),
      certs_(std::make_unique<OncParsedCertificates>()) {}

void NetworkConfigurationUpdater::Init() {
  policy_change_registrar_.Observe(
      policy_key_,
      base::Bind(&NetworkConfigurationUpdater::OnPolicyChanged,
                 base::Unretained(this)));

  if (policy_service_->IsInitializationComplete(POLICY_DOMAIN_CHROME)) {
    VLOG(1) << LogHeader() << " is already initialized.";
    ApplyPolicy();
  } else {
    policy_service_->AddObserver(POLICY_DOMAIN_CHROME, this);
  }
}

void NetworkConfigurationUpdater::ParseCurrentPolicy(
    base::ListValue* network_configs,
    base::DictionaryValue* global_network_config,
    base::ListValue* certificates) {
  const PolicyMap& policies = policy_service_->GetPolicies(
      PolicyNamespace(POLICY_DOMAIN_CHROME, std::string()));
  const base::Value* policy_value = policies.GetValue(policy_key_);

  std::string onc_blob;
  if (!policy_value)
    VLOG(2) << LogHeader() << " is not set.";
  else if (!policy_value->GetAsString(&onc_blob))
    LOG(ERROR) << LogHeader() << " is not a string value.";

  chromeos::onc::ParseAndValidateOncForImport(
      onc_blob, onc_source_, std::string() /* no passphrase */, network_configs,
      global_network_config, certificates);
}

const std::vector<OncParsedCertificates::ClientCertificate>&
NetworkConfigurationUpdater::GetClientCertificates() const {
  return certs_->client_certificates();
}

void NetworkConfigurationUpdater::OnPolicyChanged(const base::Value* previous,
                                                  const base::Value* current) {
  ApplyPolicy();
}

void NetworkConfigurationUpdater::ApplyPolicy() {
  base::ListValue network_configs;
  base::DictionaryValue global_network_config;
  base::ListValue certificates;
  ParseCurrentPolicy(&network_configs, &global_network_config, &certificates);

  ImportCertificates(certificates);
  MarkFieldsAsRecommendedForBackwardsCompatibility(&network_configs);
  ApplyNetworkPolicy(&network_configs, &global_network_config);
}

void NetworkConfigurationUpdater::
    MarkFieldsAsRecommendedForBackwardsCompatibility(
        base::Value* network_configs_onc) {
  for (auto& network_config_onc : network_configs_onc->GetList()) {
    DCHECK(network_config_onc.is_dict());
    const std::string* type =
        network_config_onc.FindStringKey(::onc::network_config::kType);
    if (!type || *type != ::onc::network_type::kEthernet)
      continue;
    const base::Value* ethernet = network_config_onc.FindKeyOfType(
        ::onc::network_config::kEthernet, base::Value::Type::DICTIONARY);
    if (!ethernet)
      continue;
    const std::string* auth =
        ethernet->FindStringKey(::onc::ethernet::kAuthentication);
    if (!auth || *auth != ::onc::ethernet::kAuthenticationNone)
      continue;

    // If anything has been recommended, trust the server and don't change
    // anything.
    if (network_config_onc.FindKey(::onc::kRecommended))
      continue;
    base::Value* static_ip_config =
        network_config_onc.FindKey(::onc::network_config::kStaticIPConfig);
    if (static_ip_config && static_ip_config->FindKey(::onc::kRecommended))
      continue;

    // Ensure kStaticIPConfig exists because a "Recommended" field will be added
    // to it.
    if (!static_ip_config) {
      static_ip_config = network_config_onc.SetKey(
          ::onc::network_config::kStaticIPConfig, base::DictionaryValue());
    }
    SetRecommended(&network_config_onc,
                   {::onc::network_config::kIPAddressConfigType,
                    ::onc::network_config::kNameServersConfigType});
    SetRecommended(static_ip_config,
                   {::onc::ipconfig::kGateway, ::onc::ipconfig::kIPAddress,
                    ::onc::ipconfig::kRoutingPrefix, ::onc::ipconfig::kType,
                    ::onc::ipconfig::kNameServers});
  }
}

void NetworkConfigurationUpdater::SetRecommended(
    base::Value* onc_value,
    std::initializer_list<base::StringPiece> recommended_field_names) {
  DCHECK(onc_value);
  DCHECK(onc_value->is_dict());
  base::Value recommended_list(base::Value::Type::LIST);
  for (const auto& recommended_field_name : recommended_field_names) {
    recommended_list.GetList().push_back(base::Value(recommended_field_name));
  }
  onc_value->SetKey(::onc::kRecommended, std::move(recommended_list));
}

std::string NetworkConfigurationUpdater::LogHeader() const {
  return chromeos::onc::GetSourceAsString(onc_source_);
}

void NetworkConfigurationUpdater::ImportCertificates(
    const base::ListValue& certificates_onc) {
  std::unique_ptr<OncParsedCertificates> incoming_certs =
      std::make_unique<OncParsedCertificates>(certificates_onc);

  bool server_or_authority_certs_changed =
      certs_->server_or_authority_certificates() !=
      incoming_certs->server_or_authority_certificates();
  bool client_certs_changed =
      certs_->client_certificates() != incoming_certs->client_certificates();

  if (!server_or_authority_certs_changed && !client_certs_changed)
    return;

  certs_ = std::move(incoming_certs);

  if (client_certs_changed)
    ImportClientCertificates();

  if (server_or_authority_certs_changed)
    NotifyPolicyProvidedCertsChanged();
}

void NetworkConfigurationUpdater::NotifyPolicyProvidedCertsChanged() {
  net::CertificateList all_server_and_authority_certs =
      GetAllServerAndAuthorityCertificates();
  net::CertificateList trust_anchors = GetWebTrustedCertificates();

  for (auto& observer : observer_list_) {
    observer.OnPolicyProvidedCertsChanged(all_server_and_authority_certs,
                                          trust_anchors);
  }
}

}  // namespace policy
