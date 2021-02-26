// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_TEST_FAKE_WAKE_LOCK_INSTANCE_H_
#define COMPONENTS_ARC_TEST_FAKE_WAKE_LOCK_INSTANCE_H_

#include "base/macros.h"
#include "components/arc/mojom/wake_lock.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

namespace arc {

class FakeWakeLockInstance : public mojom::WakeLockInstance {
 public:
  FakeWakeLockInstance();
  ~FakeWakeLockInstance() override;

  // mojom::WakeLockInstance overrides:
  void Init(mojo::PendingRemote<mojom::WakeLockHost> host_remote,
            InitCallback callback) override;

 private:
  mojo::Remote<mojom::WakeLockHost> host_remote_;

  DISALLOW_COPY_AND_ASSIGN(FakeWakeLockInstance);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_TEST_FAKE_WAKE_LOCK_INSTANCE_H_
