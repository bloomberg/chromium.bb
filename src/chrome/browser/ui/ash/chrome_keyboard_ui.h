// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_CHROME_KEYBOARD_UI_H_
#define CHROME_BROWSER_UI_ASH_CHROME_KEYBOARD_UI_H_

#include <memory>

#include "base/macros.h"
#include "ui/aura/window_observer.h"
#include "ui/keyboard/keyboard_ui.h"

class ChromeKeyboardWebContents;
class GURL;

namespace aura {
class Window;
}

namespace gfx {
class Rect;
}

namespace content {
class BrowserContext;
}

namespace ui {
class InputMethod;
class Shadow;
}  // namespace ui

// Subclass of KeyboardUI. It is used by KeyboardController to get
// access to the virtual keyboard window and setup Chrome extension functions.
class ChromeKeyboardUI : public keyboard::KeyboardUI,
                         public aura::WindowObserver {
 public:
  class TestApi {
   public:
    // Use an empty |url| to clear the override.
    static void SetOverrideVirtualKeyboardUrl(const GURL& url);

   private:
    DISALLOW_IMPLICIT_CONSTRUCTORS(TestApi);
  };

  explicit ChromeKeyboardUI(content::BrowserContext* context);
  ~ChromeKeyboardUI() override;

  // Called when a window being observed changes bounds, to update its insets.
  void UpdateInsetsForWindow(aura::Window* window);

  // Overridden from KeyboardUI:
  aura::Window* GetKeyboardWindow() override;
  bool HasKeyboardWindow() const override;
  ui::InputMethod* GetInputMethod() override;
  void ReloadKeyboardIfNeeded() override;
  void InitInsets(const gfx::Rect& new_bounds) override;
  void ResetInsets() override;

  // aura::WindowObserver overrides:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowDestroyed(aura::Window* window) override;
  void OnWindowParentChanged(aura::Window* window,
                             aura::Window* parent) override;

 private:
  class WindowBoundsChangeObserver;

  // Gets the virtual keyboard URL, either the default keyboard URL or the IME
  // override URL.
  GURL GetVirtualKeyboardUrl();

  // Determines whether a particular window should have insets for overscroll.
  bool ShouldEnableInsets(aura::Window* window);

  // Whether this window should do an overscroll to avoid occlusion by the
  // virtual keyboard. IME windows and virtual keyboard windows should always
  // avoid overscroll.
  bool ShouldWindowOverscroll(aura::Window* window);

  // Adds an observer for tracking changes to a window size or
  // position while the keyboard is displayed. Any window repositioning
  // invalidates insets for overscrolling.
  void AddBoundsChangedObserver(aura::Window* window);

  // Sets shadow around the keyboard. If shadow has not been created yet,
  // this method creates it.
  void SetShadowAroundKeyboard();

  // The BrowserContext to use for creating the WebContents hosting the
  // keyboard.
  content::BrowserContext* const browser_context_;

  std::unique_ptr<ChromeKeyboardWebContents> keyboard_contents_;
  std::unique_ptr<ui::Shadow> shadow_;
  std::unique_ptr<WindowBoundsChangeObserver> window_bounds_observer_;

  DISALLOW_COPY_AND_ASSIGN(ChromeKeyboardUI);
};

#endif  // CHROME_BROWSER_UI_ASH_CHROME_KEYBOARD_UI_H_
