// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_H_
#define UI_VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/timer/timer.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/text_input_type.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/touch/touch_editing_controller.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "ui/gfx/font_list.h"
#include "ui/gfx/range/range.h"
#include "ui/gfx/selection_model.h"
#include "ui/gfx/text_constants.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/textfield/textfield_model.h"
#include "ui/views/drag_controller.h"
#include "ui/views/selection_controller.h"
#include "ui/views/selection_controller_delegate.h"
#include "ui/views/view.h"
#include "ui/views/word_lookup_client.h"

namespace views {

class MenuRunner;
class TextfieldController;

// A views/skia textfield implementation. No platform-specific code is used.
class VIEWS_EXPORT Textfield : public View,
                               public TextfieldModel::Delegate,
                               public ContextMenuController,
                               public DragController,
                               public WordLookupClient,
                               public SelectionControllerDelegate,
                               public ui::TouchEditable,
                               public ui::TextInputClient {
 public:
  // The textfield's class name.
  static const char kViewClassName[];

  // Returns the text cursor blink time in milliseconds, or 0 for no blinking.
  static size_t GetCaretBlinkMs();

  Textfield();
  ~Textfield() override;

  // Set the controller for this textfield.
  void set_controller(TextfieldController* controller) {
    controller_ = controller;
  }

  // Gets/Sets whether or not the Textfield is read-only.
  bool read_only() const { return read_only_; }
  void SetReadOnly(bool read_only);

  // Sets the input type; displays only asterisks for TEXT_INPUT_TYPE_PASSWORD.
  void SetTextInputType(ui::TextInputType type);

  // Sets the input flags so that the system input methods can turn on/off some
  // features. The flags is the bit map of ui::TextInputFlags.
  void SetTextInputFlags(int flags);

  // Gets the text for the Textfield. Call sites should take care to not reveal
  // the text for a password textfield.
  const base::string16& text() const { return model_->text(); }

  // Sets the text currently displayed in the Textfield.  This doesn't
  // change the cursor position if the current cursor is within the
  // new text's range, or moves the cursor to the end if the cursor is
  // out of the new text's range.
  void SetText(const base::string16& new_text);

  // Appends the given string to the previously-existing text in the field.
  void AppendText(const base::string16& new_text);

  // Inserts |new_text| at the cursor position, replacing any selected text.
  // This method is used to handle user input via paths Textfield doesn't
  // normally handle, so it calls UpdateAfterChange() and notifies observers of
  // changes.
  void InsertOrReplaceText(const base::string16& new_text);

  // Returns the text that is currently selected. Call sites should take care to
  // not reveal the text for a password textfield.
  base::string16 GetSelectedText() const;

  // Select the entire text range. If |reversed| is true, the range will end at
  // the logical beginning of the text; this generally shows the leading portion
  // of text that overflows its display area.
  void SelectAll(bool reversed);

  // A convenience method to select the word closest to |point|.
  void SelectWordAt(const gfx::Point& point);

  // Clears the selection within the edit field and sets the caret to the end.
  void ClearSelection();

  // Checks if there is any selected text.
  bool HasSelection() const;

  // Gets/sets the text color to be used when painting the Textfield.
  // Call UseDefaultTextColor() to restore the default system color.
  SkColor GetTextColor() const;
  void SetTextColor(SkColor color);
  void UseDefaultTextColor();

  // Gets/sets the background color to be used when painting the Textfield.
  // Call UseDefaultBackgroundColor() to restore the default system color.
  SkColor GetBackgroundColor() const;
  void SetBackgroundColor(SkColor color);
  void UseDefaultBackgroundColor();

  // Gets/sets the selection text color to be used when painting the Textfield.
  // Call UseDefaultSelectionTextColor() to restore the default system color.
  SkColor GetSelectionTextColor() const;
  void SetSelectionTextColor(SkColor color);
  void UseDefaultSelectionTextColor();

  // Gets/sets the selection background color to be used when painting the
  // Textfield. Call UseDefaultSelectionBackgroundColor() to restore the default
  // system color.
  SkColor GetSelectionBackgroundColor() const;
  void SetSelectionBackgroundColor(SkColor color);
  void UseDefaultSelectionBackgroundColor();

  // Gets/Sets whether or not the cursor is enabled.
  bool GetCursorEnabled() const;
  void SetCursorEnabled(bool enabled);

  // Gets/Sets the fonts used when rendering the text within the Textfield.
  const gfx::FontList& GetFontList() const;
  void SetFontList(const gfx::FontList& font_list);

  // Sets the default width of the text control. See default_width_in_chars_.
  void set_default_width_in_chars(int default_width) {
    default_width_in_chars_ = default_width;
  }

  // Sets the text to display when empty.
  void set_placeholder_text(const base::string16& text) {
    placeholder_text_ = text;
  }
  base::string16 GetPlaceholderText() const;

  void set_placeholder_text_color(SkColor color) {
    placeholder_text_color_ = color;
  }

  void set_placeholder_text_draw_flags(int flags) {
    placeholder_text_draw_flags_ = flags;
  }

  // Sets whether to indicate the textfield has invalid content.
  void SetInvalid(bool invalid);
  bool invalid() const { return invalid_; }

  // Get or set the horizontal alignment used for the button from the underlying
  // RenderText object.
  gfx::HorizontalAlignment GetHorizontalAlignment() const;
  void SetHorizontalAlignment(gfx::HorizontalAlignment alignment);

  // Displays a virtual keyboard or alternate input view if enabled.
  void ShowImeIfNeeded();

  // Returns whether or not an IME is composing text.
  bool IsIMEComposing() const;

  // Gets the selected logical text range.
  const gfx::Range& GetSelectedRange() const;

  // Selects the specified logical text range.
  void SelectRange(const gfx::Range& range);

  // Gets the text selection model.
  const gfx::SelectionModel& GetSelectionModel() const;

  // Sets the specified text selection model.
  void SelectSelectionModel(const gfx::SelectionModel& sel);

  // Returns the current cursor position.
  size_t GetCursorPosition() const;

  // Set the text color over the entire text or a logical character range.
  // Empty and invalid ranges are ignored.
  void SetColor(SkColor value);
  void ApplyColor(SkColor value, const gfx::Range& range);

  // Set various text styles over the entire text or a logical character range.
  // The respective |style| is applied if |value| is true, or removed if false.
  // Empty and invalid ranges are ignored.
  void SetStyle(gfx::TextStyle style, bool value);
  void ApplyStyle(gfx::TextStyle style, bool value, const gfx::Range& range);

  // Clears Edit history.
  void ClearEditHistory();

  // Set the accessible name of the text field.
  void SetAccessibleName(const base::string16& name);

  // View overrides:
  int GetBaseline() const override;
  gfx::Size CalculatePreferredSize() const override;
  const char* GetClassName() const override;
  void SetBorder(std::unique_ptr<Border> b) override;
  gfx::NativeCursor GetCursor(const ui::MouseEvent& event) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  bool OnMouseDragged(const ui::MouseEvent& event) override;
  void OnMouseReleased(const ui::MouseEvent& event) override;
  void OnMouseCaptureLost() override;
  bool OnMouseWheel(const ui::MouseWheelEvent& event) override;
  WordLookupClient* GetWordLookupClient() override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  bool AcceleratorPressed(const ui::Accelerator& accelerator) override;
  bool CanHandleAccelerators() const override;
  void AboutToRequestFocusFromTabTraversal(bool reverse) override;
  bool SkipDefaultKeyEventProcessing(const ui::KeyEvent& event) override;
  bool GetDropFormats(
      int* formats,
      std::set<ui::Clipboard::FormatType>* format_types) override;
  bool CanDrop(const ui::OSExchangeData& data) override;
  int OnDragUpdated(const ui::DropTargetEvent& event) override;
  void OnDragExited() override;
  int OnPerformDrop(const ui::DropTargetEvent& event) override;
  void OnDragDone() override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  bool HandleAccessibleAction(const ui::AXActionData& action_data) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  bool GetNeedsNotificationWhenVisibleBoundsChange() const override;
  void OnVisibleBoundsChanged() override;
  void OnEnabledChanged() override;
  void OnPaint(gfx::Canvas* canvas) override;
  void OnFocus() override;
  void OnBlur() override;
  gfx::Point GetKeyboardContextMenuLocation() override;
  void OnNativeThemeChanged(const ui::NativeTheme* theme) override;

  // TextfieldModel::Delegate overrides:
  void OnCompositionTextConfirmedOrCleared() override;

  // ContextMenuController overrides:
  void ShowContextMenuForView(View* source,
                              const gfx::Point& point,
                              ui::MenuSourceType source_type) override;

  // DragController overrides:
  void WriteDragDataForView(View* sender,
                            const gfx::Point& press_pt,
                            ui::OSExchangeData* data) override;
  int GetDragOperationsForView(View* sender, const gfx::Point& p) override;
  bool CanStartDragForView(View* sender,
                           const gfx::Point& press_pt,
                           const gfx::Point& p) override;

  // WordLookupClient overrides:
  bool GetDecoratedWordAtPoint(const gfx::Point& point,
                               gfx::DecoratedText* decorated_word,
                               gfx::Point* baseline_point) override;

  // SelectionControllerDelegate overrides:
  bool HasTextBeingDragged() const override;

  // ui::TouchEditable overrides:
  void SelectRect(const gfx::Point& start, const gfx::Point& end) override;
  void MoveCaretTo(const gfx::Point& point) override;
  void GetSelectionEndPoints(gfx::SelectionBound* anchor,
                             gfx::SelectionBound* focus) override;
  gfx::Rect GetBounds() override;
  gfx::NativeView GetNativeView() const override;
  void ConvertPointToScreen(gfx::Point* point) override;
  void ConvertPointFromScreen(gfx::Point* point) override;
  bool DrawsHandles() override;
  void OpenContextMenu(const gfx::Point& anchor) override;
  void DestroyTouchSelection() override;

  // ui::SimpleMenuModel::Delegate overrides:
  bool IsCommandIdChecked(int command_id) const override;
  bool IsCommandIdEnabled(int command_id) const override;
  bool GetAcceleratorForCommandId(int command_id,
                                  ui::Accelerator* accelerator) const override;
  void ExecuteCommand(int command_id, int event_flags) override;

  // ui::TextInputClient overrides:
  void SetCompositionText(const ui::CompositionText& composition) override;
  void ConfirmCompositionText() override;
  void ClearCompositionText() override;
  void InsertText(const base::string16& text) override;
  void InsertChar(const ui::KeyEvent& event) override;
  ui::TextInputType GetTextInputType() const override;
  ui::TextInputMode GetTextInputMode() const override;
  base::i18n::TextDirection GetTextDirection() const override;
  int GetTextInputFlags() const override;
  bool CanComposeInline() const override;
  gfx::Rect GetCaretBounds() const override;
  bool GetCompositionCharacterBounds(uint32_t index,
                                     gfx::Rect* rect) const override;
  bool HasCompositionText() const override;
  bool GetTextRange(gfx::Range* range) const override;
  bool GetCompositionTextRange(gfx::Range* range) const override;
  bool GetSelectionRange(gfx::Range* range) const override;
  bool SetSelectionRange(const gfx::Range& range) override;
  bool DeleteRange(const gfx::Range& range) override;
  bool GetTextFromRange(const gfx::Range& range,
                        base::string16* text) const override;
  void OnInputMethodChanged() override;
  bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) override;
  void ExtendSelectionAndDelete(size_t before, size_t after) override;
  void EnsureCaretNotInRect(const gfx::Rect& rect) override;
  bool IsTextEditCommandEnabled(ui::TextEditCommand command) const override;
  void SetTextEditCommandForNextKeyEvent(ui::TextEditCommand command) override;

 protected:
  // Inserts or appends a character in response to an IME operation.
  virtual void DoInsertChar(base::char16 ch);

  // Returns the TextfieldModel's text/cursor/selection rendering model.
  gfx::RenderText* GetRenderText() const;

  gfx::Point GetLastClickLocation() const;

  // Get the text from the selection clipboard.
  virtual base::string16 GetSelectionClipboardText() const;

  // Executes the given |command|.
  virtual void ExecuteTextEditCommand(ui::TextEditCommand command);

 private:
  friend class TextfieldTestApi;

  // View overrides:
  // Declared final since overriding by subclasses would interfere with the
  // accounting related to the scheduled text edit command. Subclasses should
  // use TextfieldController::HandleKeyEvent, to intercept the key event.
  bool OnKeyPressed(const ui::KeyEvent& event) final;
  bool OnKeyReleased(const ui::KeyEvent& event) final;

  // SelectionControllerDelegate overrides:
  gfx::RenderText* GetRenderTextForSelectionController() override;
  bool IsReadOnly() const override;
  bool SupportsDrag() const override;
  void SetTextBeingDragged(bool value) override;
  int GetViewHeight() const override;
  int GetViewWidth() const override;
  int GetDragSelectionDelay() const override;
  void OnBeforePointerAction() override;
  void OnAfterPointerAction(bool text_changed, bool selection_changed) override;
  // Callers within Textfield should call UpdateAfterChange depending on the
  // return value.
  bool PasteSelectionClipboard() override;
  void UpdateSelectionClipboard() override;

  // Updates the painted background color.
  void UpdateBackgroundColor();

  // Updates the border per the state of |invalid_|.
  void UpdateBorder();

  // Does necessary updates when the text and/or cursor position changes.
  void UpdateAfterChange(bool text_changed, bool cursor_changed);

  // Updates cursor visibility and blinks the cursor if needed.
  void ShowCursor();

  // A callback function to periodically update the cursor node_data.
  void UpdateCursorVisibility();

  // Update the cursor position in the text field.
  void UpdateCursorViewPosition();

  // Gets the style::TextStyle that should be used.
  int GetTextStyle() const;

  void PaintTextAndCursor(gfx::Canvas* canvas);

  // Helper function to call MoveCursorTo on the TextfieldModel.
  void MoveCursorTo(const gfx::Point& point, bool select);

  // Convenience method to notify the InputMethod and TouchSelectionController.
  void OnCaretBoundsChanged();

  // Convenience method to call TextfieldController::OnBeforeUserAction();
  void OnBeforeUserAction();

  // Convenience method to call TextfieldController::OnAfterUserAction();
  void OnAfterUserAction();

  // Calls |model_->Cut()| and notifies TextfieldController on success.
  bool Cut();

  // Calls |model_->Copy()| and notifies TextfieldController on success.
  bool Copy();

  // Calls |model_->Paste()| and calls TextfieldController::ContentsChanged()
  // explicitly if paste succeeded.
  bool Paste();

  // Utility function to prepare the context menu.
  void UpdateContextMenu();

  // Returns true if the current text input type allows access by the IME.
  bool ImeEditingAllowed() const;

  // Reveals the password character at |index| for a set duration.
  // If |index| is -1, the existing revealed character will be reset.
  void RevealPasswordChar(int index);

  void CreateTouchSelectionControllerAndNotifyIt();

  // Called when editing a textfield fails because the textfield is readonly.
  void OnEditFailed();

  // Returns true if an insertion cursor should be visible (a vertical bar,
  // placed at the point new text will be inserted).
  bool ShouldShowCursor() const;

  // Returns true if an insertion cursor should be visible and blinking.
  bool ShouldBlinkCursor() const;

  // Starts and stops blinking the cursor, respectively. These are both
  // idempotent if the cursor is already blinking/not blinking.
  void StartBlinkingCursor();
  void StopBlinkingCursor();

  // Callback for the cursor blink timer. Called every
  // Textfield::GetCaretBlinkMs().
  void OnCursorBlinkTimerFired();

  // The text model.
  std::unique_ptr<TextfieldModel> model_;

  // This is the current listener for events from this Textfield.
  TextfieldController* controller_;

  // An edit command to execute on the next key event. When set to a valid
  // value, the key event is still passed to |controller_|, but otherwise
  // ignored in favor of the edit command. Set via
  // SetTextEditCommandForNextKeyEvent() during dispatch of that key event (see
  // comment in TextInputClient).
  ui::TextEditCommand scheduled_text_edit_command_;

  // True if this Textfield cannot accept input and is read-only.
  bool read_only_;

  // The default number of average characters for the width of this text field.
  // This will be reported as the "desired size". Defaults to 0.
  int default_width_in_chars_;

  // Flags indicating whether various system colors should be used, and if not,
  // what overriding color values should be used instead.
  bool use_default_text_color_;
  bool use_default_background_color_;
  bool use_default_selection_text_color_;
  bool use_default_selection_background_color_;
  SkColor text_color_;
  SkColor background_color_;
  SkColor selection_text_color_;
  SkColor selection_background_color_;

  // Text to display when empty.
  base::string16 placeholder_text_;

  // Placeholder text color.
  // TODO(estade): remove this when Harmony/MD is default.
  SkColor placeholder_text_color_;

  // The draw flags specified for |placeholder_text_|.
  int placeholder_text_draw_flags_;

  // True when the contents are deemed unacceptable and should be indicated as
  // such.
  bool invalid_;

  // The accessible name of the text field.
  base::string16 accessible_name_;

  // The input type of this text field.
  ui::TextInputType text_input_type_;

  // The input flags of this text field.
  int text_input_flags_;

  // The timer to reveal the last typed password character.
  base::OneShotTimer password_reveal_timer_;

  // Tracks whether a user action is being performed which may change the
  // textfield; i.e. OnBeforeUserAction() has been called, but
  // OnAfterUserAction() has not yet been called.
  bool performing_user_action_;

  // True if InputMethod::CancelComposition() should not be called.
  bool skip_input_method_cancel_composition_;

  // Insertion cursor repaint timer and visibility.
  base::RepeatingTimer cursor_blink_timer_;

  // The drop cursor is a visual cue for where dragged text will be dropped.
  bool drop_cursor_visible_;
  gfx::SelectionModel drop_cursor_position_;

  // Is the user potentially dragging and dropping from this view?
  bool initiating_drag_;

  std::unique_ptr<ui::TouchEditingControllerDeprecated>
      touch_selection_controller_;

  SelectionController selection_controller_;

  // Used to track touch drag starting location and offset to enable touch
  // scrolling.
  gfx::Point drag_start_location_;
  int drag_start_display_offset_;

  // Tracks if touch editing handles are hidden because user has started
  // scrolling. If |true|, handles are shown after scrolling ends.
  bool touch_handles_hidden_due_to_scroll_;

  // True if this textfield should use a focus ring to indicate focus.
  bool use_focus_ring_;

  // Context menu related members.
  std::unique_ptr<ui::SimpleMenuModel> context_menu_contents_;
  std::unique_ptr<views::MenuRunner> context_menu_runner_;

  // View containing the text cursor.
  View cursor_view_;

  // Used to bind callback functions to this object.
  base::WeakPtrFactory<Textfield> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(Textfield);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TEXTFIELD_TEXTFIELD_H_
