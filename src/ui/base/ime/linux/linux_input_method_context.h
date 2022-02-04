// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_LINUX_LINUX_INPUT_METHOD_CONTEXT_H_
#define UI_BASE_IME_LINUX_LINUX_INPUT_METHOD_CONTEXT_H_

#include <string>
#include <vector>

#include "base/component_export.h"
#include "ui/base/ime/text_input_type.h"

namespace gfx {
class Rect;
class Range;
}  // namespace gfx

namespace ui {

struct CompositionText;
class KeyEvent;
struct ImeTextSpan;
class VirtualKeyboardController;

// An interface of input method context for input method frameworks on
// GNU/Linux and likes.
class COMPONENT_EXPORT(UI_BASE_IME_LINUX) LinuxInputMethodContext {
 public:
  virtual ~LinuxInputMethodContext() = default;

  // Dispatches the key event to an underlying IME.  Returns true if the key
  // event is handled, otherwise false.  A client must set the text input type
  // before dispatching a key event.
  virtual bool DispatchKeyEvent(const ui::KeyEvent& key_event) = 0;

  // Returns whether the event is a peek key event.
  virtual bool IsPeekKeyEvent(const ui::KeyEvent& key_event) = 0;

  // Tells the system IME for the cursor rect which is relative to the
  // client window rect.
  virtual void SetCursorLocation(const gfx::Rect& rect) = 0;

  // Tells the system IME the surrounding text around the cursor location.
  virtual void SetSurroundingText(const std::u16string& text,
                                  const gfx::Range& selection_range) = 0;

  // Tells the system IME the content type of the text input client is changed.
  virtual void SetContentType(TextInputType input_type, int input_flags) = 0;

  // Resets the context.  A client needs to call OnTextInputTypeChanged() again
  // before calling DispatchKeyEvent().
  virtual void Reset() = 0;

  // Focuses the context.
  virtual void Focus() = 0;

  // Blurs the context.
  virtual void Blur() = 0;

  // Returns the corresponding VirtualKeyboardController instance.
  // Or nullptr, if not supported.
  virtual VirtualKeyboardController* GetVirtualKeyboardController() = 0;
};

// An interface of callback functions called from LinuxInputMethodContext.
class COMPONENT_EXPORT(UI_BASE_IME_LINUX) LinuxInputMethodContextDelegate {
 public:
  virtual ~LinuxInputMethodContextDelegate() {}

  // Commits the |text| to the text input client.
  virtual void OnCommit(const std::u16string& text) = 0;

  // Deletes the surrounding text around selection. |before| and |after|
  // are in UTF-16 code points.
  virtual void OnDeleteSurroundingText(size_t before, size_t after) = 0;

  // Sets the composition text to the text input client.
  virtual void OnPreeditChanged(const CompositionText& composition_text) = 0;

  // Cleans up a composition session and makes sure that the composition text is
  // cleared.
  virtual void OnPreeditEnd() = 0;

  // Prepares things for a new composition session.
  virtual void OnPreeditStart() = 0;

  // Sets the composition from the current text in the text input client.
  // |range| is in UTF-16 code range.
  virtual void OnSetPreeditRegion(const gfx::Range& range,
                                  const std::vector<ImeTextSpan>& spans) = 0;
};

}  // namespace ui

#endif  // UI_BASE_IME_LINUX_LINUX_INPUT_METHOD_CONTEXT_H_
