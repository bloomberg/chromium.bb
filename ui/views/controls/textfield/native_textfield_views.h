// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TEXTFIELD_NATIVE_TEXTFIELD_VIEWS_H_
#define UI_VIEWS_CONTROLS_TEXTFIELD_NATIVE_TEXTFIELD_VIEWS_H_
#pragma once

#include "base/memory/weak_ptr.h"
#include "base/string16.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/font.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/textfield/native_textfield_wrapper.h"
#include "ui/views/controls/textfield/textfield_views_model.h"
#include "ui/views/drag_controller.h"
#include "ui/views/touchui/touch_selection_controller.h"
#include "views/border.h"
#include "views/view.h"

namespace base {
class Time;
}

namespace gfx {
class Canvas;
}

namespace views {

class FocusableBorder;
class KeyEvent;
class MenuModelAdapter;
class MenuRunner;

// A views/skia only implementation of NativeTextfieldWrapper.
// No platform specific code is used.
// Following features are not yet supported.
// * BIDI/Complex script.
// * Support surrogate pair, or maybe we should just use UTF32 internally.
// * X selection (only if we want to support).
// Once completed, this will replace Textfield, NativeTextfieldWin and
// NativeTextfieldGtk.
class VIEWS_EXPORT NativeTextfieldViews : public TouchSelectionClientView,
                                          public ContextMenuController,
                                          public DragController,
                                          public NativeTextfieldWrapper,
                                          public ui::TextInputClient,
                                          public TextfieldViewsModel::Delegate {
 public:
  explicit NativeTextfieldViews(Textfield* parent);
  virtual ~NativeTextfieldViews();

  // View overrides:
  virtual gfx::NativeCursor GetCursor(const MouseEvent& event) OVERRIDE;
  virtual bool OnMousePressed(const MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const MouseEvent& event) OVERRIDE;
  virtual bool OnKeyPressed(const KeyEvent& event) OVERRIDE;
  virtual bool GetDropFormats(
      int* formats,
      std::set<ui::OSExchangeData::CustomFormat>* custom_formats) OVERRIDE;
  virtual bool CanDrop(const ui::OSExchangeData& data) OVERRIDE;
  virtual int OnDragUpdated(const DropTargetEvent& event) OVERRIDE;
  virtual int OnPerformDrop(const DropTargetEvent& event) OVERRIDE;
  virtual void OnDragDone() OVERRIDE;
  virtual bool OnKeyReleased(const KeyEvent& event) OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;

  // TouchSelectionClientView overrides:
  virtual void SelectRect(const gfx::Point& start,
                          const gfx::Point& end) OVERRIDE;

  // ContextMenuController overrides:
  virtual void ShowContextMenuForView(View* source,
                                      const gfx::Point& p,
                                      bool is_mouse_gesture) OVERRIDE;

  // Overridden from DragController:
  virtual void WriteDragDataForView(View* sender,
                                    const gfx::Point& press_pt,
                                    ui::OSExchangeData* data) OVERRIDE;
  virtual int GetDragOperationsForView(View* sender,
                                       const gfx::Point& p) OVERRIDE;
  virtual bool CanStartDragForView(View* sender,
                                   const gfx::Point& press_pt,
                                   const gfx::Point& p) OVERRIDE;

  // NativeTextfieldWrapper overrides:
  virtual string16 GetText() const OVERRIDE;
  virtual void UpdateText() OVERRIDE;
  virtual void AppendText(const string16& text) OVERRIDE;
  virtual string16 GetSelectedText() const OVERRIDE;
  virtual void SelectAll() OVERRIDE;
  virtual void ClearSelection() OVERRIDE;
  virtual void UpdateBorder() OVERRIDE;
  virtual void UpdateTextColor() OVERRIDE;
  virtual void UpdateBackgroundColor() OVERRIDE;
  virtual void UpdateReadOnly() OVERRIDE;
  virtual void UpdateFont() OVERRIDE;
  virtual void UpdateIsPassword() OVERRIDE;
  virtual void UpdateEnabled() OVERRIDE;
  virtual gfx::Insets CalculateInsets() OVERRIDE;
  virtual void UpdateHorizontalMargins() OVERRIDE;
  virtual void UpdateVerticalMargins() OVERRIDE;
  virtual bool SetFocus() OVERRIDE;
  virtual View* GetView() OVERRIDE;
  virtual gfx::NativeView GetTestingHandle() const OVERRIDE;
  virtual bool IsIMEComposing() const OVERRIDE;
  virtual void GetSelectedRange(ui::Range* range) const OVERRIDE;
  virtual void SelectRange(const ui::Range& range) OVERRIDE;
  virtual void GetSelectionModel(gfx::SelectionModel* sel) const OVERRIDE;
  virtual void SelectSelectionModel(const gfx::SelectionModel& sel) OVERRIDE;
  virtual size_t GetCursorPosition() const OVERRIDE;
  virtual bool HandleKeyPressed(const KeyEvent& e) OVERRIDE;
  virtual bool HandleKeyReleased(const KeyEvent& e) OVERRIDE;
  virtual void HandleFocus() OVERRIDE;
  virtual void HandleBlur() OVERRIDE;
  virtual ui::TextInputClient* GetTextInputClient() OVERRIDE;
  virtual void ApplyStyleRange(const gfx::StyleRange& style) OVERRIDE;
  virtual void ApplyDefaultStyle() OVERRIDE;
  virtual void ClearEditHistory() OVERRIDE;

  // ui::SimpleMenuModel::Delegate overrides
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;

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
  virtual void InsertText(const string16& text) OVERRIDE;
  virtual void InsertChar(char16 ch, int flags) OVERRIDE;
  virtual ui::TextInputType GetTextInputType() const OVERRIDE;
  virtual gfx::Rect GetCaretBounds() OVERRIDE;
  virtual bool HasCompositionText() OVERRIDE;
  virtual bool GetTextRange(ui::Range* range) OVERRIDE;
  virtual bool GetCompositionTextRange(ui::Range* range) OVERRIDE;
  virtual bool GetSelectionRange(ui::Range* range) OVERRIDE;
  virtual bool SetSelectionRange(const ui::Range& range) OVERRIDE;
  virtual bool DeleteRange(const ui::Range& range) OVERRIDE;
  virtual bool GetTextFromRange(const ui::Range& range,
                                string16* text) OVERRIDE;
  virtual void OnInputMethodChanged() OVERRIDE;
  virtual bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) OVERRIDE;

  // Overridden from TextfieldViewsModel::Delegate:
  virtual void OnCompositionTextConfirmedOrCleared() OVERRIDE;

  // Returns the TextfieldViewsModel's text/cursor/selection rendering model.
  gfx::RenderText* GetRenderText() const;

  // A callback function to periodically update the cursor state.
  void UpdateCursor();

  // Repaint the cursor.
  void RepaintCursor();

  // Update the cursor_bounds and text_offset.
  void UpdateCursorBoundsAndTextOffset(size_t cursor_pos, bool insert_mode);

  void PaintTextAndCursor(gfx::Canvas* canvas);

  // Handle the keyevent.
  bool HandleKeyEvent(const KeyEvent& key_event);

  // Helper function to call MoveCursorTo on the TextfieldViewsModel.
  bool MoveCursorTo(const gfx::Point& point, bool select);

  // Utility function to inform the parent textfield (and its controller if any)
  // that the text in the textfield has changed.
  void PropagateTextChange();

  // Does necessary updates when the text and/or the position of the cursor
  // changed.
  void UpdateAfterChange(bool text_changed, bool cursor_changed);

  // Utility function to prepare the context menu..
  void UpdateContextMenu();

  // Convenience method to call InputMethod::OnTextInputTypeChanged();
  void OnTextInputTypeChanged();

  // Convenience method to call InputMethod::OnCaretBoundsChanged();
  void OnCaretBoundsChanged();

  // Convenience method to call TextfieldController::OnBeforeUserAction();
  void OnBeforeUserAction();

  // Convenience method to call TextfieldController::OnAfterUserAction();
  void OnAfterUserAction();

  // Calls |model_->Paste()| and calls TextfieldController::ContentsChanged()
  // explicitly if paste succeeded.
  bool Paste();

  // Tracks the mouse clicks for single/double/triple clicks.
  void TrackMouseClicks(const MouseEvent& event);

  // Handles mouse press events.
  void HandleMousePressEvent(const MouseEvent& event);

  // Checks if a char is ok to be inserted into the textfield. The |ch| is a
  // modified character, i.e., modifiers took effect when generating this char.
  static bool ShouldInsertChar(char16 ch, int flags);

  // The parent textfield, the owner of this object.
  Textfield* textfield_;

  // The text model.
  scoped_ptr<TextfieldViewsModel> model_;

  // The reference to the border class. The object is owned by View::border_.
  FocusableBorder* text_border_;

  // The textfield's text and drop cursor visibility.
  bool is_cursor_visible_;
  // The drop cursor is a visual cue for where dragged text will be dropped.
  bool is_drop_cursor_visible_;

  // True if InputMethod::CancelComposition() should not be called.
  bool skip_input_method_cancel_composition_;

  // Is the user potentially dragging and dropping from this view?
  bool initiating_drag_;

  // A runnable method factory for callback to update the cursor.
  base::WeakPtrFactory<NativeTextfieldViews> cursor_timer_;

  // State variables used to track double and triple clicks.
  size_t aggregated_clicks_;
  base::Time last_click_time_;
  gfx::Point last_click_location_;

  // Context menu and its content list for the textfield.
  scoped_ptr<ui::SimpleMenuModel> context_menu_contents_;
  scoped_ptr<views::MenuModelAdapter> context_menu_delegate_;
  scoped_ptr<views::MenuRunner> context_menu_runner_;

  scoped_ptr<TouchSelectionController> touch_selection_controller_;

  DISALLOW_COPY_AND_ASSIGN(NativeTextfieldViews);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TEXTFIELD_NATIVE_TEXTFIELD_VIEWS_H_
