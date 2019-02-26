// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CONTAINED_SHELL_MOCK_CONTAINED_SHELL_CLIENT_H_
#define ASH_CONTAINED_SHELL_MOCK_CONTAINED_SHELL_CLIENT_H_

#include <memory>

#include "ash/public/interfaces/contained_shell.mojom.h"
#include "base/macros.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace ash {

class MockContainedShellClient : public mojom::ContainedShellClient {
 public:
  MockContainedShellClient();
  ~MockContainedShellClient() override;

  mojom::ContainedShellClientPtr CreateInterfacePtrAndBind();

 private:
  mojo::Binding<mojom::ContainedShellClient> binding_{this};

  DISALLOW_COPY_AND_ASSIGN(MockContainedShellClient);
};

// Helper function to bind a ContainedShellClient so that it receives mojo
// calls.
std::unique_ptr<MockContainedShellClient> BindMockContainedShellClient();

}  // namespace ash

#endif  // ASH_CONTAINED_SHELL_MOCK_CONTAINED_SHELL_CLIENT_H
