// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_KEYBOARD_CONTROLLER_H_
#define UI_KEYBOARD_KEYBOARD_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/event_types.h"
#include "base/memory/scoped_ptr.h"
#include "base/observer_list.h"
#include "ui/aura/window_observer.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/keyboard/keyboard_export.h"

namespace aura {
class Window;
}
namespace gfx {
class Rect;
}
namespace ui {
class InputMethod;
class TextInputClient;
}

namespace keyboard {

class KeyboardControllerObserver;
class KeyboardControllerProxy;
class KeyboardLayoutManager;

// Provides control of the virtual keyboard, including providing a container
// and controlling visibility.
class KEYBOARD_EXPORT KeyboardController : public ui::InputMethodObserver,
                                           public aura::WindowObserver {
 public:
  // Different ways to hide the keyboard.
  enum HideReason {
    // System initiated.
    HIDE_REASON_AUTOMATIC,
    // User initiated.
    HIDE_REASON_MANUAL,
  };

  // Takes ownership of |proxy|.
  explicit KeyboardController(KeyboardControllerProxy* proxy);
  virtual ~KeyboardController();

  // Returns the container for the keyboard, which is owned by
  // KeyboardController.
  aura::Window* GetContainerWindow();

  // Hides virtual keyboard and notifies observer bounds change.
  // This function should be called with a delay to avoid layout flicker
  // when the focus of input field quickly change. |automatic| is true when the
  // call is made by the system rather than initiated by the user.
  void HideKeyboard(HideReason reason);

  // Management of the observer list.
  virtual void AddObserver(KeyboardControllerObserver* observer);
  virtual void RemoveObserver(KeyboardControllerObserver* observer);

 private:
  // For access to Observer methods for simulation.
  friend class KeyboardControllerTest;

  // aura::WindowObserver overrides
  virtual void OnWindowHierarchyChanged(
      const HierarchyChangeParams& params) OVERRIDE;

  // InputMethodObserver overrides
  virtual void OnTextInputTypeChanged(
      const ui::TextInputClient* client) OVERRIDE {}
  virtual void OnFocus() OVERRIDE {}
  virtual void OnBlur() OVERRIDE {}
  virtual void OnUntranslatedIMEMessage(
      const base::NativeEvent& event) OVERRIDE {}
  virtual void OnCaretBoundsChanged(
      const ui::TextInputClient* client) OVERRIDE {}
  virtual void OnInputLocaleChanged() OVERRIDE {}
  virtual void OnTextInputStateChanged(
      const ui::TextInputClient* client) OVERRIDE;
  virtual void OnInputMethodDestroyed(
      const ui::InputMethod* input_method) OVERRIDE;

  // Returns true if keyboard is scheduled to hide.
  bool WillHideKeyboard() const;

  scoped_ptr<KeyboardControllerProxy> proxy_;
  scoped_ptr<aura::Window> container_;
  ui::InputMethod* input_method_;
  bool keyboard_visible_;

  ObserverList<KeyboardControllerObserver> observer_list_;

  base::WeakPtrFactory<KeyboardController> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardController);
};

}  // namespace keyboard

#endif  // UI_KEYBOARD_KEYBOARD_CONTROLLER_H_
