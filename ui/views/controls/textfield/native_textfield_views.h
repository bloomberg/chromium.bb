// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TEXTFIELD_NATIVE_TEXTFIELD_VIEWS_H_
#define UI_VIEWS_CONTROLS_TEXTFIELD_NATIVE_TEXTFIELD_VIEWS_H_

#include "base/memory/weak_ptr.h"
#include "base/strings/string16.h"
#include "base/timer/timer.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/base/touch/touch_editing_controller.h"
#include "ui/events/event_constants.h"
#include "ui/gfx/selection_model.h"
#include "ui/views/border.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/textfield/textfield_views_model.h"
#include "ui/views/drag_controller.h"
#include "ui/views/view.h"

namespace base {
class Time;
}

namespace gfx {
class Canvas;
}

namespace views {

class FocusableBorder;
class MenuModelAdapter;
class MenuRunner;
class Textfield;

// A views/skia textfield implementation. No platform-specific code is used.
// TODO(msw): Merge views::NativeTextfieldViews and views::Textfield classes.
class VIEWS_EXPORT NativeTextfieldViews : public View,
                                          public ui::TouchEditable,
                                          public ContextMenuController,
                                          public DragController,
                                          public ui::TextInputClient,
                                          public TextfieldViewsModel::Delegate {
 public:
  explicit NativeTextfieldViews(Textfield* parent);
  virtual ~NativeTextfieldViews();

  // View overrides:
  virtual gfx::NativeCursor GetCursor(const ui::MouseEvent& event) OVERRIDE;
  virtual bool OnMousePressed(const ui::MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const ui::MouseEvent& event) OVERRIDE;
  virtual void OnGestureEvent(ui::GestureEvent* event) OVERRIDE;
  virtual bool OnKeyPressed(const ui::KeyEvent& event) OVERRIDE;
  virtual bool GetDropFormats(
      int* formats,
      std::set<ui::OSExchangeData::CustomFormat>* custom_formats) OVERRIDE;
  virtual bool CanDrop(const ui::OSExchangeData& data) OVERRIDE;
  virtual int OnDragUpdated(const ui::DropTargetEvent& event) OVERRIDE;
  virtual void OnDragExited() OVERRIDE;
  virtual int OnPerformDrop(const ui::DropTargetEvent& event) OVERRIDE;
  virtual void OnDragDone() OVERRIDE;
  virtual bool OnKeyReleased(const ui::KeyEvent& event) OVERRIDE;
  virtual ui::TextInputClient* GetTextInputClient() OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;
  virtual void OnNativeThemeChanged(const ui::NativeTheme* theme) OVERRIDE;

  // ui::TouchEditable overrides:
  virtual void SelectRect(const gfx::Point& start,
                          const gfx::Point& end) OVERRIDE;
  virtual void MoveCaretTo(const gfx::Point& point) OVERRIDE;
  virtual void GetSelectionEndPoints(gfx::Rect* p1, gfx::Rect* p2) OVERRIDE;
  virtual gfx::Rect GetBounds() OVERRIDE;
  virtual gfx::NativeView GetNativeView() OVERRIDE;
  virtual void ConvertPointToScreen(gfx::Point* point) OVERRIDE;
  virtual void ConvertPointFromScreen(gfx::Point* point) OVERRIDE;
  virtual bool DrawsHandles() OVERRIDE;
  virtual void OpenContextMenu(const gfx::Point& anchor) OVERRIDE;

  // ContextMenuController overrides:
  virtual void ShowContextMenuForView(View* source,
                                      const gfx::Point& point,
                                      ui::MenuSourceType source_type) OVERRIDE;

  // Overridden from DragController:
  virtual void WriteDragDataForView(View* sender,
                                    const gfx::Point& press_pt,
                                    ui::OSExchangeData* data) OVERRIDE;
  virtual int GetDragOperationsForView(View* sender,
                                       const gfx::Point& p) OVERRIDE;
  virtual bool CanStartDragForView(View* sender,
                                   const gfx::Point& press_pt,
                                   const gfx::Point& p) OVERRIDE;

  // Gets the currently displayed text.
  base::string16 GetText() const;

  // Updates the text displayed to the text held by the parent Textfield.
  void UpdateText();

  // Adds the specified text to the text already displayed.
  void AppendText(const base::string16& text);

  // Inserts |text| at the current cursor position, replacing any selected text.
  void InsertOrReplaceText(const base::string16& text);

  // Returns the text direction.
  base::i18n::TextDirection GetTextDirection() const;

  // Returns the currently selected text.
  base::string16 GetSelectedText() const;

  // Select the entire text range. If |reversed| is true, the range will end at
  // the logical beginning of the text; this generally shows the leading portion
  // of text that overflows its display area.
  void SelectAll(bool reversed);

  // Clears the selection within the textfield and sets the caret to the end.
  void ClearSelection();

  // Updates whether there is a visible border.
  void UpdateBorder();

  // Updates the painted text color.
  void UpdateTextColor();

  // Updates the painted background color.
  void UpdateBackgroundColor();

  // Updates the read-only state.
  void UpdateReadOnly();

  // Updates the font used to render the text.
  void UpdateFont();

  // Updates the obscured state of the text for passwords, etc.
  void UpdateIsObscured();

  // Updates the enabled state.
  void UpdateEnabled();

  // Updates the horizontal and vertical margins.
  void UpdateHorizontalMargins();
  void UpdateVerticalMargins();

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

  // Get or set whether or not the cursor is enabled.
  bool GetCursorEnabled() const;
  void SetCursorEnabled(bool enabled);

  // Invoked when the parent views::Textfield receives key events.
  // returns true if the event was processed.
  bool HandleKeyPressed(const ui::KeyEvent& e);
  bool HandleKeyReleased(const ui::KeyEvent& e);

  // Invoked when the parent views:Textfield gains or loses focus.
  void HandleFocus();
  void HandleBlur();

  // Set the text colors; see views::Textfield for details.
  void SetColor(SkColor value);
  void ApplyColor(SkColor value, const gfx::Range& range);

  // Set the text styles; see the corresponding Textfield functions for details.
  void SetStyle(gfx::TextStyle style, bool value);
  void ApplyStyle(gfx::TextStyle style, bool value, const gfx::Range& range);

  // Clears the Edit history.
  void ClearEditHistory();

  // Get the height in pixels of the fonts used.
  int GetFontHeight();

  // Returns the text baseline; this value does not include any insets.
  int GetTextfieldBaseline() const;

  // Returns the width necessary to display the current text, including any
  // necessary space for the cursor or border/margin.
  int GetWidthNeededForText() const;

  // Returns whether this view is the origin of an ongoing drag operation.
  bool HasTextBeingDragged();

  // Returns the location for keyboard-triggered context menus.
  gfx::Point GetContextMenuLocation();

  // ui::SimpleMenuModel::Delegate overrides
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual bool IsItemForCommandIdDynamic(int command_id) const OVERRIDE;
  virtual base::string16 GetLabelForCommandId(int command_id) const OVERRIDE;
  virtual void ExecuteCommand(int command_id, int event_flags) OVERRIDE;

  // class name of internal
  static const char kViewClassName[];

 protected:
  // View override.
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;

 private:
  friend class NativeTextfieldViewsTest;
  friend class TouchSelectionControllerImplTest;

  // Overridden from ui::TextInputClient:
  virtual void SetCompositionText(
      const ui::CompositionText& composition) OVERRIDE;
  virtual void ConfirmCompositionText() OVERRIDE;
  virtual void ClearCompositionText() OVERRIDE;
  virtual void InsertText(const base::string16& text) OVERRIDE;
  virtual void InsertChar(char16 ch, int flags) OVERRIDE;
  virtual gfx::NativeWindow GetAttachedWindow() const OVERRIDE;
  virtual ui::TextInputType GetTextInputType() const OVERRIDE;
  virtual ui::TextInputMode GetTextInputMode() const OVERRIDE;
  virtual bool CanComposeInline() const OVERRIDE;
  virtual gfx::Rect GetCaretBounds() const OVERRIDE;
  virtual bool GetCompositionCharacterBounds(uint32 index,
                                             gfx::Rect* rect) const OVERRIDE;
  virtual bool HasCompositionText() const OVERRIDE;
  virtual bool GetTextRange(gfx::Range* range) const OVERRIDE;
  virtual bool GetCompositionTextRange(gfx::Range* range) const OVERRIDE;
  virtual bool GetSelectionRange(gfx::Range* range) const OVERRIDE;
  virtual bool SetSelectionRange(const gfx::Range& range) OVERRIDE;
  virtual bool DeleteRange(const gfx::Range& range) OVERRIDE;
  virtual bool GetTextFromRange(const gfx::Range& range,
                                base::string16* text) const OVERRIDE;
  virtual void OnInputMethodChanged() OVERRIDE;
  virtual bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) OVERRIDE;
  virtual void ExtendSelectionAndDelete(size_t before, size_t after) OVERRIDE;
  virtual void EnsureCaretInRect(const gfx::Rect& rect) OVERRIDE;
  virtual void OnCandidateWindowShown() OVERRIDE;
  virtual void OnCandidateWindowUpdated() OVERRIDE;
  virtual void OnCandidateWindowHidden() OVERRIDE;

  // Overridden from TextfieldViewsModel::Delegate:
  virtual void OnCompositionTextConfirmedOrCleared() OVERRIDE;

  // Returns the TextfieldViewsModel's text/cursor/selection rendering model.
  gfx::RenderText* GetRenderText() const;

  // Converts |text| according to the current style, e.g. STYLE_LOWERCASE.
  base::string16 GetTextForDisplay(const base::string16& text);

  // Updates any colors that have not been explicitly set from the theme.
  void UpdateColorsFromTheme(const ui::NativeTheme* theme);

  // A callback function to periodically update the cursor state.
  void UpdateCursor();

  // Repaint the cursor.
  void RepaintCursor();

  // Update the cursor_bounds and text_offset.
  void UpdateCursorBoundsAndTextOffset(size_t cursor_pos, bool insert_mode);

  void PaintTextAndCursor(gfx::Canvas* canvas);

  // Handle the keyevent.
  bool HandleKeyEvent(const ui::KeyEvent& key_event);

  // Helper function to call MoveCursorTo on the TextfieldViewsModel.
  bool MoveCursorTo(const gfx::Point& point, bool select);

  // Utility function to inform the parent views::Textfield (and any controller)
  // that the text in the textfield has changed.
  void PropagateTextChange();

  // Does necessary updates when the text and/or cursor position changes.
  void UpdateAfterChange(bool text_changed, bool cursor_changed);

  // Utility function to prepare the context menu.
  void UpdateContextMenu();

  // Convenience method to call InputMethod::OnTextInputTypeChanged();
  void OnTextInputTypeChanged();

  // Convenience method to call InputMethod::OnCaretBoundsChanged();
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

  // Tracks the mouse clicks for single/double/triple clicks.
  void TrackMouseClicks(const ui::MouseEvent& event);

  // Handles mouse press events.
  void HandleMousePressEvent(const ui::MouseEvent& event);

  // Returns true if the current text input type allows access by the IME.
  bool ImeEditingAllowed() const;

  // Returns true if distance between |event| and |last_click_location_|
  // exceeds the drag threshold.
  bool ExceededDragThresholdFromLastClickLocation(const ui::MouseEvent& event);

  // Checks if a char is ok to be inserted into the textfield. The |ch| is a
  // modified character, i.e., modifiers took effect when generating this char.
  static bool ShouldInsertChar(char16 ch, int flags);

  void CreateTouchSelectionControllerAndNotifyIt();

  // Platform specific gesture event handling.
  void PlatformGestureEventHandling(const ui::GestureEvent* event);

  // Reveals the obscured char at |index| for the given |duration|. If |index|
  // is -1, existing revealed index will be cleared.
  void RevealObscuredChar(int index, const base::TimeDelta& duration);

  // The parent views::Textfield, the owner of this object.
  Textfield* textfield_;

  // The text model.
  scoped_ptr<TextfieldViewsModel> model_;

  // The focusable border.  This is always non-NULL, but may not actually be
  // drawn.  If it is not drawn, then by default it's also zero-sized unless the
  // Textfield has explicitly-set margins.
  FocusableBorder* text_border_;

  // The text editing cursor visibility.
  bool is_cursor_visible_;

  // The drop cursor is a visual cue for where dragged text will be dropped.
  bool is_drop_cursor_visible_;
  // Position of the drop cursor, if it is visible.
  gfx::SelectionModel drop_cursor_position_;

  // True if InputMethod::CancelComposition() should not be called.
  bool skip_input_method_cancel_composition_;

  // Is the user potentially dragging and dropping from this view?
  bool initiating_drag_;

  // A runnable method factory for callback to update the cursor.
  base::WeakPtrFactory<NativeTextfieldViews> cursor_timer_;

  // State variables used to track double and triple clicks.
  size_t aggregated_clicks_;
  base::TimeDelta last_click_time_;
  gfx::Point last_click_location_;
  gfx::Range double_click_word_;

  // Context menu related members.
  scoped_ptr<ui::SimpleMenuModel> context_menu_contents_;
  scoped_ptr<views::MenuModelAdapter> context_menu_delegate_;
  scoped_ptr<views::MenuRunner> context_menu_runner_;

  scoped_ptr<ui::TouchSelectionController> touch_selection_controller_;

  // A timer to control the duration of showing the last typed char in
  // obscured text. When the timer is running, the last typed char is shown
  // and when the time expires, the last typed char is obscured.
  base::OneShotTimer<NativeTextfieldViews> obscured_reveal_timer_;

  DISALLOW_COPY_AND_ASSIGN(NativeTextfieldViews);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TEXTFIELD_NATIVE_TEXTFIELD_VIEWS_H_
