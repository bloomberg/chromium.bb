// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_AURA_TEST_MUS_WINDOW_TREE_CLIENT_TEST_API_H_
#define UI_AURA_TEST_MUS_WINDOW_TREE_CLIENT_TEST_API_H_

#include <stdint.h>

#include <memory>

#include "base/macros.h"
#include "services/ws/public/mojom/window_tree_constants.mojom.h"
#include "ui/aura/mus/mus_types.h"
#include "ui/aura/mus/window_tree_client.h"
#include "ui/aura/test/mus/change_completion_waiter.h"
#include "ui/aura/test/ui_controls_factory_aura.h"

namespace ws {
namespace mojom {
class WindowTree;
}
}  // namespace ws

namespace ui {
class Event;
}

namespace aura {

class EmbedRoot;
class Window;
class WindowMus;
class WindowTreeClientDelegate;
class WindowTreeClient;

enum class ChangeType;

// Use to access implementation details of WindowTreeClient.
class WindowTreeClientTestApi {
 public:
  explicit WindowTreeClientTestApi(WindowTreeClient* tree_client_impl);
  explicit WindowTreeClientTestApi(Window* window);
  ~WindowTreeClientTestApi();

  static std::unique_ptr<WindowTreeClient> CreateWindowTreeClient(
      WindowTreeClientDelegate* window_tree_delegate);

  // Calls OnEmbed() on the WindowTreeClient.
  void OnEmbed(ws::mojom::WindowTree* window_tree);

  // Simulates |event| matching an event observer on the window server.
  void CallOnObservedInputEvent(std::unique_ptr<ui::Event> event);

  void CallOnCaptureChanged(Window* new_capture, Window* old_capture);

  // Simulates the EmbedRoot receiving the token from the WindowTree and then
  // the WindowTree calling OnEmbedFromToken(). |visible| is the initial value
  // to supply from the server for the visibility.
  void CallOnEmbedFromToken(EmbedRoot* embed_root,
                            bool visible = true,
                            const viz::LocalSurfaceIdAllocation& lsia =
                                viz::LocalSurfaceIdAllocation());

  // Sets the WindowTree. This calls WindowTreeConnectionEstablished(), which
  // means it should only be called once, during setup.
  void SetTree(ws::mojom::WindowTree* window_tree);

  // Swaps the existing WindowTree reference to a new one. Returns the old.
  ws::mojom::WindowTree* SwapTree(ws::mojom::WindowTree* window_tree);

  bool HasEventObserver();

  Window* GetWindowByServerId(ws::Id id);

  WindowMus* NewWindowFromWindowData(WindowMus* parent,
                                     const ws::mojom::WindowData& window_data);

  bool HasInFlightChanges();

  bool HasChangeInFlightOfType(ChangeType type);

 private:
#if defined(USE_OZONE)
  friend void test::OnWindowServiceProcessedEvent(base::OnceClosure closure,
                                                  bool result);
#endif
  friend void test::WaitForAllChangesToComplete(WindowTreeClient* client);

  // |visible| whether the window is visible.
  ws::mojom::WindowDataPtr CreateWindowDataForEmbed(bool visible = true);

  // This is private as WaitForAllChangesToComplete() (in
  // change_completion_waiter) should be used instead.
  void FlushForTesting();

  WindowTreeClient* tree_client_impl_;

  DISALLOW_COPY_AND_ASSIGN(WindowTreeClientTestApi);
};

}  // namespace aura

#endif  // UI_AURA_TEST_MUS_WINDOW_TREE_CLIENT_TEST_API_H_
