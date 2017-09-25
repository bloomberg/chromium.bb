// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PAIRING_REGISTRY_H_
#define REMOTING_PROTOCOL_PAIRING_REGISTRY_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/containers/queue.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/time/time.h"

namespace base {
class DictionaryValue;
class ListValue;
class Location;
class SingleThreadTaskRunner;
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
class PairingRegistry : public base::RefCountedThreadSafe<PairingRegistry> {
 public:
  struct Pairing {
    Pairing();
    Pairing(const base::Time& created_time,
            const std::string& client_name,
            const std::string& client_id,
            const std::string& shared_secret);
    Pairing(const Pairing& other);
    ~Pairing();

    static Pairing Create(const std::string& client_name);
    static Pairing CreateFromValue(const base::DictionaryValue& pairing);

    std::unique_ptr<base::DictionaryValue> ToValue() const;

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
  typedef base::Callback<void(bool success)> DoneCallback;
  typedef base::Callback<void(std::unique_ptr<base::ListValue> pairings)>
      GetAllPairingsCallback;
  typedef base::Callback<void(Pairing pairing)> GetPairingCallback;

  static const char kCreatedTimeKey[];
  static const char kClientIdKey[];
  static const char kClientNameKey[];
  static const char kSharedSecretKey[];

  // Interface representing the persistent storage back-end.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Retrieves all JSON-encoded pairings from persistent storage.
    virtual std::unique_ptr<base::ListValue> LoadAll() = 0;

    // Deletes all pairings in persistent storage.
    virtual bool DeleteAll() = 0;

    // Retrieves the pairing identified by |client_id|.
    virtual Pairing Load(const std::string& client_id) = 0;

    // Saves |pairing| to persistent storage.
    virtual bool Save(const Pairing& pairing) = 0;

    // Deletes the pairing identified by |client_id|.
    virtual bool Delete(const std::string& client_id) = 0;
  };

  PairingRegistry(
      scoped_refptr<base::SingleThreadTaskRunner> delegate_task_runner,
      std::unique_ptr<Delegate> delegate);

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
                     const DoneCallback& callback);

  // Clear all pairings from the registry.
  void ClearAllPairings(const DoneCallback& callback);

 protected:
  friend class base::RefCountedThreadSafe<PairingRegistry>;
  virtual ~PairingRegistry();

  // Lets the tests override task posting to make all callbacks synchronous.
  virtual void PostTask(
      const scoped_refptr<base::SingleThreadTaskRunner>& task_runner,
      const base::Location& from_here,
      const base::Closure& task);

 private:
  FRIEND_TEST_ALL_PREFIXES(PairingRegistryTest, AddPairing);
  friend class NegotiatingAuthenticatorTest;

  // Helper method for unit tests.
  void AddPairing(const Pairing& pairing);

  // Blocking helper methods used to call the delegate.
  void DoLoadAll(
      const protocol::PairingRegistry::GetAllPairingsCallback& callback);
  void DoDeleteAll(
      const protocol::PairingRegistry::DoneCallback& callback);
  void DoLoad(
      const std::string& client_id,
      const protocol::PairingRegistry::GetPairingCallback& callback);
  void DoSave(
      const protocol::PairingRegistry::Pairing& pairing,
      const protocol::PairingRegistry::DoneCallback& callback);
  void DoDelete(
      const std::string& client_id,
      const protocol::PairingRegistry::DoneCallback& callback);

  // "Trampoline" callbacks that schedule the next pending request and then
  // invoke the original caller-supplied callback.
  void InvokeDoneCallbackAndScheduleNext(
      const DoneCallback& callback, bool success);
  void InvokeGetPairingCallbackAndScheduleNext(
      const GetPairingCallback& callback, Pairing pairing);
  void InvokeGetAllPairingsCallbackAndScheduleNext(
      const GetAllPairingsCallback& callback,
      std::unique_ptr<base::ListValue> pairings);

  // Sanitize |pairings| by parsing each entry and removing the secret from it.
  void SanitizePairings(const GetAllPairingsCallback& callback,
                        std::unique_ptr<base::ListValue> pairings);

  // Queue management methods.
  void ServiceOrQueueRequest(const base::Closure& request);
  void ServiceNextRequest();

  // Task runner on which all public methods of this class should be called.
  scoped_refptr<base::SingleThreadTaskRunner> caller_task_runner_;

  // Task runner used to run blocking calls to the delegate. A single thread
  // task runner is used to guarantee that one one method of the delegate is
  // called at a time.
  scoped_refptr<base::SingleThreadTaskRunner> delegate_task_runner_;

  std::unique_ptr<Delegate> delegate_;

  base::queue<base::Closure> pending_requests_;

  DISALLOW_COPY_AND_ASSIGN(PairingRegistry);
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PAIRING_REGISTRY_H_
