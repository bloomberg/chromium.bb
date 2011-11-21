// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "views/controls/textfield/native_textfield_gtk.h"

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "ui/base/range/range.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/insets.h"
#include "ui/gfx/selection_model.h"
#include "ui/gfx/skia_utils_gtk.h"
#include "ui/views/widget/native_widget_gtk.h"
#include "views/controls/textfield/gtk_views_entry.h"
#include "views/controls/textfield/gtk_views_textview.h"
#include "views/controls/textfield/native_textfield_views.h"
#include "views/controls/textfield/textfield.h"
#include "views/controls/textfield/textfield_controller.h"

namespace views {

// A character used to hide a text in password mode.
static const char kPasswordChar = '*';

// Border width for GtkTextView.
const int kTextViewBorderWidth = 4;

////////////////////////////////////////////////////////////////////////////////
// NativeTextfieldGtk, public:

NativeTextfieldGtk::NativeTextfieldGtk(Textfield* textfield)
    : textfield_(textfield),
      paste_clipboard_requested_(false) {
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
  gtk_editable_set_editable(GTK_EDITABLE(native_view()),
                            !textfield_->read_only());
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
  gtk_entry_set_visibility(GTK_ENTRY(native_view()), !textfield_->IsPassword());
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

  GtkEntry* entry = GTK_ENTRY(widget);
  insets += GetEntryInnerBorder(entry);
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

void NativeTextfieldGtk::UpdateHorizontalMargins() {
  if (!native_view())
    return;

  int left, right;
  if (!textfield_->GetHorizontalMargins(&left, &right))
    return;

  gfx::Insets insets = GetEntryInnerBorder(GTK_ENTRY(native_view()));
  GtkBorder border = {left, right, insets.top(), insets.bottom()};
  gtk_entry_set_inner_border(GTK_ENTRY(native_view()), &border);
}

void NativeTextfieldGtk::UpdateVerticalMargins() {
  if (!native_view())
    return;

  int top, bottom;
  if (!textfield_->GetVerticalMargins(&top, &bottom))
    return;

  gfx::Insets insets = GetEntryInnerBorder(GTK_ENTRY(native_view()));
  GtkBorder border = {insets.left(), insets.right(), top, bottom};
  gtk_entry_set_inner_border(GTK_ENTRY(native_view()), &border);
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

void NativeTextfieldGtk::GetSelectedRange(ui::Range* range) const {
  gint start_pos;
  gint end_pos;
  gtk_editable_get_selection_bounds(
      GTK_EDITABLE(native_view()), &start_pos, &end_pos);
  *range = ui::Range(start_pos, end_pos);
}

void NativeTextfieldGtk::SelectRange(const ui::Range& range) {
  NOTREACHED();
}

void NativeTextfieldGtk::GetSelectionModel(gfx::SelectionModel* sel) const {
  NOTREACHED();
}

void NativeTextfieldGtk::SelectSelectionModel(const gfx::SelectionModel& sel) {
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

ui::TextInputClient* NativeTextfieldGtk::GetTextInputClient() {
  return NULL;
}

void NativeTextfieldGtk::ApplyStyleRange(const gfx::StyleRange& style) {
  NOTREACHED();
}

void NativeTextfieldGtk::ApplyDefaultStyle() {
  NOTREACHED();
}

void NativeTextfieldGtk::ClearEditHistory() {
  NOTREACHED();
}

void NativeTextfieldGtk::OnActivate(GtkWidget* native_widget) {
  GdkEvent* event = gtk_get_current_event();
  if (!event || event->type != GDK_KEY_PRESS)
    return;

  KeyEvent views_key_event(event);
  gboolean handled = false;

  TextfieldController* controller = textfield_->GetController();
  if (controller)
    handled = controller->HandleKeyEvent(textfield_, views_key_event);

  Widget* widget = GetWidget();
  if (!handled && widget) {
    NativeWidgetGtk* native_widget =
        static_cast<NativeWidgetGtk*>(widget->native_widget());
    handled = native_widget->HandleKeyboardEvent(views_key_event);
  }

  // Stop signal emission if the key event is handled by us.
  if (handled) {
    // Only GtkEntry has "activate" signal.
    static guint signal_id = g_signal_lookup("activate", GTK_TYPE_ENTRY);
    g_signal_stop_emission(native_widget, signal_id, 0);
  }
}

void NativeTextfieldGtk::OnChanged(GObject* object) {
  // We need to call TextfieldController::ContentsChanged() explicitly if the
  // paste action didn't change the content at all. See http://crbug.com/79002
  const bool call_contents_changed =
      paste_clipboard_requested_ && GetText() == textfield_->text();
  textfield_->SyncText();
  textfield_->GetWidget()->NotifyAccessibilityEvent(
      textfield_, ui::AccessibilityTypes::EVENT_TEXT_CHANGED, true);
  if (call_contents_changed) {
    TextfieldController* controller = textfield_->GetController();
    if (controller)
      controller->ContentsChanged(textfield_, textfield_->text());
  }
  paste_clipboard_requested_ = false;
}

gboolean NativeTextfieldGtk::OnButtonPressEvent(GtkWidget* widget,
                                                GdkEventButton* event) {
  paste_clipboard_requested_ = false;
  return false;
}

gboolean NativeTextfieldGtk::OnButtonReleaseEventAfter(GtkWidget* widget,
                                                       GdkEventButton* event) {
  textfield_->GetWidget()->NotifyAccessibilityEvent(
      textfield_, ui::AccessibilityTypes::EVENT_TEXT_CHANGED, true);
  return false;
}

gboolean NativeTextfieldGtk::OnKeyPressEvent(GtkWidget* widget,
                                             GdkEventKey* event) {
  paste_clipboard_requested_ = false;
  return false;
}

gboolean NativeTextfieldGtk::OnKeyPressEventAfter(GtkWidget* widget,
                                                  GdkEventKey* event) {
  TextfieldController* controller = textfield_->GetController();
  if (controller) {
    KeyEvent key_event(reinterpret_cast<GdkEvent*>(event));
    return controller->HandleKeyEvent(textfield_, key_event);
  }
  return false;
}

void NativeTextfieldGtk::OnMoveCursor(GtkWidget* widget,
                                      GtkMovementStep step,
                                      gint count,
                                      gboolean extend_selection) {
  textfield_->GetWidget()->NotifyAccessibilityEvent(
      textfield_, ui::AccessibilityTypes::EVENT_TEXT_CHANGED, true);
}

void NativeTextfieldGtk::OnPasteClipboard(GtkWidget* widget) {
  if (!textfield_->read_only())
    paste_clipboard_requested_ = true;
}

////////////////////////////////////////////////////////////////////////////////
// NativeTextfieldGtk, NativeControlGtk overrides:

void NativeTextfieldGtk::CreateNativeControl() {
  NativeControlCreated(gtk_views_entry_new(this));
  gtk_entry_set_invisible_char(GTK_ENTRY(native_view()),
                                 static_cast<gunichar>(kPasswordChar));
  textfield_->UpdateAllProperties();
}

void NativeTextfieldGtk::NativeControlCreated(GtkWidget* widget) {
  NativeControlGtk::NativeControlCreated(widget);

  g_signal_connect(widget, "changed", G_CALLBACK(OnChangedThunk), this);
  // In order to properly trigger Accelerators bound to VKEY_RETURN, we need
  // to send an event when the widget gets the activate signal.
  g_signal_connect(widget, "activate", G_CALLBACK(OnActivateThunk), this);
  g_signal_connect(widget, "move-cursor", G_CALLBACK(OnMoveCursorThunk), this);
  g_signal_connect(widget, "button-press-event",
                   G_CALLBACK(OnButtonPressEventThunk), this);
  g_signal_connect(widget, "key-press-event",
                   G_CALLBACK(OnKeyPressEventThunk), this);
  g_signal_connect(widget, "paste-clipboard",
                   G_CALLBACK(OnPasteClipboardThunk), this);

  g_signal_connect_after(widget, "button-release-event",
                         G_CALLBACK(OnButtonReleaseEventAfterThunk), this);
  g_signal_connect_after(widget, "key-press-event",
                         G_CALLBACK(OnKeyPressEventAfterThunk), this);
}

bool NativeTextfieldGtk::IsPassword() {
  return textfield_->IsPassword();
}

///////////////////////////////////////////////////////////////////////////////
// NativeTextfieldWrapper:

// static
NativeTextfieldWrapper* NativeTextfieldWrapper::CreateWrapper(
    Textfield* field) {
  if (Widget::IsPureViews())
    return new NativeTextfieldViews(field);
  return new NativeTextfieldGtk(field);
}

}  // namespace views
