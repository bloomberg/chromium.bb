// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_KEYBOARD_KEYBOARD_CONTROLLER_PROXY_H_
#define UI_KEYBOARD_KEYBOARD_CONTROLLER_PROXY_H_

#include "base/memory/scoped_ptr.h"
#include "content/public/common/media_stream_request.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/keyboard/keyboard_export.h"

namespace aura {
class Window;
}
namespace content {
class BrowserContext;
class SiteInstance;
class WebContents;
}
namespace gfx {
class Rect;
}
namespace ui {
class InputMethod;
}

namespace keyboard {

// A proxy used by the KeyboardController to get access to the virtual
// keyboard window.
class KEYBOARD_EXPORT KeyboardControllerProxy {
 public:
  KeyboardControllerProxy();
  virtual ~KeyboardControllerProxy();

  // Gets the virtual keyboard window.  Ownership of the returned Window remains
  // with the proxy.
  virtual aura::Window* GetKeyboardWindow();

  // Gets the InputMethod that will provide notifications about changes in the
  // text input context.
  virtual ui::InputMethod* GetInputMethod() = 0;

  // Requests the audio input from microphone for speech input.
  virtual void RequestAudioInput(content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) = 0;

  // Shows the container window of the keyboard. The default implementation
  // simply shows the container. An overridden implementation can set up
  // necessary animation, or delay the visibility change as it desires.
  virtual void ShowKeyboardContainer(aura::Window* container);

  // Hides the container window of the keyboard. The default implementation
  // simply hides the container. An overridden implementation can set up
  // necesasry animation, or delay the visibility change as it desires.
  virtual void HideKeyboardContainer(aura::Window* container);

  // Updates the type of the focused text input box. The default implementation
  // calls OnTextInputBoxFocused javascript function through webui to update the
  // type the of focused input box.
  virtual void SetUpdateInputType(ui::TextInputType type);

 protected:
  // Gets the BrowserContext to use for creating the WebContents hosting the
  // keyboard.
  virtual content::BrowserContext* GetBrowserContext() = 0;

  // The implementation can choose to setup the WebContents before the virtual
  // keyboard page is loaded (e.g. install a WebContentsObserver).
  // SetupWebContents() is called right after creating the WebContents, before
  // loading the keyboard page.
  virtual void SetupWebContents(content::WebContents* contents);

 private:
  scoped_ptr<content::WebContents> keyboard_contents_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardControllerProxy);
};

}  // namespace keyboard

#endif  // UI_KEYBOARD_KEYBOARD_CONTROLLER_PROXY_H_
