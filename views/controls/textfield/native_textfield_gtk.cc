// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/textfield/native_textfield_gtk.h"

#include "base/string_util.h"
#include "views/controls/textfield/textfield.h"

namespace views {

////////////////////////////////////////////////////////////////////////////////
// NativeTextfieldGtk, public:

NativeTextfieldGtk::NativeTextfieldGtk(Textfield* textfield)
    : NativeControlGtk(),
      textfield_(textfield) {
  if (textfield_->style() & Textfield::STYLE_MULTILINE)
    NOTIMPLEMENTED();  // We don't support multiline yet.
}

NativeTextfieldGtk::~NativeTextfieldGtk() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeTextfieldGtk, NativeTextfieldWrapper implementation:

std::wstring NativeTextfieldGtk::GetText() const {
  if (!native_view())
    return std::wstring();
  return UTF8ToWide(gtk_entry_get_text(GTK_ENTRY(native_view())));
}

void NativeTextfieldGtk::UpdateText() {
  if (!native_view())
    return;
  gtk_entry_set_text(GTK_ENTRY(native_view()),
                     WideToUTF8(textfield_->text()).c_str());
}

void NativeTextfieldGtk::AppendText(const std::wstring& text) {
  if (!native_view())
    return;
  gtk_entry_append_text(GTK_ENTRY(native_view()), WideToUTF8(text).c_str());
}

std::wstring NativeTextfieldGtk::GetSelectedText() const {
  if (!native_view())
    return std::wstring();

  int begin, end;
  if (!gtk_editable_get_selection_bounds(GTK_EDITABLE(native_view()),
                                         &begin, &end))
    return std::wstring();  // Nothing selected.

  return UTF8ToWide(std::string(
      &gtk_entry_get_text(GTK_ENTRY(native_view()))[begin],
      end - begin));
}

void NativeTextfieldGtk::SelectAll() {
  if (!native_view())
    return;
  // -1 as the end position selects to the end of the text.
  gtk_editable_select_region(GTK_EDITABLE(native_view()),
                             0, -1);
}

void NativeTextfieldGtk::ClearSelection() {
  if (!native_view())
    return;
  gtk_editable_select_region(GTK_EDITABLE(native_view()), 0, 0);
}

void NativeTextfieldGtk::UpdateBorder() {
  if (!native_view())
    return;
  NOTIMPLEMENTED();
}

void NativeTextfieldGtk::UpdateBackgroundColor() {
  if (!native_view())
    return;
  NOTIMPLEMENTED();
}

void NativeTextfieldGtk::UpdateReadOnly() {
  if (!native_view())
    return;
  gtk_editable_set_editable(GTK_EDITABLE(native_view()),
                            textfield_->IsEnabled());
}

void NativeTextfieldGtk::UpdateFont() {
  if (!native_view())
    return;
  NOTIMPLEMENTED();
}

void NativeTextfieldGtk::UpdateEnabled() {
  if (!native_view())
    return;
  NOTIMPLEMENTED();
}

void NativeTextfieldGtk::SetHorizontalMargins(int left, int right) {
  if (!native_view())
    return;
  NOTIMPLEMENTED();
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
  GtkWidget* widget = gtk_entry_new();
  // TODO(brettw) hook in an observer to get text change events so we can call
  // the controller.
  NativeControlCreated(widget);
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
