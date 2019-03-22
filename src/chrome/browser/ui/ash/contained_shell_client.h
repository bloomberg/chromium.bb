// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CONTAINED_SHELL_CLIENT_H_
#define CHROME_BROWSER_UI_ASH_CONTAINED_SHELL_CLIENT_H_

#include "ash/public/interfaces/contained_shell.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"

class ContainedShellClient : public ash::mojom::ContainedShellClient {
 public:
  ContainedShellClient();
  ~ContainedShellClient() override;

 private:
  mojo::Binding<ash::mojom::ContainedShellClient> binding_{this};

  DISALLOW_COPY_AND_ASSIGN(ContainedShellClient);
};

#endif  // CHROME_BROWSER_UI_ASH_CONTAINED_SHELL_CLIENT_H_
