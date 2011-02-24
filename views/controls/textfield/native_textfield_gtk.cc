// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "views/controls/textfield/native_textfield_gtk.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/skia_utils_gtk.h"
#include "views/controls/textfield/gtk_views_entry.h"
#include "views/controls/textfield/gtk_views_textview.h"
#include "views/controls/textfield/native_textfield_views.h"
#include "views/controls/textfield/textfield.h"
#include "views/widget/widget_gtk.h"

namespace views {

// A character used to hide a text in password mode.
static const char kPasswordChar = '*';

// Border width for GtkTextView.
const int kTextViewBorderWidth = 4;

////////////////////////////////////////////////////////////////////////////////
// NativeTextfieldGtk, public:

NativeTextfieldGtk::NativeTextfieldGtk(Textfield* textfield)
    : textfield_(textfield) {
  if (textfield_->IsMultiLine() && textfield_->IsPassword())
    NOTIMPLEMENTED();  // We don't support multiline password yet.
  // Make |textfield| the focused view, so that when we get focused the focus
  // manager sees |textfield| as the focused view (since we are just a wrapper
  // view).
  set_focus_view(textfield);
}

NativeTextfieldGtk::~NativeTextfieldGtk() {
}

// Returns the inner border of an entry.
// static
gfx::Insets NativeTextfieldGtk::GetEntryInnerBorder(GtkEntry* entry) {
  const GtkBorder* inner_border = gtk_entry_get_inner_border(entry);
  if (inner_border)
    return gfx::Insets(*inner_border);

  // No explicit border set, try the style.
  GtkBorder* style_border;
  gtk_widget_style_get(GTK_WIDGET(entry), "inner-border", &style_border, NULL);
  if (style_border) {
    gfx::Insets insets = gfx::Insets(*style_border);
    gtk_border_free(style_border);
    return insets;
  }

  // If border is null, Gtk uses 2 on all sides.
  return gfx::Insets(2, 2, 2, 2);
}

gfx::Insets NativeTextfieldGtk::GetTextViewInnerBorder(GtkTextView* text_view) {
  return gfx::Insets(kTextViewBorderWidth / 2, kTextViewBorderWidth / 2,
                     kTextViewBorderWidth / 2, kTextViewBorderWidth / 2);
}

////////////////////////////////////////////////////////////////////////////////
// NativeTextfieldGtk, NativeTextfieldWrapper implementation:

string16 NativeTextfieldGtk::GetText() const {
  if (textfield_->IsMultiLine()) {
    GtkTextBuffer* text_buffer = gtk_text_view_get_buffer(
        GTK_TEXT_VIEW(native_view()));

    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(text_buffer, &start, &end);

    return UTF8ToUTF16(gtk_text_iter_get_visible_text(&start, &end));
  } else {
    return UTF8ToUTF16(gtk_entry_get_text(GTK_ENTRY(native_view())));
  }
}

void NativeTextfieldGtk::UpdateText() {
  if (!native_view())
    return;
  if (textfield_->IsMultiLine()) {
    GtkTextBuffer* text_buffer = gtk_text_view_get_buffer(
        GTK_TEXT_VIEW(native_view()));

    std::string utf8 = UTF16ToUTF8(textfield_->text());
    gtk_text_buffer_set_text(text_buffer,  utf8.c_str(), utf8.length());
  } else {
    gtk_entry_set_text(GTK_ENTRY(native_view()),
                       UTF16ToUTF8(textfield_->text()).c_str());
  }
}

void NativeTextfieldGtk::AppendText(const string16& text) {
  if (!native_view())
    return;
  if (textfield_->IsMultiLine()) {
    GtkTextBuffer* text_buffer = gtk_text_view_get_buffer(
        GTK_TEXT_VIEW(native_view()));

    GtkTextIter end;
    gtk_text_buffer_get_end_iter(text_buffer, &end);

    std::string utf8 = UTF16ToUTF8(text);
    gtk_text_buffer_insert(text_buffer, &end, utf8.c_str(), utf8.length());
  } else {
    gtk_entry_append_text(GTK_ENTRY(native_view()), UTF16ToUTF8(text).c_str());
  }
}

string16 NativeTextfieldGtk::GetSelectedText() const {
  if (!native_view())
    return string16();

  string16 result;

  if (textfield_->IsMultiLine()) {
    GtkTextBuffer* text_buffer = gtk_text_view_get_buffer(
        GTK_TEXT_VIEW(native_view()));

    GtkTextIter start;
    GtkTextIter end;
    if (gtk_text_buffer_get_selection_bounds(text_buffer, &start, &end)) {
      gchar* selected_text = gtk_text_iter_get_visible_text(&start, &end);
      if (selected_text)
        UTF8ToUTF16(selected_text, strlen(selected_text), &result);
    }
  } else {
    gint start_pos;
    gint end_pos;
    if (!gtk_editable_get_selection_bounds(GTK_EDITABLE(native_view()),
                                           &start_pos, &end_pos))
      return result;  // No selection.

    UTF8ToUTF16(gtk_editable_get_chars(GTK_EDITABLE(native_view()),
                                       start_pos, end_pos),
                end_pos - start_pos, &result);
  }

  return result;
}

void NativeTextfieldGtk::SelectAll() {
  if (!native_view())
    return;
  if (textfield_->IsMultiLine()) {
    GtkTextBuffer* text_buffer = gtk_text_view_get_buffer(
        GTK_TEXT_VIEW(native_view()));

    GtkTextIter start;
    GtkTextIter end;
    gtk_text_buffer_get_bounds(text_buffer, &start, &end);
    gtk_text_buffer_select_range(text_buffer, &start, &end);
  } else {
    // -1 as the end position selects to the end of the text.
    gtk_editable_select_region(GTK_EDITABLE(native_view()), 0, -1);
  }
}

void NativeTextfieldGtk::ClearSelection() {
  if (!native_view())
    return;
  if (textfield_->IsMultiLine()) {
    GtkTextBuffer* text_buffer = gtk_text_view_get_buffer(
        GTK_TEXT_VIEW(native_view()));

    GtkTextMark* insert_mark = gtk_text_buffer_get_insert(text_buffer);
    GtkTextIter insert;
    gtk_text_buffer_get_iter_at_mark(text_buffer, &insert, insert_mark);
    gtk_text_buffer_select_range(text_buffer, &insert, &insert);
  } else {
    gtk_editable_select_region(GTK_EDITABLE(native_view()), 0, 0);
  }
}

void NativeTextfieldGtk::UpdateBorder() {
  if (!native_view())
    return;

  if (textfield_->IsMultiLine()) {
    if (!textfield_->draw_border()) {
      gtk_container_set_border_width(GTK_CONTAINER(native_view()), 0);

      // Use margin to match entry with no border
      textfield_->SetHorizontalMargins(kTextViewBorderWidth / 2 + 1,
                                       kTextViewBorderWidth / 2 + 1);
    }
  } else {
    if (!textfield_->draw_border())
      gtk_entry_set_has_frame(GTK_ENTRY(native_view()), false);
  }
}

void NativeTextfieldGtk::UpdateTextColor() {
  if (textfield_->use_default_text_color()) {
    // Passing NULL as the color undoes the effect of previous calls to
    // gtk_widget_modify_text.
    gtk_widget_modify_text(native_view(), GTK_STATE_NORMAL, NULL);
    return;
  }
  GdkColor gdk_color = gfx::SkColorToGdkColor(textfield_->text_color());
  gtk_widget_modify_text(native_view(), GTK_STATE_NORMAL, &gdk_color);
}

void NativeTextfieldGtk::UpdateBackgroundColor() {
  if (textfield_->use_default_background_color()) {
    // Passing NULL as the color undoes the effect of previous calls to
    // gtk_widget_modify_base.
    gtk_widget_modify_base(native_view(), GTK_STATE_NORMAL, NULL);
    return;
  }
  GdkColor gdk_color = gfx::SkColorToGdkColor(textfield_->background_color());
  gtk_widget_modify_base(native_view(), GTK_STATE_NORMAL, &gdk_color);
}

void NativeTextfieldGtk::UpdateReadOnly() {
  if (!native_view())
    return;

  if (textfield_->IsMultiLine()) {
    gtk_text_view_set_editable(GTK_TEXT_VIEW(native_view()),
                               !textfield_->read_only());
  } else {
    gtk_editable_set_editable(GTK_EDITABLE(native_view()),
                              !textfield_->read_only());
  }
}

void NativeTextfieldGtk::UpdateFont() {
  if (!native_view())
    return;
  PangoFontDescription* pfd = textfield_->font().GetNativeFont();
  gtk_widget_modify_font(native_view(), pfd);
  pango_font_description_free(pfd);
}

void NativeTextfieldGtk::UpdateIsPassword() {
  if (!native_view())
    return;
  if (!textfield_->IsMultiLine()) {
    gtk_entry_set_visibility(GTK_ENTRY(native_view()),
                             !textfield_->IsPassword());
  }
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
  gfx::Insets insets;

  if (textfield_->IsMultiLine()) {
    insets += GetTextViewInnerBorder(GTK_TEXT_VIEW(widget));
  } else {
    GtkEntry* entry = GTK_ENTRY(widget);
    insets += GetEntryInnerBorder(entry);
    if (entry->has_frame) {
      insets += gfx::Insets(widget->style->ythickness,
                            widget->style->xthickness,
                            widget->style->ythickness,
                            widget->style->xthickness);
    }
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

void NativeTextfieldGtk::UpdateHorizontalMargins() {
  if (!native_view())
    return;

  int left, right;
  if (!textfield_->GetHorizontalMargins(&left, &right))
    return;

  if (textfield_->IsMultiLine()) {
    GtkTextView* text_view = GTK_TEXT_VIEW(native_view());
    gtk_text_view_set_left_margin(text_view, left);
    gtk_text_view_set_right_margin(text_view, right);
  } else {
    gfx::Insets insets = GetEntryInnerBorder(GTK_ENTRY(native_view()));
    GtkBorder border = {left, right, insets.top(), insets.bottom()};
    gtk_entry_set_inner_border(GTK_ENTRY(native_view()), &border);
  }
}

void NativeTextfieldGtk::UpdateVerticalMargins() {
  if (!native_view())
    return;

  int top, bottom;
  if (!textfield_->GetVerticalMargins(&top, &bottom))
    return;

  if (!textfield_->IsMultiLine()) {
    gfx::Insets insets = GetEntryInnerBorder(GTK_ENTRY(native_view()));
    GtkBorder border = {insets.left(), insets.right(), top, bottom};
    gtk_entry_set_inner_border(GTK_ENTRY(native_view()), &border);
  } else {
    NOTIMPLEMENTED();
  }
}

bool NativeTextfieldGtk::SetFocus() {
  OnFocus();
  return true;
}

View* NativeTextfieldGtk::GetView() {
  return this;
}

gfx::NativeView NativeTextfieldGtk::GetTestingHandle() const {
  return native_view();
}

bool NativeTextfieldGtk::IsIMEComposing() const {
  return false;
}

void NativeTextfieldGtk::GetSelectedRange(TextRange* range) const {
  NOTREACHED();
}

void NativeTextfieldGtk::SelectRange(const TextRange& range) {
  NOTREACHED();
}

size_t NativeTextfieldGtk::GetCursorPosition() const {
  NOTREACHED();
  return 0U;
}

bool NativeTextfieldGtk::HandleKeyPressed(const views::KeyEvent& e) {
  return false;
}

bool NativeTextfieldGtk::HandleKeyReleased(const views::KeyEvent& e) {
  return false;
}

void NativeTextfieldGtk::HandleFocus() {
}

void NativeTextfieldGtk::HandleBlur() {
}

// static
gboolean NativeTextfieldGtk::OnKeyPressEventHandler(
    GtkWidget* widget,
    GdkEventKey* event,
    NativeTextfieldGtk* textfield) {
  return textfield->OnKeyPressEvent(event);
}

gboolean NativeTextfieldGtk::OnKeyPressEvent(GdkEventKey* event) {
  Textfield::Controller* controller = textfield_->GetController();
  if (controller) {
    KeyEvent key_event(reinterpret_cast<GdkEvent*>(event));
    return controller->HandleKeyEvent(textfield_, key_event);
  }
  return false;
}

// static
gboolean NativeTextfieldGtk::OnActivateHandler(
    GtkWidget* widget,
    NativeTextfieldGtk* textfield) {
  return textfield->OnActivate();
}

gboolean NativeTextfieldGtk::OnActivate() {
  GdkEvent* event = gtk_get_current_event();
  if (!event || event->type != GDK_KEY_PRESS)
    return false;

  GdkEventKey* key_event = reinterpret_cast<GdkEventKey*>(event);
  gboolean handled = false;

  Textfield::Controller* controller = textfield_->GetController();
  if (controller) {
    KeyEvent views_key_event(event);
    handled = controller->HandleKeyEvent(textfield_, views_key_event);
  }

  WidgetGtk* widget = static_cast<WidgetGtk*>(GetWidget());
  if (!handled && widget)
    handled = widget->HandleKeyboardEvent(key_event);

  return handled;
}

// static
gboolean NativeTextfieldGtk::OnChangedHandler(
    GtkWidget* widget,
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
  if (textfield_->IsMultiLine()) {
    NativeControlCreated(gtk_views_textview_new(this));
    if (textfield_->draw_border())
      gtk_container_set_border_width(GTK_CONTAINER(native_view()),
                                     kTextViewBorderWidth);

    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(native_view()),
                                GTK_WRAP_WORD_CHAR);
  } else {
    NativeControlCreated(gtk_views_entry_new(this));
    gtk_entry_set_invisible_char(GTK_ENTRY(native_view()),
                                 static_cast<gunichar>(kPasswordChar));
  }
  textfield_->UpdateAllProperties();
}

void NativeTextfieldGtk::NativeControlCreated(GtkWidget* widget) {
  NativeControlGtk::NativeControlCreated(widget);

  if (GTK_IS_TEXT_VIEW(widget)) {
    GtkTextBuffer* text_buffer = gtk_text_view_get_buffer(
        GTK_TEXT_VIEW(widget));
    g_signal_connect(text_buffer, "changed",
                     G_CALLBACK(OnChangedHandler), this);
  } else {
    g_signal_connect(widget, "changed",
                     G_CALLBACK(OnChangedHandler), this);
  }
  g_signal_connect_after(widget, "key-press-event",
                         G_CALLBACK(OnKeyPressEventHandler), this);
  // In order to properly trigger Accelerators bound to VKEY_RETURN, we need to
  // send an event when the widget gets the activate signal.
  g_signal_connect(widget, "activate", G_CALLBACK(OnActivateHandler), this);
}

bool NativeTextfieldGtk::IsPassword() {
  return textfield_->IsPassword();
}

///////////////////////////////////////////////////////////////////////////////
// NativeTextfieldWrapper:

// static
NativeTextfieldWrapper* NativeTextfieldWrapper::CreateWrapper(
    Textfield* field) {
  if (NativeTextfieldViews::IsTextfieldViewsEnabled()) {
    return new NativeTextfieldViews(field);
  }
  return new NativeTextfieldGtk(field);
}

}  // namespace views
