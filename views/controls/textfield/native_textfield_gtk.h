// Copyright (c) 2009 The Chromium Authors. All rights reserved.
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
  virtual string16 GetText() const;
  virtual void UpdateText();
  virtual void AppendText(const string16& text);
  virtual string16 GetSelectedText() const;
  virtual void SelectAll();
  virtual void ClearSelection();
  virtual void UpdateBorder();
  virtual void UpdateTextColor();
  virtual void UpdateBackgroundColor();
  virtual void UpdateReadOnly();
  virtual void UpdateFont();
  virtual void UpdateIsPassword();
  virtual void UpdateEnabled();
  virtual bool IsPassword();
  virtual gfx::Insets CalculateInsets();
  virtual void SetHorizontalMargins(int left, int right);
  virtual void SetFocus();
  virtual View* GetView();
  virtual gfx::NativeView GetTestingHandle() const;
  virtual bool IsIMEComposing() const;

  // Overridden from NativeControlGtk:
  virtual void CreateNativeControl();
  virtual void NativeControlCreated(GtkWidget* widget);

 private:
  Textfield* textfield_;

  // Callback when the entry text changes.
  static gboolean OnKeyPressEventHandler(
      GtkWidget* entry,
      GdkEventKey* event,
      NativeTextfieldGtk* textfield);
  gboolean OnKeyPressEvent(GdkEventKey* event);
  static gboolean OnChangedHandler(
      GtkWidget* entry,
      NativeTextfieldGtk* textfield);
  gboolean OnChanged();

  DISALLOW_COPY_AND_ASSIGN(NativeTextfieldGtk);
};

}  // namespace views

#endif  // VIEWS_CONTROLS_TEXTFIELD_NATIVE_TEXTFIELD_GTK_H_
