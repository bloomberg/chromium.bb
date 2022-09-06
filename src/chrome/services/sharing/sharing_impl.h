// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICES_SHARING_SHARING_IMPL_H_
#define CHROME_SERVICES_SHARING_SHARING_IMPL_H_

#include <map>
#include <memory>
#include <vector>

#include "ash/services/nearby/public/mojom/nearby_connections.mojom-forward.h"
#include "ash/services/nearby/public/mojom/sharing.mojom.h"
#include "ash/services/nearby/public/mojom/webrtc.mojom-forward.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/task/sequenced_task_runner.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "services/network/public/mojom/mdns_responder.mojom-forward.h"
#include "services/network/public/mojom/p2p.mojom-forward.h"

namespace location {
namespace nearby {
namespace connections {
class NearbyConnections;
}  // namespace connections
}  // namespace nearby
}  // namespace location

namespace sharing {

class NearbySharingDecoder;

class SharingImpl : public mojom::Sharing {
 public:
  using NearbyConnectionsMojom =
      location::nearby::connections::mojom::NearbyConnections;
  using NearbyConnections = location::nearby::connections::NearbyConnections;
  using NearbyConnectionsDependenciesPtr =
      location::nearby::connections::mojom::NearbyConnectionsDependenciesPtr;

  SharingImpl(mojo::PendingReceiver<mojom::Sharing> receiver,
              scoped_refptr<base::SequencedTaskRunner> io_task_runner);
  SharingImpl(const SharingImpl&) = delete;
  SharingImpl& operator=(const SharingImpl&) = delete;
  ~SharingImpl() override;

  // mojom::Sharing:
  void Connect(
      NearbyConnectionsDependenciesPtr deps,
      mojo::PendingReceiver<NearbyConnectionsMojom> connections_receiver,
      mojo::PendingReceiver<sharing::mojom::NearbySharingDecoder>
          decoder_receiver) override;
  void ShutDown(ShutDownCallback callback) override;

 private:
  friend class SharingImplTest;

  void DoShutDown(bool is_expected);
  void NearbyConnectionsDisconnected();

  mojo::Receiver<mojom::Sharing> receiver_;
  const scoped_refptr<base::SequencedTaskRunner> io_task_runner_;

  std::unique_ptr<NearbyConnections> nearby_connections_;

  std::unique_ptr<NearbySharingDecoder> nearby_decoder_;

  base::WeakPtrFactory<SharingImpl> weak_ptr_factory_{this};
};

}  // namespace sharing

#endif  // CHROME_SERVICES_SHARING_SHARING_IMPL_H_
