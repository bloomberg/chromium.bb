// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include "views/controls/textfield/native_textfield_gtk.h"

#include "base/gfx/gtk_util.h"
#include "base/string_util.h"
#include "skia/ext/skia_utils_gtk.h"
#include "views/controls/textfield/textfield.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeTextfieldGtk, public:

NativeTextfieldGtk::NativeTextfieldGtk(Textfield* textfield)
    : textfield_(textfield) {
  if (textfield_->style() & Textfield::STYLE_MULTILINE)
    NOTIMPLEMENTED();  // We don't support multiline yet.
}

NativeTextfieldGtk::~NativeTextfieldGtk() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeTextfieldGtk, NativeTextfieldWrapper implementation:

string16 NativeTextfieldGtk::GetText() const {
  return UTF8ToUTF16(gtk_entry_get_text(GTK_ENTRY(native_view())));
}

void NativeTextfieldGtk::UpdateText() {
  if (!native_view())
    return;
  gtk_entry_set_text(GTK_ENTRY(native_view()),
                     UTF16ToUTF8(textfield_->text()).c_str());
}

void NativeTextfieldGtk::AppendText(const string16& text) {
  gint position = -1;
  gtk_editable_insert_text(GTK_EDITABLE(native_view()),
                           UTF16ToUTF8(text).c_str(),
                           text.size(), &position);
  if (!native_view())
    return;
  gtk_entry_append_text(GTK_ENTRY(native_view()), UTF16ToUTF8(text).c_str());
}

string16 NativeTextfieldGtk::GetSelectedText() const {
  if (!native_view())
    return string16();

  string16 result;
  gint start_pos;
  gint end_pos;
  if (!gtk_editable_get_selection_bounds(GTK_EDITABLE(native_view()),
                                         &start_pos, &end_pos))
    return result;  // No selection.

  UTF8ToUTF16(gtk_editable_get_chars(GTK_EDITABLE(native_view()),
                                     start_pos, end_pos),
              end_pos - start_pos, &result);
  return result;
}

void NativeTextfieldGtk::SelectAll() {
  if (!native_view())
    return;
  // -1 as the end position selects to the end of the text.
  gtk_editable_select_region(GTK_EDITABLE(native_view()), 0, -1);
}

void NativeTextfieldGtk::ClearSelection() {
  if (!native_view())
    return;
  gtk_editable_select_region(GTK_EDITABLE(native_view()), 0, 0);
}

void NativeTextfieldGtk::UpdateBorder() {
  if (!native_view())
    return;
}

void NativeTextfieldGtk::UpdateBackgroundColor() {
  if (textfield_->use_default_background_color()) {
    // Passing NULL as the color undoes the effect of previous calls to
    // gtk_widget_modify_base.
    gtk_widget_modify_base(native_view(), GTK_STATE_NORMAL, NULL);
    return;
  }
  GdkColor gdk_color = skia::SkColorToGdkColor(textfield_->background_color());
  gtk_widget_modify_base(native_view(), GTK_STATE_NORMAL, &gdk_color);
}

void NativeTextfieldGtk::UpdateReadOnly() {
  if (!native_view())
    return;
  gtk_editable_set_editable(GTK_EDITABLE(native_view()),
                            !textfield_->read_only());
}

void NativeTextfieldGtk::UpdateFont() {
  if (!native_view())
    return;
  gtk_widget_modify_font(native_view(),
                         gfx::Font::PangoFontFromGfxFont(textfield_->font()));
}

void NativeTextfieldGtk::UpdateEnabled() {
  if (!native_view())
    return;
  SetEnabled(textfield_->IsEnabled());
}

void NativeTextfieldGtk::SetHorizontalMargins(int left, int right) {
  if (!native_view())
    return;
  GtkBorder border = { left, right, 0, 0 };
  gtk_entry_set_inner_border(GTK_ENTRY(native_view()), &border);
}

void NativeTextfieldGtk::SetFocus() {
  Focus();
}

View* NativeTextfieldGtk::GetView() {
  return this;
}

gfx::NativeView NativeTextfieldGtk::GetTestingHandle() const {
  return native_view();
}

////////////////////////////////////////////////////////////////////////////////
// NativeTextfieldGtk, NativeControlGtk overrides:

void NativeTextfieldGtk::CreateNativeControl() {
  // TODO(brettw) hook in an observer to get text change events so we can call
  // the controller.
  NativeControlCreated(gtk_entry_new());
}

void NativeTextfieldGtk::NativeControlCreated(GtkWidget* widget) {
  NativeControlGtk::NativeControlCreated(widget);
  // TODO(port): post-creation init
}

////////////////////////////////////////////////////////////////////////////////
// NativeTextfieldWrapper, public:

// static
NativeTextfieldWrapper* NativeTextfieldWrapper::CreateWrapper(
    Textfield* field) {
  return new NativeTextfieldGtk(field);
}

}  // namespace views
