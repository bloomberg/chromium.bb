// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef REMOTING_PROTOCOL_PAIRING_REGISTRY_H_
#define REMOTING_PROTOCOL_PAIRING_REGISTRY_H_

#include <map>
#include <string>

#include "base/callback.h"
#include "base/gtest_prod_util.h"
#include "base/memory/ref_counted.h"
#include "base/threading/non_thread_safe.h"
#include "base/time.h"

namespace remoting {
namespace protocol {

// TODO(jamiewalch): This class is little more than a wrapper around the
// Pairing and Delegate classes. Refactor it away.

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
  typedef base::Callback<void(Pairing client_information)> GetPairingCallback;
  typedef base::Callback<void(bool success)> AddPairingCallback;

  // Interface representing the persistent storage back-end.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Add pairing information to persistent storage. If a non-NULL callback
    // is provided, invoke it on completion to indicate success or failure.
    // Must not block.
    //
    // TODO(jamiewalch): Plumb the callback into the RequestPairing flow so
    // that the client isn't sent the pairing information until it has been
    // saved.
    virtual void AddPairing(const Pairing& new_paired_client,
                            const AddPairingCallback& callback) = 0;

    // Retrieve the Pairing for the specified client id. If none is found,
    // invoke the callback with a default pairing. Must not block.
    virtual void GetPairing(const std::string& client_id,
                            const GetPairingCallback& callback) = 0;
  };

  explicit PairingRegistry(scoped_ptr<Delegate> delegate);

  // Create a pairing for a new client and save it to disk.
  Pairing CreatePairing(const std::string& client_name);

  // Get the pairing for the specified client id. See the corresponding
  // Delegate method for details.
  void GetPairing(const std::string& client_id,
                  const GetPairingCallback& callback);

 private:
  friend class base::RefCountedThreadSafe<PairingRegistry>;

  virtual ~PairingRegistry();

  scoped_ptr<Delegate> delegate_;

  DISALLOW_COPY_AND_ASSIGN(PairingRegistry);
};

// Temporary delegate that just logs NOTIMPLEMENTED for Load/Save.
// TODO(jamiewalch): Delete once Delegates are implemented for all platforms.
class NotImplementedPairingRegistryDelegate : public PairingRegistry::Delegate {
 public:
  virtual void AddPairing(
      const PairingRegistry::Pairing& paired_clients,
      const PairingRegistry::AddPairingCallback& callback) OVERRIDE;
  virtual void GetPairing(
      const std::string& client_id,
      const PairingRegistry::GetPairingCallback& callback) OVERRIDE;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PAIRING_REGISTRY_H_
