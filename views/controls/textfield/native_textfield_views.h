// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_TEXTFIELD_NATIVE_TEXTFIELD_VIEWS_H_
#define VIEWS_CONTROLS_TEXTFIELD_NATIVE_TEXTFIELD_VIEWS_H_
#pragma once

#include "base/string16.h"
#include "base/task.h"
#include "ui/base/models/simple_menu_model.h"
#include "ui/gfx/font.h"
#include "views/border.h"
#include "views/controls/textfield/native_textfield_wrapper.h"
#include "views/controls/textfield/textfield_views_model.h"
#include "views/ime/text_input_client.h"
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
class Menu2;

// A views/skia only implementation of NativeTextfieldWrapper.
// No platform specific code is used.
// Following features are not yet supported.
// * BIDI/Complex script.
// * Support surrogate pair, or maybe we should just use UTF32 internally.
// * X selection (only if we want to support).
// * STYLE_MULTILINE, STYLE_LOWERCASE text. (These are not used in
//   chromeos, so we may not need them)
class NativeTextfieldViews : public View,
                             public ContextMenuController,
                             public DragController,
                             public NativeTextfieldWrapper,
                             public ui::SimpleMenuModel::Delegate,
                             public TextInputClient,
                             public TextfieldViewsModel::Delegate {
 public:
  explicit NativeTextfieldViews(Textfield* parent);
  ~NativeTextfieldViews();

  // View overrides:
  virtual gfx::NativeCursor GetCursor(const MouseEvent& event) OVERRIDE;
  virtual bool OnMousePressed(const MouseEvent& event) OVERRIDE;
  virtual bool OnMouseDragged(const MouseEvent& event) OVERRIDE;
  virtual void OnMouseReleased(const MouseEvent& event) OVERRIDE;
  virtual bool OnKeyPressed(const KeyEvent& event) OVERRIDE;
  virtual bool GetDropFormats(
      int* formats,
      std::set<OSExchangeData::CustomFormat>* custom_formats) OVERRIDE;
  virtual bool CanDrop(const OSExchangeData& data) OVERRIDE;
  virtual int OnDragUpdated(const DropTargetEvent& event) OVERRIDE;
  virtual int OnPerformDrop(const DropTargetEvent& event) OVERRIDE;
  virtual void OnDragDone() OVERRIDE;
  virtual bool OnKeyReleased(const KeyEvent& event) OVERRIDE;
  virtual void OnPaint(gfx::Canvas* canvas) OVERRIDE;
  virtual void OnFocus() OVERRIDE;
  virtual void OnBlur() OVERRIDE;

  // ContextMenuController overrides:
  virtual void ShowContextMenuForView(View* source,
                                      const gfx::Point& p,
                                      bool is_mouse_gesture) OVERRIDE;

  // Overridden from DragController:
  virtual void WriteDragDataForView(View* sender,
                                    const gfx::Point& press_pt,
                                    OSExchangeData* data) OVERRIDE;
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
  virtual size_t GetCursorPosition() const OVERRIDE;
  virtual bool HandleKeyPressed(const KeyEvent& e) OVERRIDE;
  virtual bool HandleKeyReleased(const KeyEvent& e) OVERRIDE;
  virtual void HandleFocus() OVERRIDE;
  virtual void HandleBlur() OVERRIDE;
  virtual TextInputClient* GetTextInputClient() OVERRIDE;

  // ui::SimpleMenuModel::Delegate overrides
  virtual bool IsCommandIdChecked(int command_id) const OVERRIDE;
  virtual bool IsCommandIdEnabled(int command_id) const OVERRIDE;
  virtual bool GetAcceleratorForCommandId(
      int command_id,
      ui::Accelerator* accelerator) OVERRIDE;
  virtual void ExecuteCommand(int command_id) OVERRIDE;

  // class name of internal
  static const char kViewClassName[];

  // Returns true when
  // 1) built with GYP_DEFINES="touchui=1"
  // 2) enabled by SetEnableTextfieldViews(true)
  // 3) enabled by the command line flag "--enable-textfield-view".
  static bool IsTextfieldViewsEnabled();
  // Enable/Disable TextfieldViews implementation for Textfield.
  static void SetEnableTextfieldViews(bool enabled);

 protected:
  // View override.
  virtual void OnBoundsChanged(const gfx::Rect& previous_bounds) OVERRIDE;

 private:
  friend class NativeTextfieldViewsTest;

  // Overridden from TextInputClient:
  virtual void SetCompositionText(
      const ui::CompositionText& composition) OVERRIDE;
  virtual void ConfirmCompositionText() OVERRIDE;
  virtual void ClearCompositionText() OVERRIDE;
  virtual void InsertText(const string16& text) OVERRIDE;
  virtual void InsertChar(char16 ch, int flags) OVERRIDE;
  virtual ui::TextInputType GetTextInputType() OVERRIDE;
  virtual gfx::Rect GetCaretBounds() OVERRIDE;
  virtual bool HasCompositionText() OVERRIDE;
  virtual bool GetTextRange(ui::Range* range) OVERRIDE;
  virtual bool GetCompositionTextRange(ui::Range* range) OVERRIDE;
  virtual bool GetSelectionRange(ui::Range* range) OVERRIDE;
  virtual bool SetSelectionRange(const ui::Range& range) OVERRIDE;
  virtual bool DeleteRange(const ui::Range& range) OVERRIDE;
  virtual bool GetTextFromRange(
      const ui::Range& range,
      const base::Callback<void(const string16&)>& callback) OVERRIDE;
  virtual void OnInputMethodChanged() OVERRIDE;
  virtual bool ChangeTextDirectionAndLayoutAlignment(
      base::i18n::TextDirection direction) OVERRIDE;
  virtual View* GetOwnerViewOfTextInputClient() OVERRIDE;

  // Overridden from TextfieldViewsModel::Delegate:
  virtual void OnCompositionTextConfirmedOrCleared() OVERRIDE;

  // Returns the Textfield's font.
  const gfx::Font& GetFont() const;

  // Returns the Textfield's text color.
  SkColor GetTextColor() const;

  // A callback function to periodically update the cursor state.
  void UpdateCursor();

  // Repaint the cursor.
  void RepaintCursor();

  // Update the cursor_bounds and text_offset.
  void UpdateCursorBoundsAndTextOffset();

  void PaintTextAndCursor(gfx::Canvas* canvas);

  // Handle the keyevent.
  bool HandleKeyEvent(const KeyEvent& key_event);

  // Find a cusor position for given |point| in this views coordinates.
  size_t FindCursorPosition(const gfx::Point& point) const;

  // Returns true if the local point is over the selected range of text.
  bool IsPointInSelection(const gfx::Point& point) const;

  // Helper function to call MoveCursorTo on the TextfieldViewsModel.
  bool MoveCursorTo(const gfx::Point& point, bool select);

  // Utility function to inform the parent textfield (and its controller if any)
  // that the text in the textfield has changed.
  void PropagateTextChange();

  // Does necessary updates when the text and/or the position of the cursor
  // changed.
  void UpdateAfterChange(bool text_changed, bool cursor_changed);

  // Utility function to create the context menu if one does not already exist.
  void InitContextMenuIfRequired();

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

  // Checks if a char is ok to be inserted into the textfield. The |ch| is a
  // modified character, i.e., modifiers took effect when generating this char.
  static bool ShouldInsertChar(char16 ch, int flags);

  // The parent textfield, the owner of this object.
  Textfield* textfield_;

  // The text model.
  scoped_ptr<TextfieldViewsModel> model_;

  // The reference to the border class. The object is owned by View::border_.
  FocusableBorder* text_border_;

  // The x offset for the text to be drawn, without insets;
  int text_offset_;

  // Cursor's bounds in the textfield's coordinates.
  gfx::Rect cursor_bounds_;

  // True if the textfield is in insert mode.
  bool insert_;

  // The drawing state of cursor. True to draw.
  bool is_cursor_visible_;

  // True if InputMethod::CancelComposition() should not be called.
  bool skip_input_method_cancel_composition_;

  // Is the user potentially dragging and dropping from this view?
  bool initiating_drag_;

  // A runnable method factory for callback to update the cursor.
  ScopedRunnableMethodFactory<NativeTextfieldViews> cursor_timer_;

  // State variables used to track double and triple clicks.
  size_t aggregated_clicks_;
  base::Time last_click_time_;
  gfx::Point last_click_location_;

  // Context menu and its content list for the textfield.
  scoped_ptr<ui::SimpleMenuModel> context_menu_contents_;
  scoped_ptr<Menu2> context_menu_menu_;

  DISALLOW_COPY_AND_ASSIGN(NativeTextfieldViews);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_TEXTFIELD_NATIVE_TEXTFIELD_VIEWS_H_
