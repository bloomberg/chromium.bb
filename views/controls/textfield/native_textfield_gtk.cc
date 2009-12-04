// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include "views/controls/textfield/native_textfield_gtk.h"

#include "app/gfx/insets.h"
#include "app/gfx/gtk_util.h"
#include "base/string_util.h"
#include "skia/ext/skia_utils_gtk.h"
#include "views/controls/textfield/textfield.h"

namespace views {
// A character used to hide a text in password mode.
const char kPasswordChar = '*';

////////////////////////////////////////////////////////////////////////////////
// NativeTextfieldGtk, public:

NativeTextfieldGtk::NativeTextfieldGtk(Textfield* textfield)
    : textfield_(textfield) {
  if (textfield_->style() & Textfield::STYLE_MULTILINE)
    NOTIMPLEMENTED();  // We don't support multiline yet.
  // Make |textfield| the focused view, so that when we get focused the focus
  // manager sees |textfield| as the focused view (since we are just a wrapper
  // view).
  set_focus_view(textfield);
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

  if (!textfield_->draw_border())
    gtk_entry_set_has_frame(GTK_ENTRY(native_view()), false);
}

void NativeTextfieldGtk::UpdateTextColor() {
  if (textfield_->use_default_text_color()) {
    // Passing NULL as the color undoes the effect of previous calls to
    // gtk_widget_modify_text.
    gtk_widget_modify_text(native_view(), GTK_STATE_NORMAL, NULL);
    return;
  }
  GdkColor gdk_color = skia::SkColorToGdkColor(textfield_->text_color());
  gtk_widget_modify_text(native_view(), GTK_STATE_NORMAL, &gdk_color);
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
  PangoFontDescription* pfd =
      gfx::Font::PangoFontFromGfxFont(textfield_->font());
  gtk_widget_modify_font(native_view(), pfd);
  pango_font_description_free(pfd);
}

void NativeTextfieldGtk::UpdateEnabled() {
  if (!native_view())
    return;
  SetEnabled(textfield_->IsEnabled());
}

gfx::Insets NativeTextfieldGtk::CalculateInsets() {
  if (!native_view())
    return gfx::Insets();

  GtkWidget* widget = native_view();
  GtkEntry* entry = GTK_ENTRY(widget);
  gfx::Insets insets;

  const GtkBorder* inner_border = gtk_entry_get_inner_border(entry);
  if (inner_border) {
    insets += gfx::Insets(*inner_border);
  } else {
    // No explicit border set, try the style.
    GtkBorder* style_border;
    gtk_widget_style_get(widget, "inner-border", &style_border, NULL);
    if (style_border) {
      insets += gfx::Insets(*style_border);
      gtk_border_free(style_border);
    } else {
      // If border is null, Gtk uses 2 on all sides.
      insets += gfx::Insets(2, 2, 2, 2);
    }
  }

  if (entry->has_frame) {
    insets += gfx::Insets(widget->style->ythickness,
                          widget->style->xthickness,
                          widget->style->ythickness,
                          widget->style->xthickness);
  }

  gboolean interior_focus;
  gint focus_width;
  gtk_widget_style_get(widget,
                       "focus-line-width", &focus_width,
                       "interior-focus", &interior_focus,
                       NULL);
  if (!interior_focus)
    insets += gfx::Insets(focus_width, focus_width, focus_width, focus_width);

  return insets;
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

// static
gboolean NativeTextfieldGtk::OnKeyPressEventHandler(
    GtkWidget* entry,
    GdkEventKey* event,
    NativeTextfieldGtk* textfield) {
  return textfield->OnKeyPressEvent(event);
}

gboolean NativeTextfieldGtk::OnKeyPressEvent(GdkEventKey* event) {
  Textfield::Controller* controller = textfield_->GetController();
  if (controller) {
    Textfield::Keystroke ks(event);
    return controller->HandleKeystroke(textfield_, ks);
  }
  return false;
}

// static
gboolean NativeTextfieldGtk::OnChangedHandler(
    GtkWidget* entry,
    NativeTextfieldGtk* textfield) {
  return textfield->OnChanged();
}

gboolean NativeTextfieldGtk::OnChanged() {
  textfield_->SyncText();
  Textfield::Controller* controller = textfield_->GetController();
  if (controller)
    controller->ContentsChanged(textfield_, GetText());
  return false;
}

////////////////////////////////////////////////////////////////////////////////
// NativeTextfieldGtk, NativeControlGtk overrides:

void NativeTextfieldGtk::CreateNativeControl() {
  NativeControlCreated(gtk_entry_new());
  if (textfield_->IsPassword()) {
    gtk_entry_set_invisible_char(GTK_ENTRY(native_view()),
                                 static_cast<gunichar>(kPasswordChar));
    gtk_entry_set_visibility(GTK_ENTRY(native_view()), false);
  }
}

void NativeTextfieldGtk::NativeControlCreated(GtkWidget* widget) {
  NativeControlGtk::NativeControlCreated(widget);
  g_signal_connect(widget, "changed",
                   G_CALLBACK(OnChangedHandler), this);
  g_signal_connect(widget, "key-press-event",
                   G_CALLBACK(OnKeyPressEventHandler), this);
}

////////////////////////////////////////////////////////////////////////////////
// NativeTextfieldWrapper, public:

// static
NativeTextfieldWrapper* NativeTextfieldWrapper::CreateWrapper(
    Textfield* field) {
  return new NativeTextfieldGtk(field);
}

}  // namespace views
