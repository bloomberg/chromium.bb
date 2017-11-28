// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "services/ui/common/util.h"
#include "services/ui/ws/test_utils.h"
#include "services/ui/ws/window_server_test_base.h"
#include "ui/aura/client/transient_window_client.h"
#include "ui/aura/env.h"
#include "ui/aura/mus/window_port_mus.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/mus/window_tree_client_delegate.h"
#include "ui/aura/mus/window_tree_host_mus.h"
#include "ui/aura/mus/window_tree_host_mus_init_params.h"
#include "ui/aura/test/mus/window_tree_client_private.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/aura/window_tracker.h"
#include "ui/gfx/geometry/rect.h"

namespace ui {
namespace ws {

namespace {

Id server_id(aura::Window* window) {
  return aura::WindowMus::Get(window)->server_id();
}

aura::Window* GetChildWindowByServerId(aura::WindowTreeClient* client,
                                       aura::Id id) {
  return aura::WindowTreeClientPrivate(client).GetWindowByServerId(id);
}

class TestWindowManagerDelegate : public aura::WindowManagerDelegate {
 public:
  TestWindowManagerDelegate() {}
  ~TestWindowManagerDelegate() override {}

  // WindowManagerDelegate:
  void SetWindowManagerClient(aura::WindowManagerClient* client) override {}
  void OnWmAcceleratedWidgetAvailableForDisplay(
      int64_t display_id,
      gfx::AcceleratedWidget widget) override {}
  void OnWmConnected() override {}
  void OnWmSetBounds(aura::Window* window, const gfx::Rect& bounds) override {}
  bool OnWmSetProperty(
      aura::Window* window,
      const std::string& name,
      std::unique_ptr<std::vector<uint8_t>>* new_data) override {
    return false;
  }
  void OnWmSetModalType(aura::Window* window, ui::ModalType type) override {}
  void OnWmSetCanFocus(aura::Window* window, bool can_focus) override {}
  aura::Window* OnWmCreateTopLevelWindow(
      ui::mojom::WindowType window_type,
      std::map<std::string, std::vector<uint8_t>>* properties) override {
    return nullptr;
  }
  void OnWmClientJankinessChanged(const std::set<aura::Window*>& client_windows,
                                  bool not_responding) override {}
  void OnWmBuildDragImage(const gfx::Point& screen_location,
                          const SkBitmap& drag_image,
                          const gfx::Vector2d& drag_image_offset,
                          ui::mojom::PointerKind source) override {}
  void OnWmMoveDragImage(const gfx::Point& screen_location) override {}
  void OnWmDestroyDragImage() override {}
  void OnWmWillCreateDisplay(const display::Display& display) override {}
  void OnWmNewDisplay(std::unique_ptr<aura::WindowTreeHostMus> window_tree_host,
                      const display::Display& display) override {}
  void OnWmDisplayRemoved(aura::WindowTreeHostMus* window_tree_host) override {}
  void OnWmDisplayModified(const display::Display& display) override {}
  mojom::EventResult OnAccelerator(
      uint32_t accelerator_id,
      const ui::Event& event,
      std::unordered_map<std::string, std::vector<uint8_t>>* properties)
      override {
    return ui::mojom::EventResult::UNHANDLED;
  }
  void OnCursorTouchVisibleChanged(bool enabled) override {}
  void OnWmPerformMoveLoop(aura::Window* window,
                           mojom::MoveLoopSource source,
                           const gfx::Point& cursor_location,
                           const base::Callback<void(bool)>& on_done) override {
  }
  void OnWmCancelMoveLoop(aura::Window* window) override {}
  void OnWmSetClientArea(
      aura::Window* window,
      const gfx::Insets& insets,
      const std::vector<gfx::Rect>& additional_client_areas) override {}
  bool IsWindowActive(aura::Window* window) override { return true; }
  void OnWmDeactivateWindow(aura::Window* window) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestWindowManagerDelegate);
};

class BoundsChangeObserver : public aura::WindowObserver {
 public:
  explicit BoundsChangeObserver(aura::Window* window) : window_(window) {
    window_->AddObserver(this);
  }
  ~BoundsChangeObserver() override { window_->RemoveObserver(this); }

 private:
  // Overridden from WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override {
    DCHECK_EQ(window, window_);
    EXPECT_TRUE(WindowServerTestBase::QuitRunLoop());
  }

  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(BoundsChangeObserver);
};

// Wait until the bounds of the supplied window change; returns false on
// timeout.
bool WaitForBoundsToChange(aura::Window* window) {
  BoundsChangeObserver observer(window);
  return WindowServerTestBase::DoRunLoopWithTimeout();
}

// Spins a run loop until the tree beginning at |root| has |tree_size| windows
// (including |root|).
class TreeSizeMatchesObserver : public aura::WindowObserver {
 public:
  TreeSizeMatchesObserver(aura::Window* tree, size_t tree_size)
      : tree_(tree), tree_size_(tree_size) {
    tree_->AddObserver(this);
  }
  ~TreeSizeMatchesObserver() override { tree_->RemoveObserver(this); }

  bool IsTreeCorrectSize() { return CountWindows(tree_) == tree_size_; }

 private:
  // Overridden from WindowObserver:
  void OnWindowHierarchyChanged(const HierarchyChangeParams& params) override {
    if (IsTreeCorrectSize())
      EXPECT_TRUE(WindowServerTestBase::QuitRunLoop());
  }

  size_t CountWindows(const aura::Window* window) const {
    size_t count = 1;
    for (const aura::Window* child : window->children())
      count += CountWindows(child);
    return count;
  }

  aura::Window* tree_;
  size_t tree_size_;

  DISALLOW_COPY_AND_ASSIGN(TreeSizeMatchesObserver);
};

// Wait until |window| has |tree_size| descendants; returns false on timeout.
// The count includes |window|. For example, if you want to wait for |window| to
// have a single child, use a |tree_size| of 2.
bool WaitForTreeSizeToMatch(aura::Window* window, size_t tree_size) {
  TreeSizeMatchesObserver observer(window, tree_size);
  return observer.IsTreeCorrectSize() ||
         WindowServerTestBase::DoRunLoopWithTimeout();
}

class StackingOrderChangeObserver : public aura::WindowObserver {
 public:
  StackingOrderChangeObserver(aura::Window* window) : window_(window) {
    window_->AddObserver(this);
  }
  ~StackingOrderChangeObserver() override { window_->RemoveObserver(this); }

 private:
  // Overridden from aura::WindowObserver:
  void OnWindowStackingChanged(aura::Window* window) override {
    DCHECK_EQ(window, window_);
    EXPECT_TRUE(WindowServerTestBase::QuitRunLoop());
  }

  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(StackingOrderChangeObserver);
};

// Wait until |window|'s tree size matches |tree_size|; returns false on
// timeout.
bool WaitForStackingOrderChange(aura::Window* window) {
  StackingOrderChangeObserver observer(window);
  return WindowServerTestBase::DoRunLoopWithTimeout();
}

// Tracks a window's destruction. Query is_valid() for current state.
class WindowTracker : public aura::WindowObserver {
 public:
  explicit WindowTracker(aura::Window* window) : window_(window) {
    window_->AddObserver(this);
  }
  ~WindowTracker() override {
    if (window_)
      window_->RemoveObserver(this);
  }

  bool is_valid() const { return !!window_; }

 private:
  // Overridden from WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override {
    DCHECK_EQ(window, window_);
    window_ = nullptr;
  }

  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(WindowTracker);
};

// Creates a new visible Window. If |parent| is non-null the newly created
// window is added to it.
aura::Window* NewVisibleWindow(
    aura::Window* parent,
    aura::WindowTreeClient* client,
    aura::WindowMusType type = aura::WindowMusType::LOCAL) {
  std::unique_ptr<aura::WindowPortMus> window_port_mus =
      std::make_unique<aura::WindowPortMus>(client, type);
  aura::Window* window = new aura::Window(nullptr, std::move(window_port_mus));
  window->Init(ui::LAYER_NOT_DRAWN);
  window->Show();
  if (parent)
    parent->AddChild(window);
  return window;
}

}  // namespace

// WindowServer
// -----------------------------------------------------------------

struct EmbedResult {
  bool IsValid() const {
    return window_tree_client.get() != nullptr &&
           window_tree_host.get() != nullptr;
  }

  std::unique_ptr<aura::WindowTreeClient> window_tree_client;
  std::unique_ptr<aura::WindowTreeHostMus> window_tree_host;
};

aura::Window* GetFirstRoot(aura::WindowTreeClient* client) {
  return client->GetRoots().empty() ? nullptr : *client->GetRoots().begin();
}

// These tests model synchronization of two peer clients of the window server,
// that are given access to some root window.

class WindowServerTest : public WindowServerTestBase {
 public:
  struct ClientAreaChange {
    aura::Window* window = nullptr;
    gfx::Insets insets;
  };

  WindowServerTest() {}

  aura::Window* GetFirstWMRoot() { return GetFirstRoot(window_manager()); }

  // Embeds another version of the test app @ window. This runs a run loop until
  // a response is received, or a timeout. The return value is always non-null,
  // but if there is an error there is no WindowTreeClient. Always use
  // ASSERT_EQ(result->IsValid()) on the return value.
  std::unique_ptr<EmbedResult> Embed(aura::WindowTreeClient* window_tree_client,
                                     aura::Window* window) {
    DCHECK(!embed_details_);
    embed_details_ = std::make_unique<EmbedDetails>();
    window_tree_client->Embed(window, ConnectAndGetWindowServerClient(), 0,
                              base::Bind(&WindowServerTest::EmbedCallbackImpl,
                                         base::Unretained(this)));
    if (embed_details_->callback_run) {
      // The callback was run immediately, this indicates an immediate failure,
      // such as |window| has children.
      EXPECT_FALSE(embed_details_->embed_result);
      embed_details_.reset();
      return std::make_unique<EmbedResult>();
    }
    // Wait for EmbedCallbackImpl() to be called with the result.
    embed_details_->waiting = true;
    if (!WindowServerTestBase::DoRunLoopWithTimeout()) {
      embed_details_.reset();
      return std::make_unique<EmbedResult>();
    }
    std::unique_ptr<EmbedResult> result = std::move(embed_details_->result);
    embed_details_.reset();
    return result;
  }

  // Establishes a connection to this application and asks for a
  // WindowTreeClient.
  ui::mojom::WindowTreeClientPtr ConnectAndGetWindowServerClient() {
    ui::mojom::WindowTreeClientPtr client;
    connector()->BindInterface(test_name(), &client);
    return client;
  }

  std::unique_ptr<ClientAreaChange> WaitForClientAreaToChange() {
    client_area_change_ = std::make_unique<ClientAreaChange>();
    // The nested run loop is quit in OnWmSetClientArea(). Client area
    // changes don't route through the window, only the WindowManagerDelegate.
    if (!WindowServerTestBase::DoRunLoopWithTimeout()) {
      client_area_change_.reset();
      return nullptr;
    }
    return std::move(client_area_change_);
  }

  // WindowServerTestBase:
  void OnEmbed(
      std::unique_ptr<aura::WindowTreeHostMus> window_tree_host) override {
    if (!embed_details_) {
      WindowServerTestBase::OnEmbed(std::move(window_tree_host));
      return;
    }

    embed_details_->result->window_tree_host = std::move(window_tree_host);
    embed_details_->result->window_tree_client = ReleaseMostRecentClient();
    if (embed_details_->callback_run)
      EXPECT_TRUE(WindowServerTestBase::QuitRunLoop());
  }
  void OnWmSetClientArea(
      aura::Window* window,
      const gfx::Insets& insets,
      const std::vector<gfx::Rect>& additional_client_areas) override {
    if (!client_area_change_.get())
      return;

    client_area_change_->window = window;
    client_area_change_->insets = insets;
    EXPECT_TRUE(WindowServerTestBase::QuitRunLoop());
  }

 private:
  // Used to track the state of a call to window->Embed().
  struct EmbedDetails {
    EmbedDetails() : result(std::make_unique<EmbedResult>()) {}

    // The callback function supplied to Embed() was called.
    bool callback_run = false;

    // The boolean supplied to the Embed() callback.
    bool embed_result = false;

    // Whether a MessageLoop is running.
    bool waiting = false;

    std::unique_ptr<EmbedResult> result;
  };

  void EmbedCallbackImpl(bool result) {
    embed_details_->callback_run = true;
    embed_details_->embed_result = result;
    if (embed_details_->waiting &&
        (!result || embed_details_->result->window_tree_client))
      EXPECT_TRUE(WindowServerTestBase::QuitRunLoop());
  }

  std::unique_ptr<EmbedDetails> embed_details_;

  std::unique_ptr<ClientAreaChange> client_area_change_;

  DISALLOW_COPY_AND_ASSIGN(WindowServerTest);
};

TEST_F(WindowServerTest, RootWindow) {
  ASSERT_NE(nullptr, window_manager());
  EXPECT_EQ(1u, window_manager()->GetRoots().size());
}

TEST_F(WindowServerTest, Embed) {
  aura::Window* window = NewVisibleWindow(GetFirstWMRoot(), window_manager(),
                                          aura::WindowMusType::EMBED_IN_OWNER);
  std::unique_ptr<EmbedResult> embed_result = Embed(window_manager(), window);
  ASSERT_TRUE(embed_result->IsValid());

  aura::Window* embed_root = embed_result->window_tree_host->window();
  // WindowTreeHost::window() is the single root of the embed.
  EXPECT_EQ(1u, embed_result->window_tree_client->GetRoots().size());
  EXPECT_EQ(embed_root, GetFirstRoot(embed_result->window_tree_client.get()));
  EXPECT_EQ(LoWord(server_id(window)), LoWord(server_id(embed_root)));
  EXPECT_NE(0u, server_id(embed_root) >> 16);
  EXPECT_EQ(nullptr, embed_root->parent());
  EXPECT_TRUE(embed_root->children().empty());
}

// Window manager has two windows, N1 and N11. Embeds A at N1. A should not see
// N11.
TEST_F(WindowServerTest, EmbeddedDoesntSeeChild) {
  aura::Window* window = NewVisibleWindow(GetFirstWMRoot(), window_manager(),
                                          aura::WindowMusType::EMBED_IN_OWNER);
  std::unique_ptr<EmbedResult> embed_result = Embed(window_manager(), window);
  ASSERT_TRUE(embed_result->IsValid());
  aura::Window* embed_root = embed_result->window_tree_host->window();
  EXPECT_EQ(LoWord(server_id(window)), LoWord(server_id(embed_root)));
  EXPECT_NE(0u, server_id(embed_root) >> 16);
  EXPECT_EQ(nullptr, embed_root->parent());
  EXPECT_TRUE(embed_root->children().empty());
}

// TODO(beng): write a replacement test for the one that once existed here:
// This test validates the following scenario:
// -  a window originating from one client
// -  a window originating from a second client
// +  the client originating the window is destroyed
// -> the window should still exist (since the second client is live) but
//    should be disconnected from any windows.
// http://crbug.com/396300
//
// TODO(beng): The new test should validate the scenario as described above
//             except that the second client still has a valid tree.

// Verifies that bounds changes applied to a window hierarchy in one client
// are reflected to another.
TEST_F(WindowServerTest, SetBounds) {
  aura::Window* window = NewVisibleWindow(GetFirstWMRoot(), window_manager(),
                                          aura::WindowMusType::EMBED_IN_OWNER);

  std::unique_ptr<EmbedResult> embed_result = Embed(window_manager(), window);
  ASSERT_TRUE(embed_result->IsValid());
  aura::Window* embed_root = embed_result->window_tree_host->window();
  EXPECT_EQ(window->bounds(), embed_root->bounds());

  window->SetBounds(gfx::Rect(0, 0, 100, 100));
  ASSERT_TRUE(WaitForBoundsToChange(embed_root));
  EXPECT_EQ(window->bounds(), embed_root->bounds());
}

// Verifies that bounds changes applied to a window owned by a different
// client can be refused.
TEST_F(WindowServerTest, SetBoundsSecurity) {
  TestWindowManagerDelegate wm_delegate;
  set_window_manager_delegate(&wm_delegate);

  aura::Window* window = NewVisibleWindow(GetFirstWMRoot(), window_manager(),
                                          aura::WindowMusType::EMBED_IN_OWNER);

  std::unique_ptr<EmbedResult> embed_result = Embed(window_manager(), window);
  ASSERT_TRUE(embed_result->IsValid());
  aura::Window* embed_root = embed_result->window_tree_host->window();
  window->SetBounds(gfx::Rect(0, 0, 800, 600));
  ASSERT_TRUE(WaitForBoundsToChange(embed_root));

  embed_result->window_tree_host->SetBoundsInPixels(gfx::Rect(0, 0, 1024, 768));
  // Bounds change is initially accepted, but the server declines the request.
  EXPECT_NE(window->bounds(), embed_root->bounds());

  // The client is notified when the requested is declined, and updates the
  // local bounds accordingly.
  ASSERT_TRUE(WaitForBoundsToChange(embed_root));
  EXPECT_EQ(window->bounds(), embed_root->bounds());
  set_window_manager_delegate(nullptr);
}

// Verifies that a root window can always be destroyed.
TEST_F(WindowServerTest, DestroySecurity) {
  aura::Window* window = NewVisibleWindow(GetFirstWMRoot(), window_manager(),
                                          aura::WindowMusType::EMBED_IN_OWNER);

  std::unique_ptr<EmbedResult> embed_result = Embed(window_manager(), window);
  ASSERT_TRUE(embed_result->IsValid());
  aura::Window* embed_root = embed_result->window_tree_host->window();

  // The root can be destroyed, even though it was not created by the client.
  aura::WindowTracker tracker;
  tracker.Add(window);
  tracker.Add(embed_root);
  embed_result->window_tree_host.reset();
  EXPECT_FALSE(tracker.Contains(embed_root));
  EXPECT_TRUE(tracker.Contains(window));

  delete window;
  EXPECT_FALSE(tracker.Contains(window));
}

TEST_F(WindowServerTest, MultiRoots) {
  aura::Window* window1 = NewVisibleWindow(GetFirstWMRoot(), window_manager(),
                                           aura::WindowMusType::EMBED_IN_OWNER);
  aura::Window* window2 = NewVisibleWindow(GetFirstWMRoot(), window_manager(),
                                           aura::WindowMusType::EMBED_IN_OWNER);
  std::unique_ptr<EmbedResult> embed_result1 = Embed(window_manager(), window1);
  ASSERT_TRUE(embed_result1->IsValid());
  std::unique_ptr<EmbedResult> embed_result2 = Embed(window_manager(), window2);
  ASSERT_TRUE(embed_result2->IsValid());
}

TEST_F(WindowServerTest, Reorder) {
  aura::Window* window1 = NewVisibleWindow(GetFirstWMRoot(), window_manager(),
                                           aura::WindowMusType::EMBED_IN_OWNER);

  std::unique_ptr<EmbedResult> embed_result = Embed(window_manager(), window1);
  ASSERT_TRUE(embed_result->IsValid());
  aura::WindowTreeClient* embedded = embed_result->window_tree_client.get();
  aura::Window* embed_root = embed_result->window_tree_host->window();

  aura::Window* window11 = NewVisibleWindow(embed_root, embedded);
  aura::Window* window12 = NewVisibleWindow(embed_root, embedded);
  ASSERT_TRUE(WaitForTreeSizeToMatch(window1, 3u));

  // |embedded|'s WindowTree has an id_ of embedded_client_id, so window11's
  // client_id part should be embedded_client_id in the WindowTree for
  // window_manager(). Similar for window12.
  ClientSpecificId embedded_client_id = test::kWindowManagerClientId + 1;
  Id window11_in_wm = embedded_client_id << 16 | LoWord(server_id(window11));
  Id window12_in_wm = embedded_client_id << 16 | LoWord(server_id(window12));

  {
    window11->parent()->StackChildAtTop(window11);
    // The |embedded| tree should be updated immediately.
    EXPECT_EQ(embed_root->children().front(),
              GetChildWindowByServerId(embedded, server_id(window12)));
    EXPECT_EQ(embed_root->children().back(),
              GetChildWindowByServerId(embedded, server_id(window11)));

    // The window_manager() tree is still not updated.
    EXPECT_EQ(window1->children().back(),
              GetChildWindowByServerId(window_manager(), window12_in_wm));

    // Wait until window_manager() tree is updated.
    ASSERT_TRUE(WaitForStackingOrderChange(
        GetChildWindowByServerId(window_manager(), window11_in_wm)));
    EXPECT_EQ(window1->children().front(),
              GetChildWindowByServerId(window_manager(), window12_in_wm));
    EXPECT_EQ(window1->children().back(),
              GetChildWindowByServerId(window_manager(), window11_in_wm));
  }

  {
    window11->parent()->StackChildAtBottom(window11);
    // |embedded| should be updated immediately.
    EXPECT_EQ(embed_root->children().front(),
              GetChildWindowByServerId(embedded, server_id(window11)));
    EXPECT_EQ(embed_root->children().back(),
              GetChildWindowByServerId(embedded, server_id(window12)));

    // |window_manager()| is also eventually updated.
    EXPECT_EQ(window1->children().back(),
              GetChildWindowByServerId(window_manager(), window11_in_wm));
    ASSERT_TRUE(WaitForStackingOrderChange(
        GetChildWindowByServerId(window_manager(), window11_in_wm)));
    EXPECT_EQ(window1->children().front(),
              GetChildWindowByServerId(window_manager(), window11_in_wm));
    EXPECT_EQ(window1->children().back(),
              GetChildWindowByServerId(window_manager(), window12_in_wm));
  }
}

namespace {

class VisibilityChangeObserver : public aura::WindowObserver {
 public:
  explicit VisibilityChangeObserver(aura::Window* window) : window_(window) {
    window_->AddObserver(this);
  }
  ~VisibilityChangeObserver() override { window_->RemoveObserver(this); }

 private:
  // Overridden from WindowObserver:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override {
    EXPECT_EQ(window, window_);
    EXPECT_TRUE(WindowServerTestBase::QuitRunLoop());
  }

  aura::Window* window_;

  DISALLOW_COPY_AND_ASSIGN(VisibilityChangeObserver);
};

}  // namespace

TEST_F(WindowServerTest, Visible) {
  aura::Window* window1 = NewVisibleWindow(GetFirstWMRoot(), window_manager(),
                                           aura::WindowMusType::EMBED_IN_OWNER);

  // Embed another app and verify initial state.
  std::unique_ptr<EmbedResult> embed_result = Embed(window_manager(), window1);
  ASSERT_TRUE(embed_result->IsValid());
  aura::Window* embed_root = embed_result->window_tree_host->window();
  EXPECT_TRUE(embed_root->TargetVisibility());
  EXPECT_TRUE(embed_root->IsVisible());

  // Change the visible state from the first client and verify its mirrored
  // correctly to the embedded app.
  {
    VisibilityChangeObserver observer(embed_root);
    window1->Hide();
    ASSERT_TRUE(WindowServerTestBase::DoRunLoopWithTimeout());
  }

  EXPECT_FALSE(window1->TargetVisibility());
  EXPECT_FALSE(window1->IsVisible());

  EXPECT_FALSE(embed_root->TargetVisibility());
  EXPECT_FALSE(embed_root->IsVisible());

  // Make the node visible again.
  {
    VisibilityChangeObserver observer(embed_root);
    window1->Show();
    ASSERT_TRUE(WindowServerTestBase::DoRunLoopWithTimeout());
  }

  EXPECT_TRUE(window1->TargetVisibility());
  EXPECT_TRUE(window1->IsVisible());

  EXPECT_TRUE(embed_root->TargetVisibility());
  EXPECT_TRUE(embed_root->IsVisible());
}

// TODO(beng): tests for window event dispatcher.
// - verify that we see events for all windows.

TEST_F(WindowServerTest, EmbedFailsWithChildren) {
  aura::Window* window1 = NewVisibleWindow(GetFirstWMRoot(), window_manager(),
                                           aura::WindowMusType::EMBED_IN_OWNER);
  ASSERT_TRUE(NewVisibleWindow(window1, window_manager()));
  std::unique_ptr<EmbedResult> embed_result = Embed(window_manager(), window1);
  // Embed() should fail as |window1| has a child.
  EXPECT_FALSE(embed_result->IsValid());
}

namespace {

class DestroyObserver : public aura::WindowObserver {
 public:
  DestroyObserver(aura::WindowTreeClient* client, bool* got_destroy)
      : got_destroy_(got_destroy) {
    GetFirstRoot(client)->AddObserver(this);
  }
  ~DestroyObserver() override {}

 private:
  // Overridden from aura::WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override {
    *got_destroy_ = true;
    window->RemoveObserver(this);

    EXPECT_TRUE(WindowServerTestBase::QuitRunLoop());
  }

  bool* got_destroy_;

  DISALLOW_COPY_AND_ASSIGN(DestroyObserver);
};

}  // namespace

// Verifies deleting a Window that is the root of another client notifies
// observers in the right order (OnWindowDestroyed() before
// OnWindowManagerDestroyed()).
TEST_F(WindowServerTest, WindowServerDestroyedAfterRootObserver) {
  aura::Window* window = NewVisibleWindow(GetFirstWMRoot(), window_manager(),
                                          aura::WindowMusType::EMBED_IN_OWNER);

  std::unique_ptr<EmbedResult> embed_result = Embed(window_manager(), window);
  ASSERT_TRUE(embed_result->IsValid());
  aura::WindowTreeClient* embedded_client =
      embed_result->window_tree_client.get();

  bool got_destroy = false;
  DestroyObserver observer(embedded_client, &got_destroy);
  // Delete the window |embedded_client| is embedded in. |embedded_client| is
  // asynchronously notified and cleans up.
  delete window;
  EXPECT_TRUE(DoRunLoopWithTimeout());
  ASSERT_TRUE(got_destroy);
  // The WindowTreeHost was destroyed as well (by
  // WindowServerTestBase::OnEmbedRootDestroyed()).
  embed_result->window_tree_host.release();
  EXPECT_EQ(0u, embed_result->window_tree_client->GetRoots().size());
}

TEST_F(WindowServerTest, ClientAreaChanged) {
  aura::Window* window = NewVisibleWindow(GetFirstWMRoot(), window_manager(),
                                          aura::WindowMusType::EMBED_IN_OWNER);

  std::unique_ptr<EmbedResult> embed_result = Embed(window_manager(), window);
  ASSERT_TRUE(embed_result->IsValid());

  // Verify change from embedded makes it to parent.
  const gfx::Insets insets(1, 2, 3, 4);
  embed_result->window_tree_host->SetClientArea(insets,
                                                std::vector<gfx::Rect>());
  std::unique_ptr<ClientAreaChange> client_area_change =
      WaitForClientAreaToChange();
  ASSERT_TRUE(client_area_change);
  EXPECT_EQ(window, client_area_change->window);
  EXPECT_EQ(insets, client_area_change->insets);
}

class EstablishConnectionViaFactoryDelegate : public TestWindowManagerDelegate {
 public:
  explicit EstablishConnectionViaFactoryDelegate(aura::WindowTreeClient* client)
      : client_(client), run_loop_(nullptr), created_window_(nullptr) {}
  ~EstablishConnectionViaFactoryDelegate() override {}

  bool QuitOnCreate() {
    if (run_loop_)
      return false;

    created_window_ = nullptr;
    run_loop_ = std::make_unique<base::RunLoop>();
    run_loop_->Run();
    run_loop_.reset();
    return created_window_ != nullptr;
  }

  aura::Window* created_window() { return created_window_; }

  // WindowManagerDelegate:
  aura::Window* OnWmCreateTopLevelWindow(
      ui::mojom::WindowType window_type,
      std::map<std::string, std::vector<uint8_t>>* properties) override {
    created_window_ = NewVisibleWindow((*client_->GetRoots().begin()), client_,
                                       aura::WindowMusType::TOP_LEVEL_IN_WM);
    if (run_loop_)
      run_loop_->Quit();
    return created_window_;
  }

 private:
  aura::WindowTreeClient* client_;
  std::unique_ptr<base::RunLoop> run_loop_;
  aura::Window* created_window_;

  DISALLOW_COPY_AND_ASSIGN(EstablishConnectionViaFactoryDelegate);
};

TEST_F(WindowServerTest, EstablishConnectionViaFactory) {
  EstablishConnectionViaFactoryDelegate delegate(window_manager());
  set_window_manager_delegate(&delegate);
  aura::WindowTreeClient second_client(connector(), this, nullptr, nullptr,
                                       nullptr, false);
  second_client.ConnectViaWindowTreeFactory();
  aura::WindowTreeHostMus window_tree_host_in_second_client(
      aura::CreateInitParamsForTopLevel(&second_client));
  window_tree_host_in_second_client.InitHost();
  window_tree_host_in_second_client.window()->Show();
  ASSERT_TRUE(second_client.GetRoots().count(
                  window_tree_host_in_second_client.window()) > 0);
  // Wait for the window to appear in the wm.
  ASSERT_TRUE(delegate.QuitOnCreate());

  aura::Window* window_in_wm = delegate.created_window();
  ASSERT_TRUE(window_in_wm);

  // Change the bounds in the wm, and make sure the child sees it.
  const gfx::Rect window_bounds(1, 11, 12, 101);
  window_in_wm->SetBounds(window_bounds);
  ASSERT_TRUE(
      WaitForBoundsToChange(window_tree_host_in_second_client.window()));
  EXPECT_EQ(window_bounds,
            window_tree_host_in_second_client.GetBoundsInPixels());
}

TEST_F(WindowServerTest, OnWindowHierarchyChangedIncludesTransientParent) {
  // Create a second connection. In the second connection create a window,
  // parent it to the root, create another window, mark it as a transient parent
  // of the first window and then add it.
  EstablishConnectionViaFactoryDelegate delegate(window_manager());
  set_window_manager_delegate(&delegate);
  aura::WindowTreeClient second_client(connector(), this, nullptr, nullptr,
                                       nullptr, false);
  second_client.ConnectViaWindowTreeFactory();
  aura::WindowTreeHostMus window_tree_host_in_second_client(
      aura::CreateInitParamsForTopLevel(&second_client));
  window_tree_host_in_second_client.InitHost();
  window_tree_host_in_second_client.window()->Show();
  aura::Window* second_client_child = NewVisibleWindow(
      window_tree_host_in_second_client.window(), &second_client);
  // Create the transient without a parent, set transient parent, then add.
  aura::Window* transient = NewVisibleWindow(nullptr, &second_client);
  aura::client::TransientWindowClient* transient_window_client =
      aura::client::GetTransientWindowClient();
  transient_window_client->AddTransientChild(second_client_child, transient);
  second_client_child->AddChild(transient);

  // Wait for the top-level to appear in the window manager.
  ASSERT_TRUE(delegate.QuitOnCreate());
  aura::Window* top_level_in_wm = delegate.created_window();

  // Makes sure the window manager sees the same structure and the transient
  // parent is connected correctly.
  ASSERT_TRUE(WaitForTreeSizeToMatch(top_level_in_wm, 3u));
  ASSERT_EQ(1u, top_level_in_wm->children().size());
  aura::Window* second_client_child_in_wm = top_level_in_wm->children()[0];
  ASSERT_EQ(1u, second_client_child_in_wm->children().size());
  aura::Window* transient_in_wm = second_client_child_in_wm->children()[0];
  ASSERT_EQ(second_client_child_in_wm,
            transient_window_client->GetTransientParent(transient_in_wm));
}

}  // namespace ws
}  // namespace ui
