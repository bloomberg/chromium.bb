// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_KIOSK_NEXT_SHELL_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_KIOSK_NEXT_SHELL_CLIENT_H_

#include "ash/public/interfaces/kiosk_next_shell.mojom.h"
#include "base/macros.h"
#include "components/account_id/account_id.h"
#include "mojo/public/cpp/bindings/binding.h"

class KioskNextShellClient : public ash::mojom::KioskNextShellClient {
 public:
  KioskNextShellClient();
  ~KioskNextShellClient() override;

  // Returns the singleton KioskNextShellClient instance, if it exists.
  static KioskNextShellClient* Get();

  // mojom::KioskNextShellClient:
  void LaunchKioskNextShell(const AccountId& account_id) override;

  bool has_launched() const { return has_launched_; }

 private:
  mojo::Binding<ash::mojom::KioskNextShellClient> binding_{this};

  // True once the KioskNextShell has been launched.
  bool has_launched_ = false;

  DISALLOW_COPY_AND_ASSIGN(KioskNextShellClient);
};

#endif  // CHROME_BROWSER_UI_ASH_KIOSK_NEXT_SHELL_CLIENT_H_
