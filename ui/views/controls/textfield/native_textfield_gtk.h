// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef UI_VIEWS_CONTROLS_TEXTFIELD_NATIVE_TEXTFIELD_GTK_H_
#define UI_VIEWS_CONTROLS_TEXTFIELD_NATIVE_TEXTFIELD_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "base/string16.h"
#include "ui/base/gtk/gtk_signal.h"
#include "ui/views/controls/textfield/native_textfield_wrapper.h"
#include "views/controls/native_control_gtk.h"

namespace gfx {
class SelectionModel;
}

namespace views {

class NativeTextfieldGtk : public NativeControlGtk,
                           public NativeTextfieldWrapper {
 public:
  explicit NativeTextfieldGtk(Textfield* parent);
  virtual ~NativeTextfieldGtk();

  // Returns the textfield this NativeTextfieldGtk wraps.
  Textfield* textfield() const { return textfield_; }

  // Returns the inner border of the entry.
  static gfx::Insets GetEntryInnerBorder(GtkEntry* entry);
  static gfx::Insets GetTextViewInnerBorder(GtkTextView* text_view);

  // Overridden from NativeTextfieldWrapper:
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
  virtual bool HandleKeyPressed(const views::KeyEvent& e) OVERRIDE;
  virtual bool HandleKeyReleased(const views::KeyEvent& e) OVERRIDE;
  virtual void HandleFocus() OVERRIDE;
  virtual void HandleBlur() OVERRIDE;
  virtual ui::TextInputClient* GetTextInputClient() OVERRIDE;
  virtual void ApplyStyleRange(const gfx::StyleRange& style) OVERRIDE;
  virtual void ApplyDefaultStyle() OVERRIDE;
  virtual void ClearEditHistory() OVERRIDE;

  // Overridden from NativeControlGtk:
  virtual void CreateNativeControl() OVERRIDE;
  virtual void NativeControlCreated(GtkWidget* widget) OVERRIDE;

  // Returns true if the textfield is for password.
  bool IsPassword();

 private:
  // Gtk signal callbacks.
  CHROMEGTK_CALLBACK_0(NativeTextfieldGtk, void, OnActivate);
  CHROMEGTK_CALLBACK_1(NativeTextfieldGtk, gboolean, OnButtonPressEvent,
                       GdkEventButton*);
  CHROMEGTK_CALLBACK_1(NativeTextfieldGtk, gboolean, OnButtonReleaseEventAfter,
                       GdkEventButton*);
  CHROMEG_CALLBACK_0(NativeTextfieldGtk, void, OnChanged, GObject*);
  CHROMEGTK_CALLBACK_1(NativeTextfieldGtk, gboolean, OnKeyPressEvent,
                       GdkEventKey*);
  CHROMEGTK_CALLBACK_1(NativeTextfieldGtk, gboolean, OnKeyPressEventAfter,
                       GdkEventKey*);
  CHROMEGTK_CALLBACK_3(NativeTextfieldGtk, void, OnMoveCursor,
                       GtkMovementStep, gint, gboolean);
  CHROMEGTK_CALLBACK_0(NativeTextfieldGtk, void, OnPasteClipboard);

  Textfield* textfield_;

  // Indicates that user requested to paste clipboard.
  // The actual paste clipboard action might be performed later if the
  // clipboard is not empty.
  bool paste_clipboard_requested_;

  DISALLOW_COPY_AND_ASSIGN(NativeTextfieldGtk);
};

}  // namespace views

#endif  // UI_VIEWS_CONTROLS_TEXTFIELD_NATIVE_TEXTFIELD_GTK_H_
