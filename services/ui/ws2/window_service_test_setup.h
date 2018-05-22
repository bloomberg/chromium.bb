// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS2_WINDOW_SERVICE_TEST_SETUP_H_
#define SERVICES_UI_WS2_WINDOW_SERVICE_TEST_SETUP_H_

#include <memory>

#include "base/macros.h"
#include "base/test/scoped_task_environment.h"
#include "services/ui/ws2/test_window_service_delegate.h"
#include "services/ui/ws2/test_window_tree_client.h"
#include "services/ui/ws2/window_service_client_test_helper.h"
#include "ui/aura/test/aura_test_helper.h"
#include "ui/compositor/test/context_factories_for_test.h"

namespace wm {
class ScopedCaptureClient;
}

namespace ui {
namespace ws2 {

class WindowService;
class WindowServiceClient;
class WindowServiceClientTestHelper;

struct Embedding;

// Helper to setup state needed for WindowService tests.
class WindowServiceTestSetup {
 public:
  WindowServiceTestSetup();
  ~WindowServiceTestSetup();

  // |flags| mirrors that from mojom::WindowTree::Embed(), see it for details.
  std::unique_ptr<Embedding> CreateEmbedding(aura::Window* embed_root,
                                             uint32_t flags = 0);

  aura::Window* root() { return aura_test_helper_.root_window(); }
  TestWindowServiceDelegate* delegate() { return &delegate_; }
  TestWindowTreeClient* window_tree_client() { return &window_tree_client_; }
  WindowServiceClientTestHelper* client_test_helper() {
    return client_test_helper_.get();
  }

  std::vector<Change>* changes() {
    return window_tree_client_.tracker()->changes();
  }

 private:
  base::test::ScopedTaskEnvironment task_environment_{
      base::test::ScopedTaskEnvironment::MainThreadType::UI};
  aura::test::AuraTestHelper aura_test_helper_;
  std::unique_ptr<wm::ScopedCaptureClient> scoped_capture_client_;
  TestWindowServiceDelegate delegate_;
  std::unique_ptr<WindowService> service_;
  TestWindowTreeClient window_tree_client_;
  std::unique_ptr<WindowServiceClient> window_service_client_;
  std::unique_ptr<WindowServiceClientTestHelper> client_test_helper_;

  DISALLOW_COPY_AND_ASSIGN(WindowServiceTestSetup);
};

// Embedding contains the object necessary for an embedding. This is created
// by way of WindowServiceTestSetup::CreateEmbededing().
//
// NOTE: destroying this object does not destroy the embedding.
struct Embedding {
  Embedding();
  ~Embedding();

  std::vector<Change>* changes() {
    return window_tree_client.tracker()->changes();
  }

  TestWindowTreeClient window_tree_client;

  // NOTE: this is owned by the WindowServiceClient that Embed() was called on.
  WindowServiceClient* window_service_client = nullptr;

  std::unique_ptr<WindowServiceClientTestHelper> client_test_helper;
};

}  // namespace ws2
}  // namespace ui

#endif  // SERVICES_UI_WS2_WINDOW_SERVICE_TEST_SETUP_H_
