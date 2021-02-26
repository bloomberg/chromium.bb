// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/sharing/sharing_impl.h"

#include <utility>

#include "base/callback.h"
#include "chrome/services/sharing/nearby/decoder/nearby_decoder.h"
#include "chrome/services/sharing/nearby/nearby_connections.h"
#include "chromeos/services/nearby/public/mojom/nearby_decoder.mojom.h"

namespace sharing {

SharingImpl::SharingImpl(
    mojo::PendingReceiver<mojom::Sharing> receiver,
    scoped_refptr<base::SequencedTaskRunner> io_task_runner)
    : receiver_(this, std::move(receiver)),
      io_task_runner_(std::move(io_task_runner)) {}

SharingImpl::~SharingImpl() = default;

void SharingImpl::Connect(
    NearbyConnectionsDependenciesPtr deps,
    mojo::PendingReceiver<NearbyConnectionsMojom> connections_receiver,
    mojo::PendingReceiver<sharing::mojom::NearbySharingDecoder>
        decoder_receiver) {
  DCHECK(!nearby_connections_);
  DCHECK(!nearby_decoder_);

  nearby_connections_ = std::make_unique<NearbyConnections>(
      std::move(connections_receiver), std::move(deps), io_task_runner_,
      base::BindOnce(&SharingImpl::NearbyConnectionsDisconnected,
                     weak_ptr_factory_.GetWeakPtr()));
  nearby_decoder_ =
      std::make_unique<NearbySharingDecoder>(std::move(decoder_receiver));
}

void SharingImpl::ShutDown(ShutDownCallback callback) {
  nearby_connections_.reset();
  nearby_decoder_.reset();
  std::move(callback).Run();
}

void SharingImpl::NearbyConnectionsDisconnected() {
  nearby_connections_.reset();
}

}  // namespace sharing
