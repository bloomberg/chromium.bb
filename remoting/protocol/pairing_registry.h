// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PAIRING_REGISTRY_H_
#define REMOTING_PROTOCOL_PAIRING_REGISTRY_H_

#include <map>
#include <queue>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/threading/non_thread_safe.h"
#include "base/time/time.h"

namespace base {
class ListValue;
}  // namespace base

namespace remoting {
namespace protocol {

// PairingRegistry holds information about paired clients to support
// PIN-less authentication. For each paired client, the registry holds
// the following information:
//   * The name of the client. This is supplied by the client and is not
//     guaranteed to be unique.
//   * The unique id of the client. This is generated on-demand by this
//     class and sent in plain-text by the client during authentication.
//   * The shared secret for the client. This is generated on-demand by this
//     class and used in the SPAKE2 exchange to mutually verify identity.
class PairingRegistry : public base::RefCountedThreadSafe<PairingRegistry>,
                        public base::NonThreadSafe {
 public:
  struct Pairing {
    Pairing();
    Pairing(const base::Time& created_time,
            const std::string& client_name,
            const std::string& client_id,
            const std::string& shared_secret);
    ~Pairing();

    static Pairing Create(const std::string& client_name);

    bool operator==(const Pairing& other) const;

    bool is_valid() const;

    base::Time created_time() const { return created_time_; }
    std::string client_id() const { return client_id_; }
    std::string client_name() const { return client_name_; }
    std::string shared_secret() const { return shared_secret_; }

   private:
    base::Time created_time_;
    std::string client_name_;
    std::string client_id_;
    std::string shared_secret_;
  };

  // Mapping from client id to pairing information.
  typedef std::map<std::string, Pairing> PairedClients;

  // Delegate callbacks.
  typedef base::Callback<void(const std::string& pairings_json)> LoadCallback;
  typedef base::Callback<void(bool success)> SaveCallback;
  typedef base::Callback<void(Pairing pairing)> GetPairingCallback;
  typedef base::Callback<void(scoped_ptr<base::ListValue> pairings)>
      GetAllPairingsCallback;

  static const char kCreatedTimeKey[];
  static const char kClientIdKey[];
  static const char kClientNameKey[];
  static const char kSharedSecretKey[];

  // Interface representing the persistent storage back-end.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Save JSON-encoded pairing information to persistent storage. If
    // a non-NULL callback is provided, invoke it on completion to
    // indicate success or failure. Must not block.
    virtual void Save(const std::string& pairings_json,
                      const SaveCallback& callback) = 0;

    // Retrieve the JSON-encoded pairing information from persistent
    // storage. Must not block.
    virtual void Load(const LoadCallback& callback) = 0;
  };

  explicit PairingRegistry(scoped_ptr<Delegate> delegate);

  // Creates a pairing for a new client and saves it to disk.
  //
  // TODO(jamiewalch): Plumb the Save callback into the RequestPairing flow
  // so that the client isn't sent the pairing information until it has been
  // saved.
  Pairing CreatePairing(const std::string& client_name);

  // Gets the pairing for the specified client id. See the corresponding
  // Delegate method for details. If none is found, the callback is invoked
  // with an invalid Pairing.
  void GetPairing(const std::string& client_id,
                  const GetPairingCallback& callback);

  // Gets all pairings with the shared secrets removed as a base::ListValue.
  void GetAllPairings(const GetAllPairingsCallback& callback);

  // Delete a pairing, identified by its client ID. |callback| is called with
  // the result of saving the new config, which occurs even if the client ID
  // did not match any pairing.
  void DeletePairing(const std::string& client_id,
                     const SaveCallback& callback);

  // Clear all pairings from the registry.
  void ClearAllPairings(const SaveCallback& callback);

 private:
  FRIEND_TEST_ALL_PREFIXES(PairingRegistryTest, AddPairing);
  FRIEND_TEST_ALL_PREFIXES(PairingRegistryTest, GetAllPairingsJSON);
  friend class NegotiatingAuthenticatorTest;
  friend class base::RefCountedThreadSafe<PairingRegistry>;

  virtual ~PairingRegistry();

  // Helper method for unit tests.
  void AddPairing(const Pairing& pairing);

  // Worker functions for each of the public methods, passed as a callback to
  // the delegate.
  void MergePairingAndSave(const Pairing& pairing,
                           const SaveCallback& callback,
                           const std::string& pairings_json);
  void DoGetPairing(const std::string& client_id,
                    const GetPairingCallback& callback,
                    const std::string& pairings_json);
  void SanitizePairings(const GetAllPairingsCallback& callback,
                        const std::string& pairings_json);
  void DoDeletePairing(const std::string& client_id,
                       const SaveCallback& callback,
                       const std::string& pairings_json);

  // "Trampoline" callbacks that schedule the next pending request and then
  // invoke the original caller-supplied callback.
  void InvokeLoadCallbackAndScheduleNext(
      const LoadCallback& callback, const std::string& pairings_json);
  void InvokeSaveCallbackAndScheduleNext(
      const SaveCallback& callback, bool success);
  void InvokeGetPairingCallbackAndScheduleNext(
      const GetPairingCallback& callback, Pairing pairing);
  void InvokeGetAllPairingsCallbackAndScheduleNext(
      const GetAllPairingsCallback& callback,
      scoped_ptr<base::ListValue> pairings);

  // Queue management methods.
  void ServiceOrQueueRequest(const base::Closure& request);
  void ServiceNextRequest();

  // Translate between the structured and serialized forms of the pairing data.
  static PairedClients DecodeJson(const std::string& pairings_json);
  static std::string EncodeJson(const PairedClients& clients);
  static scoped_ptr<base::ListValue> ConvertToListValue(
      const PairedClients& clients,
      bool include_shared_secrets);

  scoped_ptr<Delegate> delegate_;

  std::queue<base::Closure> pending_requests_;

  DISALLOW_COPY_AND_ASSIGN(PairingRegistry);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PAIRING_REGISTRY_H_
