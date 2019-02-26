// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_AURA_H_
#define CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_AURA_H_

#include "base/macros.h"
#include "chromecast/browser/cast_content_gesture_handler.h"
#include "chromecast/browser/cast_content_window.h"
#include "ui/aura/window_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace content {
class WebContents;
}  // namespace content

namespace chromecast {
namespace shell {

class TouchBlocker;

class CastContentWindowAura : public CastContentWindow,
                              public aura::WindowObserver {
 public:
  ~CastContentWindowAura() override;

  // CastContentWindow implementation:
  void CreateWindowForWebContents(
      content::WebContents* web_contents,
      CastWindowManager* window_manager,
      CastWindowManager::WindowId z_order,
      VisibilityPriority visibility_priority) override;
  void GrantScreenAccess() override;
  void RevokeScreenAccess() override;
  void RequestVisibility(VisibilityPriority visibility_priority) override;
  void NotifyVisibilityChange(VisibilityType visibility_type) override;
  void RequestMoveOut() override;
  void EnableTouchInput(bool enabled) override;

  // aura::WindowObserver implementation:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;
  void OnWindowDestroyed(aura::Window* window) override;

 private:
  friend class CastContentWindow;

  // This class should only be instantiated by CastContentWindow::Create.
  CastContentWindowAura(const CastContentWindow::CreateParams& params);

  CastContentWindow::Delegate* const delegate_;

  // Utility class for detecting and dispatching gestures to delegates.
  std::unique_ptr<CastContentGestureHandler> gesture_dispatcher_;
  CastContentGestureHandler::Priority const gesture_priority_;

  const bool is_touch_enabled_;
  std::unique_ptr<TouchBlocker> touch_blocker_;

  // TODO(seantopping): Inject in constructor.
  CastWindowManager* window_manager_ = nullptr;
  aura::Window* window_;
  bool has_screen_access_;

  DISALLOW_COPY_AND_ASSIGN(CastContentWindowAura);
};

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_BROWSER_CAST_CONTENT_WINDOW_AURA_H_
