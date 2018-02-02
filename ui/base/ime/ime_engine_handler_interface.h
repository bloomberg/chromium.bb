// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_BASE_IME_IME_ENGINE_HANDLER_INTERFACE_H_
#define UI_BASE_IME_IME_ENGINE_HANDLER_INTERFACE_H_

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

#include "base/callback.h"
#include "build/build_config.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/ime/ui_base_ime_export.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ui {

class KeyEvent;

// A interface to handle the engine handler method call.
class UI_BASE_IME_EXPORT IMEEngineHandlerInterface {
 public:
  typedef base::Callback<void(bool consumed)> KeyEventDoneCallback;

  // A information about a focused text input field.
  // A type of each member is based on the html spec, but InputContext can be
  // used to specify about a non html text field like Omnibox.
  struct InputContext {
    InputContext() {}
    InputContext(TextInputType type_, TextInputMode mode_, int flags_)
        : type(type_), mode(mode_), flags(flags_) {}
    InputContext(int id_, TextInputType type_, TextInputMode mode_, int flags_)
        : id(id_), type(type_), mode(mode_), flags(flags_) {}
    // An attribute of the context id which used for ChromeOS only.
    int id;
    // An attribute of the field defined at
    // http://www.w3.org/TR/html401/interact/forms.html#input-control-types.
    TextInputType type;
    // An attribute of the field defined at
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/
    //  association-of-controls-and-forms.html#input-modalities
    //  :-the-inputmode-attribute.
    TextInputMode mode;
    // An antribute to indicate the flags for web input fields. Please refer to
    // WebTextInputType.
    int flags;
  };

  virtual ~IMEEngineHandlerInterface() {}

  // Called when the Chrome input field get the focus.
  virtual void FocusIn(const InputContext& input_context) = 0;

  // Called when the Chrome input field lose the focus.
  virtual void FocusOut() = 0;

  // Called when the IME is enabled.
  virtual void Enable(const std::string& component_id) = 0;

  // Called when the IME is disabled.
  virtual void Disable() = 0;

  // Called when the IME is reset.
  virtual void Reset() = 0;

  // Called when the key event is received.
  // Actual implementation must call |callback| after key event handling.
  virtual void ProcessKeyEvent(const KeyEvent& key_event,
                               KeyEventDoneCallback& callback) = 0;

  // Called when a new surrounding text is set. The |text| is surrounding text
  // and |cursor_pos| is 0 based index of cursor position in |text|. If there is
  // selection range, |anchor_pos| represents opposite index from |cursor_pos|.
  // Otherwise |anchor_pos| is equal to |cursor_pos|. If not all surrounding
  // text is given |offset_pos| indicates the starting offset of |text|.
  virtual void SetSurroundingText(const std::string& text,
                                  uint32_t cursor_pos,
                                  uint32_t anchor_pos,
                                  uint32_t offset_pos) = 0;

  // Called when the composition bounds changed.
  virtual void SetCompositionBounds(const std::vector<gfx::Rect>& bounds) = 0;

  // Returns whether the engine is interested in key events.
  // If not, InputMethodChromeOS won't feed it with key events.
  virtual bool IsInterestedInKeyEvent() const = 0;

  // Clear the current composition.
  virtual bool ClearComposition(int context_id, std::string* error) = 0;

  // Commit the specified text to the specified context.  Fails if the context
  // is not focused.
  virtual bool CommitText(int context_id,
                          const char* text,
                          std::string* error) = 0;

  // Returns true if this IME is active, false if not.
  virtual bool IsActive() const = 0;

  // Returns the current active input_component id.
  virtual const std::string& GetActiveComponentId() const = 0;

  // Deletes |number_of_chars| unicode characters as the basis of |offset| from
  // the surrounding text. The |offset| is relative position based on current
  // caret.
  // NOTE: Currently we are falling back to backspace forwarding workaround,
  // because delete_surrounding_text is not supported in Chrome. So this
  // function is restricted for only preceding text.
  // TODO(nona): Support full spec delete surrounding text.
  virtual bool DeleteSurroundingText(int context_id,
                                     int offset,
                                     size_t number_of_chars,
                                     std::string* error) = 0;
#if defined(OS_CHROMEOS)

  // Called when a property is activated or changed.
  virtual void PropertyActivate(const std::string& property_name) = 0;

  // Called when the candidate in lookup table is clicked. The |index| is 0
  // based candidate index in lookup table.
  virtual void CandidateClicked(uint32_t index) = 0;

  // Show or hide the candidate window.
  virtual bool SetCandidateWindowVisible(bool visible, std::string* error) = 0;

  // Set the position of the cursor in the candidate window.
  virtual bool SetCursorPosition(int context_id,
                                 int candidate_id,
                                 std::string* error) = 0;
  // Hides the input view window (from API call).
  virtual void HideInputView() = 0;

#elif defined(OS_LINUX) || defined(OS_WIN)

  // Get the id of the IME extension.
  virtual std::string GetExtensionId() const = 0;
#endif  // defined(OS_CHROMEOS)
 protected:
  IMEEngineHandlerInterface() {}
};

}  // namespace ui

#endif  // UI_BASE_IME_IME_ENGINE_HANDLER_INTERFACE_H_
