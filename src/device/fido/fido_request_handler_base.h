// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DEVICE_FIDO_FIDO_REQUEST_HANDLER_BASE_H_
#define DEVICE_FIDO_FIDO_REQUEST_HANDLER_BASE_H_

#include <array>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/component_export.h"
#include "base/containers/flat_set.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/strings/string_piece_forward.h"
#include "build/build_config.h"
#include "device/fido/fido_discovery_base.h"
#include "device/fido/fido_transport_protocol.h"

namespace base {
class ElapsedTimer;
}

namespace device {

class BleAdapterManager;
class FidoAuthenticator;
class FidoDiscoveryFactory;

// Base class that handles authenticator discovery/removal. Its lifetime is
// equivalent to that of a single WebAuthn request. For each authenticator, the
// per-device work is carried out by one FidoAuthenticator instance, which is
// constructed in a FidoDiscoveryBase and passed to the request handler via its
// Observer interface.
class COMPONENT_EXPORT(DEVICE_FIDO) FidoRequestHandlerBase
    : public FidoDiscoveryBase::Observer {
 public:
  using RequestCallback = base::RepeatingCallback<void(const std::string&)>;
  using BlePairingCallback =
      base::RepeatingCallback<void(std::string authenticator_id,
                                   base::Optional<std::string> pin_code,
                                   base::OnceClosure success_callback,
                                   base::OnceClosure error_callback)>;

  enum class RequestType { kMakeCredential, kGetAssertion };

  struct AuthenticatorState {
    explicit AuthenticatorState(FidoAuthenticator* authenticator);
    ~AuthenticatorState();

    FidoAuthenticator* authenticator;
    std::unique_ptr<base::ElapsedTimer> timer;
  };

  using AuthenticatorMap =
      std::map<std::string, std::unique_ptr<AuthenticatorState>, std::less<>>;

  // Encapsulates data required to initiate WebAuthN UX dialog. Once all
  // components of TransportAvailabilityInfo is set,
  // AuthenticatorRequestClientDelegate should be notified.
  // TODO(hongjunchoi): Add async calls to notify embedder when Bluetooth is
  // powered on/off.
  struct COMPONENT_EXPORT(DEVICE_FIDO) TransportAvailabilityInfo {
    TransportAvailabilityInfo();
    TransportAvailabilityInfo(const TransportAvailabilityInfo& other);
    TransportAvailabilityInfo& operator=(
        const TransportAvailabilityInfo& other);
    ~TransportAvailabilityInfo();

    // TODO(hongjunchoi): Factor |request_type| from TransportAvailabilityInfo.
    // See: https://crbug.com/875011
    RequestType request_type = RequestType::kMakeCredential;

    // Indicates whether this is a GetAssertion request with an empty allow
    // list.
    bool has_empty_allow_list = false;

    // The intersection of transports supported by the client and allowed by the
    // relying party.
    base::flat_set<FidoTransportProtocol> available_transports;

    bool has_recognized_mac_touch_id_credential = false;
    bool is_ble_powered = false;
    bool can_power_on_ble_adapter = false;

    // Indicates whether the native Windows WebAuthn API is available.
    // Dispatching to it should be controlled by the embedder.
    //
    // The embedder:
    //  - may choose not to dispatch immediately if caBLE is available
    //  - should dispatch immediately if no other transport is available
    bool has_win_native_api_authenticator = false;

    // Indicates whether the Windows native UI will include a privacy notice
    // when creating a resident credential.
    bool win_native_ui_shows_resident_credential_notice = false;

    // Contains the authenticator ID of the native Windows
    // authenticator if |has_win_native_api_authenticator| is true.
    // This allows the observer to distinguish it from other
    // authenticators.
    std::string win_native_api_authenticator_id;
  };

  class COMPONENT_EXPORT(DEVICE_FIDO) Observer {
   public:
    virtual ~Observer();

    // This method will not be invoked until the observer is set.
    virtual void OnTransportAvailabilityEnumerated(
        TransportAvailabilityInfo data) = 0;

    // If true, the request handler will defer dispatch of its request onto the
    // given authenticator to the embedder. The embedder needs to call
    // |StartAuthenticatorRequest| when it wants to initiate request dispatch.
    //
    // This method is invoked before |FidoAuthenticatorAdded|, and may be
    // invoked multiple times for the same authenticator. Depending on the
    // result, the request handler might decide not to make the authenticator
    // available, in which case it never gets passed to
    // |FidoAuthenticatorAdded|.
    virtual bool EmbedderControlsAuthenticatorDispatch(
        const FidoAuthenticator& authenticator) = 0;

    virtual void BluetoothAdapterPowerChanged(bool is_powered_on) = 0;
    virtual void FidoAuthenticatorAdded(
        const FidoAuthenticator& authenticator) = 0;
    virtual void FidoAuthenticatorRemoved(base::StringPiece device_id) = 0;

    // SupportsPIN returns true if this observer supports collecting a PIN from
    // the user. If this function returns false, |CollectPIN| and
    // |FinishCollectPIN| will not be called.
    virtual bool SupportsPIN() const = 0;

    // CollectPIN is called when a PIN is needed to complete a request. The
    // |attempts| parameter is either |nullopt| to indicate that the user needs
    // to set a PIN, or contains the number of PIN attempts remaining before a
    // hard lock.
    virtual void CollectPIN(
        base::Optional<int> attempts,
        base::OnceCallback<void(std::string)> provide_pin_cb) = 0;

    virtual void FinishCollectToken() = 0;

    // Called when a biometric enrollment may be completed as part of the
    // request and the user should be notified to collect samples.
    // |next_callback| must be executed asynchronously at any time to move on to
    // the next step of the request.
    virtual void StartBioEnrollment(base::OnceClosure next_callback) = 0;

    // Called when a biometric enrollment sample has been collected.
    // |bio_samples_remaining| is the number of samples needed to finish the
    // enrollment.
    virtual void OnSampleCollected(int bio_samples_remaining) = 0;

    // Called when an authenticator reports internal user verification has
    // failed (e.g. not recognising the user's fingerprints) and the user should
    // try again. Receives the number of |attempts| before the device locks
    // internal user verification.
    virtual void OnRetryUserVerification(int attempts) = 0;

    // Called when an authenticator reports internal user verification has been
    // locked (e.g. by repeated failed attempts to recognise the user's
    // fingerprints).
    virtual void OnInternalUserVerificationLocked() = 0;

    // SetMightCreateResidentCredential indicates whether the activation of an
    // authenticator may cause a resident credential to be created. A resident
    // credential may be discovered by someone with physical access to the
    // authenticator and thus has privacy implications. Initially, this is
    // assumed to be false.
    virtual void SetMightCreateResidentCredential(bool v) = 0;
  };

  // The |available_transports| should be the intersection of transports
  // supported by the client and allowed by the relying party.
  FidoRequestHandlerBase(
      FidoDiscoveryFactory* fido_discovery_factory,
      const base::flat_set<FidoTransportProtocol>& available_transports);
  ~FidoRequestHandlerBase() override;

  // Triggers DispatchRequest() if |active_authenticators_| hold
  // FidoAuthenticator with given |authenticator_id|.
  void StartAuthenticatorRequest(const std::string& authenticator_id);

  // Invokes |FidoAuthenticator::Cancel| on all authenticators, except if
  // matching |exclude_id|, if one is provided. Cancelled authenticators are
  // immediately removed from |active_authenticators_|.
  //
  // This function is invoked either when: (a) the entire WebAuthn API request
  // is canceled or, (b) a successful response or "invalid state error" is
  // received from the any one of the connected authenticators, in which case
  // all other authenticators are cancelled.
  // https://w3c.github.io/webauthn/#iface-pkcredential
  void CancelActiveAuthenticators(base::StringPiece exclude_id = "");
  virtual void OnBluetoothAdapterEnumerated(bool is_present,
                                            bool is_powered_on,
                                            bool can_power_on,
                                            bool is_peripheral_role_supported);
  void OnBluetoothAdapterPowerChanged(bool is_powered_on);
  void PowerOnBluetoothAdapter();

  base::WeakPtr<FidoRequestHandlerBase> GetWeakPtr();

  void set_observer(Observer* observer) {
    DCHECK(!observer_) << "Only one observer is supported.";
    observer_ = observer;

    DCHECK(notify_observer_callback_);
    notify_observer_callback_.Run();
  }

  // Returns whether FidoAuthenticator with id equal to |authenticator_id|
  // exists. Fake FidoRequestHandler objects used in testing overrides this
  // function to simulate scenarios where authenticator with |authenticator_id|
  // is known to the system.
  virtual bool HasAuthenticator(const std::string& authentiator_id) const;

  TransportAvailabilityInfo& transport_availability_info() {
    return transport_availability_info_;
  }

  const AuthenticatorMap& AuthenticatorsForTesting() {
    return active_authenticators_;
  }

  std::unique_ptr<BleAdapterManager>&
  get_bluetooth_adapter_manager_for_testing() {
    return bluetooth_adapter_manager_;
  }

  void StopDiscoveries();

 protected:
  // Subclasses implement this method to dispatch their request onto the given
  // FidoAuthenticator. The FidoAuthenticator is owned by this
  // FidoRequestHandler and stored in active_authenticators().
  virtual void DispatchRequest(FidoAuthenticator*) = 0;

  void Start();

  // Returns |true| if a short enough time has elapsed since the request was
  // dispatched that an authenticator may be suspected to have returned a
  // response without user interaction.
  // Must be called after |DispatchRequest| is called.
  bool AuthenticatorMayHaveReturnedImmediately(
      const std::string& authenticator_id);

  AuthenticatorMap& active_authenticators() { return active_authenticators_; }
  std::vector<std::unique_ptr<FidoDiscoveryBase>>& discoveries() {
    return discoveries_;
  }
  Observer* observer() const { return observer_; }

  // FidoDiscoveryBase::Observer
  void DiscoveryStarted(
      FidoDiscoveryBase* discovery,
      bool success,
      std::vector<FidoAuthenticator*> authenticators) override;
  void AuthenticatorAdded(FidoDiscoveryBase* discovery,
                          FidoAuthenticator* authenticator) override;
  void AuthenticatorRemoved(FidoDiscoveryBase* discovery,
                            FidoAuthenticator* authenticator) override;

 private:
  friend class FidoRequestHandlerTest;

  void InitDiscoveries(
      FidoDiscoveryFactory* fido_discovery_factory,
      const base::flat_set<FidoTransportProtocol>& available_transports);
#if defined(OS_WIN)
  void InitDiscoveriesWin(
      FidoDiscoveryFactory* fido_discovery_factory,
      const base::flat_set<FidoTransportProtocol>& available_transports);
#endif

  void NotifyObserverTransportAvailability();

  // Invokes FidoAuthenticator::InitializeAuthenticator(), followed by
  // DispatchRequest(). InitializeAuthenticator() sends a GetInfo command
  // to FidoDeviceAuthenticator instances in order to determine their protocol
  // versions before a request can be dispatched.
  void InitializeAuthenticatorAndDispatchRequest(
      const std::string& authenticator_id);
  void ConstructBleAdapterPowerManager();

  AuthenticatorMap active_authenticators_;
  std::vector<std::unique_ptr<FidoDiscoveryBase>> discoveries_;
  Observer* observer_ = nullptr;
  TransportAvailabilityInfo transport_availability_info_;
  base::RepeatingClosure notify_observer_callback_;
  std::unique_ptr<BleAdapterManager> bluetooth_adapter_manager_;

  base::WeakPtrFactory<FidoRequestHandlerBase> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(FidoRequestHandlerBase);
};

}  // namespace device

#endif  // DEVICE_FIDO_FIDO_REQUEST_HANDLER_BASE_H_
