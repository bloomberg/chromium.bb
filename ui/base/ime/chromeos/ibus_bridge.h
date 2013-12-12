// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
// TODO(nona): Rename this file to ime_bridge.h

#ifndef UI_BASE_IME_CHROMEOS_IBUS_BRIDGE_H_
#define UI_BASE_IME_CHROMEOS_IBUS_BRIDGE_H_

#include <string>
#include "base/basictypes.h"
#include "base/callback.h"
#include "ui/base/ime/text_input_mode.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/ui_export.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ui {
class KeyEvent;
}  // namespace ui

namespace chromeos {
namespace input_method {
class CandidateWindow;
}  // namespace input_method

class IBusText;

class UI_EXPORT IBusInputContextHandlerInterface {
 public:
  // Called when the engine commit a text.
  virtual void CommitText(const std::string& text) = 0;

  // Called when the engine update preedit stroing.
  virtual void UpdatePreeditText(const IBusText& text,
                                 uint32 cursor_pos,
                                 bool visible) = 0;

  // Called when the engine request deleting surrounding string.
  virtual void DeleteSurroundingText(int32 offset, uint32 length) = 0;
};


// A interface to handle the engine handler method call.
class UI_EXPORT IBusEngineHandlerInterface {
 public:
  typedef base::Callback<void (bool consumed)> KeyEventDoneCallback;

  // A information about a focused text input field.
  // A type of each member is based on the html spec, but InputContext can be
  // used to specify about a non html text field like Omnibox.
  struct InputContext {
    InputContext(ui::TextInputType type_, ui::TextInputMode mode_) :
      type(type_), mode(mode_) {}

    // An attribute of the field defined at
    // http://www.w3.org/TR/html401/interact/forms.html#input-control-types.
    ui::TextInputType type;
    // An attribute of the field defined at
    // http://www.whatwg.org/specs/web-apps/current-work/multipage/
    //  association-of-controls-and-forms.html#input-modalities
    //  :-the-inputmode-attribute.
    ui::TextInputMode mode;
  };

  virtual ~IBusEngineHandlerInterface() {}

  // Called when the Chrome input field get the focus.
  virtual void FocusIn(const InputContext& input_context) = 0;

  // Called when the Chrome input field lose the focus.
  virtual void FocusOut() = 0;

  // Called when the IME is enabled.
  virtual void Enable() = 0;

  // Called when the IME is disabled.
  virtual void Disable() = 0;

  // Called when a property is activated or changed.
  virtual void PropertyActivate(const std::string& property_name) = 0;

  // Called when the IME is reset.
  virtual void Reset() = 0;

  // Called when the key event is received.
  // Actual implementation must call |callback| after key event handling.
  virtual void ProcessKeyEvent(const ui::KeyEvent& key_event,
                               const KeyEventDoneCallback& callback) = 0;

  // Called when the candidate in lookup table is clicked. The |index| is 0
  // based candidate index in lookup table.
  virtual void CandidateClicked(uint32 index) = 0;

  // Called when a new surrounding text is set. The |text| is surrounding text
  // and |cursor_pos| is 0 based index of cursor position in |text|. If there is
  // selection range, |anchor_pos| represents opposite index from |cursor_pos|.
  // Otherwise |anchor_pos| is equal to |cursor_pos|.
  virtual void SetSurroundingText(const std::string& text, uint32 cursor_pos,
                                  uint32 anchor_pos) = 0;

 protected:
  IBusEngineHandlerInterface() {}
};

// A interface to handle the candidate window related method call.
class UI_EXPORT IBusPanelCandidateWindowHandlerInterface {
 public:
  virtual ~IBusPanelCandidateWindowHandlerInterface() {}

  // Called when the IME updates the lookup table.
  virtual void UpdateLookupTable(
      const input_method::CandidateWindow& candidate_window,
      bool visible) = 0;

  // Called when the IME updates the auxiliary text. The |text| is given in
  // UTF-8 encoding.
  virtual void UpdateAuxiliaryText(const std::string& text, bool visible) = 0;

  // Called when the IME updates the preedit text. The |text| is given in
  // UTF-8 encoding.
  virtual void UpdatePreeditText(const std::string& text, uint32 cursor_pos,
                                 bool visible) = 0;

  // Called when the application changes its caret bounds.
  virtual void SetCursorBounds(const gfx::Rect& cursor_bounds,
                               const gfx::Rect& composition_head) = 0;

  // Called when the text field's focus state is changed.
  // |is_focused| is true when the text field gains the focus.
  virtual void FocusStateChanged(bool is_focused) {}

 protected:
  IBusPanelCandidateWindowHandlerInterface() {}
};


// IBusBridge provides access of each IME related handler. This class is used
// for IME implementation without ibus-daemon. The legacy ibus IME communicates
// their engine with dbus protocol, but new implementation doesn't. Instead of
// dbus communcation, new implementation calls target service(e.g. PanelService
// or EngineService) directly by using this class.
class UI_EXPORT IBusBridge {
 public:
  virtual ~IBusBridge();

  // Allocates the global instance. Must be called before any calls to Get().
  static void Initialize();

  // Releases the global instance.
  static void Shutdown();

  // Returns IBusBridge global instance. Initialize() must be called first.
  static IBusBridge* Get();

  // Returns current InputContextHandler. This function returns NULL if input
  // context is not ready to use.
  virtual IBusInputContextHandlerInterface* GetInputContextHandler() const = 0;

  // Updates current InputContextHandler. If there is no active input context,
  // pass NULL for |handler|. Caller must release |handler|.
  virtual void SetInputContextHandler(
      IBusInputContextHandlerInterface* handler) = 0;

  // Initializes the mapping from |engine_id| to |handler|.
  // |engine_id| must not be empty and |handler| must not be null.
  virtual void SetEngineHandler(
      const std::string& engine_id,
      IBusEngineHandlerInterface* handler) = 0;

  // Returns IBusEngineHandlerInterface* mapped by |engine_id|.  NULL is
  // returned if |engine_id| is not mapped any engines.
  virtual IBusEngineHandlerInterface* GetEngineHandler(
      const std::string& engine_id) = 0;

  // Updates current EngineHandler. If there is no active engine service, pass
  // NULL for |handler|. Caller must release |handler|.
  virtual void SetCurrentEngineHandler(IBusEngineHandlerInterface* handler) = 0;

  // Updates current EngineHandler by Engine ID. If there is no active
  // engine service, pass an empty string for |engine_id|.  The set
  // IBusEngineHandlerInterface is returned.
  virtual IBusEngineHandlerInterface* SetCurrentEngineHandlerById(
      const std::string& engine_id) = 0;

  // Returns current EngineHandler. This function returns NULL if current engine
  // is not ready to use.
  virtual IBusEngineHandlerInterface* GetCurrentEngineHandler() const = 0;

  // Returns current CandidateWindowHandler. This function returns NULL if
  // current candidate window is not ready to use.
  virtual IBusPanelCandidateWindowHandlerInterface*
      GetCandidateWindowHandler() const = 0;

  // Updates current CandidatWindowHandler. If there is no active candidate
  // window service, pass NULL for |handler|. Caller must release |handler|.
  virtual void SetCandidateWindowHandler(
      IBusPanelCandidateWindowHandlerInterface* handler) = 0;

 protected:
  IBusBridge();

 private:
  DISALLOW_COPY_AND_ASSIGN(IBusBridge);
};

}  // namespace chromeos

#endif  // UI_BASE_IME_CHROMEOS_IBUS_BRIDGE_H_
