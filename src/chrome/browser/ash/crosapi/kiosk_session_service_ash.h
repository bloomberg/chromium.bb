// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_CROSAPI_KIOSK_SESSION_SERVICE_ASH_H_
#define CHROME_BROWSER_ASH_CROSAPI_KIOSK_SESSION_SERVICE_ASH_H_

#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "chromeos/crosapi/mojom/kiosk_session_service.mojom.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver_set.h"
#include "mojo/public/cpp/bindings/remote_set.h"

namespace crosapi {

// Implements the crosapi interface for web Kiosk session.
class KioskSessionServiceAsh : public mojom::KioskSessionService {
 public:
  KioskSessionServiceAsh();
  KioskSessionServiceAsh(const KioskSessionServiceAsh&) = delete;
  KioskSessionServiceAsh& operator=(const KioskSessionServiceAsh&) = delete;
  ~KioskSessionServiceAsh() override;

  void BindReceiver(mojo::PendingReceiver<mojom::KioskSessionService> receiver);

  // crosapi::mojom::KioskSessionService:
  void AttemptUserExit() override;

 private:
  // Any number of crosapi clients can connect to this class.
  mojo::ReceiverSet<mojom::KioskSessionService> receivers_;
};

}  // namespace crosapi

#endif  // CHROME_BROWSER_ASH_CROSAPI_KIOSK_SESSION_SERVICE_ASH_H_
