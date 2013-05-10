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
    std::string client_id;
    std::string client_name;
    std::string shared_secret;
  };

  // Mapping from client id to pairing information.
  typedef std::map<std::string, Pairing> PairedClients;

  // Interface representing the persistent storage back-end.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Save pairing information to persistent storage. Must not block.
    virtual void Save(const PairedClients& paired_clients) = 0;
  };

  explicit PairingRegistry(scoped_ptr<Delegate> delegate,
                           const PairedClients& paired_clients);

  // Create a pairing for a new client and save it to disk.
  const Pairing& CreatePairing(const std::string& client_name);

  // Look up the shared secret for the specified client id. Returns an empty
  // string if the client id is not known.
  std::string GetSecret(const std::string &client_id) const;

 private:
  friend class base::RefCountedThreadSafe<PairingRegistry>;

  virtual ~PairingRegistry();

  // Callback for the Delegate::Load method. Invoked when the stored pairings
  // have been loaded.
  void OnLoad(const PairedClients& paired_clients);

  scoped_ptr<Delegate> delegate_;
  PairedClients paired_clients_;

  DISALLOW_COPY_AND_ASSIGN(PairingRegistry);
};

// Temporary delegate that just logs NOTIMPLEMENTED for Load/Save.
// TODO(jamiewalch): Delete once Delegates are implemented for all platforms.
class NotImplementedPairingRegistryDelegate : public PairingRegistry::Delegate {
 public:
  virtual void Save(
      const PairingRegistry::PairedClients& paired_clients) OVERRIDE;
};

}  // namespace protocol
}  // namespace remoting

#endif  // REMOTING_PROTOCOL_PAIRING_REGISTRY_H_
