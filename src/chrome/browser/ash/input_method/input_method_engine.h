// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_ENGINE_H_
#define CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_ENGINE_H_

#include <stddef.h>
#include <stdint.h>

#include <map>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "base/values.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/ash/input_method/assistive_window_properties.h"
#include "chrome/browser/ash/input_method/input_method_engine_observer.h"
#include "chrome/browser/ash/input_method/suggestion_handler_interface.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_observer.h"
#include "components/prefs/pref_change_registrar.h"
#include "ui/base/ime/ash/ime_engine_handler_interface.h"
#include "ui/base/ime/ash/input_method_descriptor.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/candidate_window.h"
#include "ui/base/ime/composition_text.h"
#include "ui/events/event.h"

static_assert(BUILDFLAG(IS_CHROMEOS_ASH), "For ChromeOS ash-chrome only");

namespace ui {
struct CompositionText;
class IMEEngineHandlerInterface;
class KeyEvent;

namespace ime {
struct AssistiveWindowButton;
struct InputMethodMenuItem;
struct SuggestionDetails;
}  // namespace ime
}  // namespace ui

namespace ash {
namespace input_method {

struct AssistiveWindowProperties;

class InputMethodEngine : virtual public ui::IMEEngineHandlerInterface,
                          public ProfileObserver,
                          public SuggestionHandlerInterface {
 public:
  enum {
    MENU_ITEM_MODIFIED_LABEL = 0x0001,
    MENU_ITEM_MODIFIED_STYLE = 0x0002,
    MENU_ITEM_MODIFIED_CHECKED = 0x0010,
  };

  enum SegmentStyle {
    SEGMENT_STYLE_UNDERLINE,
    SEGMENT_STYLE_DOUBLE_UNDERLINE,
    SEGMENT_STYLE_NO_UNDERLINE,
  };

  struct SegmentInfo {
    int start;
    int end;
    SegmentStyle style;
  };

  struct UsageEntry {
    std::string title;
    std::string body;
  };

  struct Candidate {
    Candidate();
    Candidate(const Candidate& other);
    virtual ~Candidate();

    std::string value;
    int id;
    std::string label;
    std::string annotation;
    UsageEntry usage;
    std::vector<Candidate> candidates;
  };

  struct CandidateWindowProperty {
    CandidateWindowProperty();
    virtual ~CandidateWindowProperty();
    CandidateWindowProperty(const CandidateWindowProperty& other);
    int page_size;
    bool is_cursor_visible;
    bool is_vertical;
    bool show_window_at_composition;

    // Auxiliary text is typically displayed in the footer of the candidate
    // window.
    std::string auxiliary_text;
    bool is_auxiliary_text_visible;

    // The index of the current chosen candidate out of total candidates.
    // value is -1 if there is no current chosen candidate.
    int current_candidate_index = -1;
    int total_candidates = 0;
  };

  InputMethodEngine();
  InputMethodEngine(const InputMethodEngine&) = delete;
  InputMethodEngine& operator=(const InputMethodEngine&) = delete;
  ~InputMethodEngine() override;

  virtual void Initialize(std::unique_ptr<InputMethodEngineObserver> observer,
                          const char* extension_id,
                          Profile* profile);

  // Returns true if this IME is active, false if not.
  bool IsActive() const;

  // Returns the current active input_component id.
  const std::string& GetActiveComponentId() const;

  // Clear the current composition.
  bool ClearComposition(int context_id, std::string* error);

  // Commit the specified text to the specified context.  Fails if the context
  // is not focused.
  bool CommitText(int context_id,
                  const std::u16string& text,
                  std::string* error);

  // Notifies InputContextHandler to commit any composition text.
  // Set |reset_engine| to false if the event was from the extension.
  void ConfirmCompositionText(bool reset_engine, bool keep_selection);

  // Deletes |number_of_chars| unicode characters as the basis of |offset| from
  // the surrounding text. The |offset| is relative position based on current
  // caret.
  // NOTE: Currently we are falling back to backspace forwarding workaround,
  // because delete_surrounding_text is not supported in Chrome. So this
  // function is restricted for only preceding text.
  // TODO(nona): Support full spec delete surrounding text.
  bool DeleteSurroundingText(int context_id,
                             int offset,
                             size_t number_of_chars,
                             std::string* error);

  // Commit the text currently being composed to the composition.
  // Fails if the context is not focused.
  bool FinishComposingText(int context_id, std::string* error);

  // Send the sequence of key events.
  bool SendKeyEvents(int context_id,
                     const std::vector<ui::KeyEvent>& events,
                     std::string* error);

  // Set the current composition and associated properties.
  bool SetComposition(int context_id,
                      const char* text,
                      int selection_start,
                      int selection_end,
                      int cursor,
                      const std::vector<SegmentInfo>& segments,
                      std::string* error);

  // Set the current composition range around the current cursor.
  // This function is deprecated. Use |SetComposingRange| instead.
  bool SetCompositionRange(int context_id,
                           int selection_before,
                           int selection_after,
                           const std::vector<SegmentInfo>& segments,
                           std::string* error);

  // Set the current composition range.
  bool SetComposingRange(int context_id,
                         int start,
                         int end,
                         const std::vector<SegmentInfo>& segments,
                         std::string* error);

  gfx::Range GetAutocorrectRange(int context_id, std::string* error);

  gfx::Rect GetAutocorrectCharacterBounds(int context_id, std::string* error);

  gfx::Rect GetTextFieldBounds(int context_id, std::string* error);

  bool SetAutocorrectRange(int context_id,
                           const gfx::Range& range,
                           std::string* error);

  // Set the current selection range.
  bool SetSelectionRange(int context_id,
                         int start,
                         int end,
                         std::string* error);

  // Called when a key event is handled.
  void KeyEventHandled(const std::string& extension_id,
                       const std::string& request_id,
                       bool handled);

  // Returns the request ID for this key event.
  std::string AddPendingKeyEvent(
      const std::string& component_id,
      ui::IMEEngineHandlerInterface::KeyEventDoneCallback callback);

  // Resolves all the pending key event callbacks as not handled.
  void CancelPendingKeyEvents();

  // Get the composition bounds.
  const std::vector<gfx::Rect>& composition_bounds() const {
    return composition_bounds_;
  }

  int GetContextIdForTesting() const { return context_id_; }

  PrefChangeRegistrar* GetPrefChangeRegistrarForTesting() const {
    return pref_change_registrar_.get();
  }

  // ui::IMEEngineHandlerInterface overrides.
  void FocusIn(const ui::IMEEngineHandlerInterface::InputContext& input_context)
      override;
  void FocusOut() override;
  void OnTouch(ui::EventPointerType pointerType) override;
  void Enable(const std::string& component_id) override;
  void Disable() override;
  void Reset() override;
  void ProcessKeyEvent(const ui::KeyEvent& key_event,
                       KeyEventDoneCallback callback) override;
  void SetSurroundingText(const std::u16string& text,
                          uint32_t cursor_pos,
                          uint32_t anchor_pos,
                          uint32_t offset_pos) override;
  void SetCompositionBounds(const std::vector<gfx::Rect>& bounds) override;
  void PropertyActivate(const std::string& property_name) override;
  void CandidateClicked(uint32_t index) override;
  void AssistiveWindowButtonClicked(
      const ui::ime::AssistiveWindowButton& button) override;
  void SetMirroringEnabled(bool mirroring_enabled) override;
  void SetCastingEnabled(bool casting_enabled) override;
  ui::VirtualKeyboardController* GetVirtualKeyboardController() const override;

  // SuggestionHandlerInterface overrides.
  bool DismissSuggestion(int context_id, std::string* error) override;
  bool SetSuggestion(int context_id,
                     const ui::ime::SuggestionDetails& details,
                     std::string* error) override;
  bool AcceptSuggestion(int context_id, std::string* error) override;
  void OnSuggestionsChanged(
      const std::vector<std::string>& suggestions) override;
  bool SetButtonHighlighted(int context_id,
                            const ui::ime::AssistiveWindowButton& button,
                            bool highlighted,
                            std::string* error) override;
  void ClickButton(const ui::ime::AssistiveWindowButton& button) override;
  bool AcceptSuggestionCandidate(int context_id,
                                 const std::u16string& candidate,
                                 std::string* error) override;
  bool SetAssistiveWindowProperties(
      int context_id,
      const AssistiveWindowProperties& assistive_window,
      std::string* error) override;
  void Announce(const std::u16string& message) override;

  // ProfileObserver overrides.
  void OnProfileWillBeDestroyed(Profile* profile) override;

  // This function returns the current property of the candidate window of the
  // corresponding engine_id. If the CandidateWindowProperty is not set for the
  // engine_id, a default value is set. The caller can use the returned value as
  // the default property and modify some of specified items.
  const CandidateWindowProperty& GetCandidateWindowProperty(
      const std::string& engine_id);

  // Changes the property of the candidate window of the given engine_id and
  // repaints the candidate window widget.
  void SetCandidateWindowProperty(const std::string& engine_id,
                                  const CandidateWindowProperty& property);

  // Show or hide the candidate window.
  bool SetCandidateWindowVisible(bool visible, std::string* error);

  // Set the list of entries displayed in the candidate window.
  bool SetCandidates(int context_id,
                     const std::vector<Candidate>& candidates,
                     std::string* error);

  // Set the position of the cursor in the candidate window.
  bool SetCursorPosition(int context_id, int candidate_id, std::string* error);

  // Set the list of items that appears in the language menu when this IME is
  // active.
  bool SetMenuItems(const std::vector<InputMethodManager::MenuItem>& items,
                    std::string* error);

  // Update the state of the menu items.
  bool UpdateMenuItems(const std::vector<InputMethodManager::MenuItem>& items,
                       std::string* error);

  // Hides the input view window (from API call).
  void HideInputView();

  // Notifies the InputContextHandler that the autocorrect range should
  // be updated and the autocorrect text has updated.
  // Sets the autocorrect range to be `range`. The `range` is in bytes.
  // TODO(b/171924748): Improve documentation for this function all the way down
  // the stack.
  bool SetAutocorrectRange(const gfx::Range& range);

  gfx::Range GetAutocorrectRange();

 protected:
  virtual void OnInputMethodOptionsChanged();

  // The observer object receiving events for this IME.
  std::unique_ptr<InputMethodEngineObserver> observer_;

 private:
  struct PendingKeyEvent {
    PendingKeyEvent(
        const std::string& component_id,
        ui::IMEEngineHandlerInterface::KeyEventDoneCallback callback);
    PendingKeyEvent(PendingKeyEvent&& other);

    PendingKeyEvent(const PendingKeyEvent&) = delete;
    PendingKeyEvent& operator=(const PendingKeyEvent&) = delete;

    ~PendingKeyEvent();

    std::string component_id;
    ui::IMEEngineHandlerInterface::KeyEventDoneCallback callback;
  };

  // Notifies InputContextHandler that the composition is changed.
  void UpdateComposition(const ui::CompositionText& composition_text,
                         uint32_t cursor_pos,
                         bool is_visible);

  // Notifies InputContextHandler to commit |text|.
  void CommitTextToInputContext(int context_id, const std::u16string& text);

  // Converts MenuItem to InputMethodMenuItem.
  void MenuItemToProperty(const InputMethodManager::MenuItem& item,
                          ui::ime::InputMethodMenuItem* property);

  // The current candidate window.
  ui::CandidateWindow candidate_window_;

  // The candidate window property of the current engine_id.
  std::pair<std::string, CandidateWindowProperty> candidate_window_property_;

  // Indicates whether the candidate window is visible.
  bool window_visible_ = false;

  // Mapping of candidate index to candidate id.
  std::vector<int> candidate_ids_;

  // Mapping of candidate id to index.
  std::map<int, int> candidate_indexes_;

  // Whether the screen is in mirroring mode.
  bool is_mirroring_ = false;

  // Whether the desktop is being casted.
  bool is_casting_ = false;

  ui::TextInputType current_input_type_;

  // ID that is used for the current input context.  False if there is no focus.
  int context_id_;

  // Next id that will be assigned to a context.
  int next_context_id_;

  // The input_component ID in IME extension's manifest.
  std::string active_component_id_;

  // The IME extension ID.
  std::string extension_id_;

  raw_ptr<Profile> profile_;

  unsigned int next_request_id_ = 1;

  std::map<std::string, PendingKeyEvent> pending_key_events_;

  // The composition text to be set from calling input.ime.setComposition API.
  ui::CompositionText composition_;

  bool composition_changed_;

  // The composition bounds returned by inputMethodPrivate.getCompositionBounds
  // API.
  std::vector<gfx::Rect> composition_bounds_;

  // The text to be committed from calling input.ime.commitText API.
  std::u16string text_;

  bool commit_text_changed_;

  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  base::Value input_method_settings_snapshot_;

  base::ScopedObservation<Profile, ProfileObserver> profile_observation_{this};
};

}  // namespace input_method
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_INPUT_METHOD_INPUT_METHOD_ENGINE_H_
