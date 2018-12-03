// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/ws/client_root.h"

#include <string>

#include "services/ws/public/cpp/property_type_converters.h"
#include "services/ws/public/mojom/window_manager.mojom.h"
#include "services/ws/window_service.h"
#include "services/ws/window_service_test_setup.h"
#include "services/ws/window_tree_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/mus/property_converter.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tracker.h"

namespace ws {
namespace {

// WindowObserver that changes a property (|aura::client::kNameKey|) from
// OnWindowPropertyChanged(). This mirrors ash changing a property when applying
// a property change from a client.
class CascadingPropertyTestHelper : public aura::WindowObserver {
 public:
  explicit CascadingPropertyTestHelper(aura::Window* window) : window_(window) {
    window_->AddObserver(this);
  }
  ~CascadingPropertyTestHelper() override { window_->RemoveObserver(this); }

  bool did_set_property() const { return did_set_property_; }

  // WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override {
    if (!did_set_property_) {
      did_set_property_ = true;
      window->SetProperty(aura::client::kNameKey, new std::string("TEST"));
    }
  }

 private:
  aura::Window* window_;
  bool did_set_property_ = false;

  DISALLOW_COPY_AND_ASSIGN(CascadingPropertyTestHelper);
};

// Verifies a property change that occurs while servicing a property change from
// the client results in notifying the client of the new property.
TEST(ClientRoot, CascadingPropertyChange) {
  WindowServiceTestSetup setup;
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  ASSERT_TRUE(top_level);
  setup.changes()->clear();
  CascadingPropertyTestHelper property_helper(top_level);

  // Apply a change from a client.
  aura::PropertyConverter::PrimitiveType client_value = true;
  std::vector<uint8_t> client_transport_value =
      mojo::ConvertTo<std::vector<uint8_t>>(client_value);
  setup.window_tree_test_helper()->SetWindowProperty(
      top_level, mojom::WindowManager::kAlwaysOnTop_Property,
      client_transport_value, 2);

  // CascadingPropertyTestHelper should have gotten the change *and* changed
  // another property.
  EXPECT_TRUE(property_helper.did_set_property());
  ASSERT_FALSE(setup.changes()->empty());

  // The client should be notified of the new value.
  EXPECT_EQ(CHANGE_TYPE_PROPERTY_CHANGED, (*setup.changes())[0].type);
  EXPECT_EQ(mojom::WindowManager::kName_Property,
            (*setup.changes())[0].property_key);
  setup.changes()->erase(setup.changes()->begin());

  // And the initial change should be acked with completed.
  EXPECT_EQ("ChangeCompleted id=2 success=true",
            SingleChangeToDescription(*setup.changes()));
  EXPECT_TRUE(top_level->GetProperty(aura::client::kAlwaysOnTopKey));
}

// Verifies embedded clients are notified of changes in screen bounds.
TEST(ClientRoot, EmbedBoundsInScreen) {
  WindowServiceTestSetup setup;
  aura::Window* embed_window = setup.window_tree_test_helper()->NewWindow();
  embed_window->SetBounds(gfx::Rect(1, 2, 3, 4));
  aura::Window* window = setup.window_tree_test_helper()->NewWindow();
  aura::Window* top_level =
      setup.window_tree_test_helper()->NewTopLevelWindow();
  std::unique_ptr<EmbeddingHelper> embedding_helper =
      setup.CreateEmbedding(embed_window);
  embedding_helper->window_tree_client.set_track_root_bounds_changes(true);
  ASSERT_TRUE(embedding_helper);
  embedding_helper->changes()->clear();
  window->AddChild(embed_window);
  EXPECT_TRUE(embedding_helper->changes()->empty());
  top_level->AddChild(window);
  std::vector<Change>* embedding_changes = embedding_helper->changes();
  auto iter =
      FirstChangeOfType(*embedding_changes, CHANGE_TYPE_NODE_BOUNDS_CHANGED);
  ASSERT_NE(iter, embedding_changes->end());
  EXPECT_EQ(gfx::Rect(1, 2, 3, 4), iter->bounds2);
  embedding_changes->clear();

  window->SetBounds(gfx::Rect(11, 12, 100, 100));
  iter = FirstChangeOfType(*embedding_changes, CHANGE_TYPE_NODE_BOUNDS_CHANGED);
  ASSERT_NE(iter, embedding_changes->end());
  EXPECT_EQ(gfx::Rect(12, 14, 3, 4), iter->bounds2);
  embedding_changes->clear();

  top_level->SetBounds(gfx::Rect(100, 50, 100, 100));
  iter = FirstChangeOfType(*embedding_changes, CHANGE_TYPE_NODE_BOUNDS_CHANGED);
  ASSERT_NE(iter, embedding_changes->end());
  EXPECT_EQ(gfx::Rect(112, 64, 3, 4), iter->bounds2);
}

}  // namespace
}  // namespace ws
