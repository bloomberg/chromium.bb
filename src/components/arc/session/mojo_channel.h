// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_SESSION_MOJO_CHANNEL_H_
#define COMPONENTS_ARC_SESSION_MOJO_CHANNEL_H_

#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "components/arc/session/connection_holder.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace arc {

// Thin interface to wrap Remote<T> with type erasure.
class MojoChannelBase {
 public:
  virtual ~MojoChannelBase() = default;

 protected:
  MojoChannelBase() = default;

 private:
  DISALLOW_COPY_AND_ASSIGN(MojoChannelBase);
};

// Thin wrapper for Remote<T>, where T is one of ARC mojo Instance class.
template <typename InstanceType, typename HostType>
class MojoChannel : public MojoChannelBase {
 public:
  MojoChannel(ConnectionHolder<InstanceType, HostType>* holder,
              mojo::PendingRemote<InstanceType> remote)
      : holder_(holder), remote_(std::move(remote)) {
    // Delay registration to the ConnectionHolder until the version is ready.
  }

  ~MojoChannel() override { holder_->CloseInstance(remote_.get()); }

  void set_disconnect_handler(base::OnceClosure error_handler) {
    remote_.set_disconnect_handler(std::move(error_handler));
  }

  void QueryVersion() {
    // Note: the callback will not be called if |remote_| is destroyed.
    remote_.QueryVersion(
        base::BindOnce(&MojoChannel::OnVersionReady, base::Unretained(this)));
  }

 private:
  void OnVersionReady(uint32_t unused_version) {
    holder_->SetInstance(remote_.get(), remote_.version());
  }

  // Externally owned ConnectionHolder instance.
  ConnectionHolder<InstanceType, HostType>* const holder_;

  // Put as a last member to ensure that any callback tied to the |remote_|
  // is not invoked.
  mojo::Remote<InstanceType> remote_;

  DISALLOW_COPY_AND_ASSIGN(MojoChannel);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_SESSION_MOJO_CHANNEL_H_
