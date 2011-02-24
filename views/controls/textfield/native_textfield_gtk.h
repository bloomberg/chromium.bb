// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef VIEWS_CONTROLS_TEXTFIELD_NATIVE_TEXTFIELD_GTK_H_
#define VIEWS_CONTROLS_TEXTFIELD_NATIVE_TEXTFIELD_GTK_H_
#pragma once

#include <gtk/gtk.h>

#include "base/string16.h"
#include "views/controls/native_control_gtk.h"
#include "views/controls/textfield/native_textfield_wrapper.h"

namespace views {

class NativeTextfieldGtk : public NativeControlGtk,
                           public NativeTextfieldWrapper {
 public:
  explicit NativeTextfieldGtk(Textfield* parent);
  ~NativeTextfieldGtk();

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
  virtual void GetSelectedRange(TextRange* range) const OVERRIDE;
  virtual void SelectRange(const TextRange& range) OVERRIDE;
  virtual size_t GetCursorPosition() const OVERRIDE;
  virtual bool HandleKeyPressed(const views::KeyEvent& e) OVERRIDE;
  virtual bool HandleKeyReleased(const views::KeyEvent& e) OVERRIDE;
  virtual void HandleFocus() OVERRIDE;
  virtual void HandleBlur() OVERRIDE;

  // Overridden from NativeControlGtk:
  virtual void CreateNativeControl() OVERRIDE;
  virtual void NativeControlCreated(GtkWidget* widget) OVERRIDE;

  // Returns true if the textfield is for password.
  bool IsPassword();

 private:
  Textfield* textfield_;

  // Callback when the entry text changes.
  static gboolean OnKeyPressEventHandler(
      GtkWidget* entry,
      GdkEventKey* event,
      NativeTextfieldGtk* textfield);
  gboolean OnKeyPressEvent(GdkEventKey* event);
  static gboolean OnActivateHandler(
      GtkWidget* entry,
      NativeTextfieldGtk* textfield);
  gboolean OnActivate();
  static gboolean OnChangedHandler(
      GtkWidget* entry,
      NativeTextfieldGtk* textfield);
  gboolean OnChanged();

  DISALLOW_COPY_AND_ASSIGN(NativeTextfieldGtk);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_TEXTFIELD_NATIVE_TEXTFIELD_GTK_H_
