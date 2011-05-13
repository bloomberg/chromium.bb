// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/textfield/native_textfield_views.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "grit/app_strings.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/range/range.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/insets.h"
#include "views/background.h"
#include "views/border.h"
#include "views/controls/focusable_border.h"
#include "views/controls/menu/menu_2.h"
#include "views/controls/textfield/textfield.h"
#include "views/controls/textfield/textfield_controller.h"
#include "views/controls/textfield/textfield_views_model.h"
#include "views/events/event.h"
#include "views/ime/input_method.h"
#include "views/metrics.h"
#include "views/views_delegate.h"
#include "views/widget/root_view.h"

#if defined(OS_LINUX)
#include "ui/gfx/gtk_util.h"
#endif

namespace {

// A global flag to switch the Textfield wrapper to TextfieldViews.
bool textfield_view_enabled = false;

// Color setttings for text, backgrounds and cursor.
// These are tentative, and should be derived from theme, system
// settings and current settings.
const SkColor kSelectedTextColor = SK_ColorWHITE;
const SkColor kReadonlyTextColor = SK_ColorDKGRAY;
const SkColor kFocusedSelectionColor = SK_ColorBLUE;
const SkColor kUnfocusedSelectionColor = SK_ColorLTGRAY;
const SkColor kCursorColor = SK_ColorBLACK;

// Parameters to control cursor blinking.
const int kCursorVisibleTimeMs = 800;
const int kCursorInvisibleTimeMs = 500;

}  // namespace

namespace views {

const char NativeTextfieldViews::kViewClassName[] =
    "views/NativeTextfieldViews";

NativeTextfieldViews::NativeTextfieldViews(Textfield* parent)
    : textfield_(parent),
      ALLOW_THIS_IN_INITIALIZER_LIST(model_(new TextfieldViewsModel(this))),
      text_border_(new FocusableBorder()),
      text_offset_(0),
      insert_(true),
      is_cursor_visible_(false),
      skip_input_method_cancel_composition_(false),
      initiating_drag_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(cursor_timer_(this)),
      aggregated_clicks_(0),
      last_click_time_(base::Time::FromInternalValue(0)),
      last_click_location_(0, 0) {
  set_border(text_border_);

  // Multiline is not supported.
  DCHECK_NE(parent->style(), Textfield::STYLE_MULTILINE);
  // Lowercase is not supported.
  DCHECK_NE(parent->style(), Textfield::STYLE_LOWERCASE);

  SetContextMenuController(this);
  SetDragController(this);
}

NativeTextfieldViews::~NativeTextfieldViews() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeTextfieldViews, View overrides:

bool NativeTextfieldViews::OnMousePressed(const MouseEvent& event) {
  OnBeforeUserAction();
  textfield_->RequestFocus();

  if (event.IsOnlyLeftMouseButton()) {
    base::TimeDelta time_delta = event.time_stamp() - last_click_time_;
    gfx::Point location_delta = event.location().Subtract(last_click_location_);
    if (time_delta.InMilliseconds() <= GetDoubleClickInterval() &&
        !ExceededDragThreshold(location_delta.x(), location_delta.y())) {
      aggregated_clicks_ = (aggregated_clicks_ + 1) % 3;
    } else {
      aggregated_clicks_ = 0;
    }
    last_click_time_ = event.time_stamp();
    last_click_location_ = event.location();

    initiating_drag_ = false;
    switch(aggregated_clicks_) {
      case 0:
        if (!IsPointInSelection(event.location()))
          MoveCursorTo(event.location(), event.IsShiftDown());
        else
          initiating_drag_ = true;
        break;
      case 1:
        model_->SelectWord();
        break;
      case 2:
        model_->SelectAll();
        break;
      default:
        NOTREACHED();
    }
    SchedulePaint();
  }

  OnAfterUserAction();
  return true;
}

bool NativeTextfieldViews::OnMouseDragged(const MouseEvent& event) {
  // Don't adjust the cursor on a potential drag and drop.
  if (initiating_drag_)
    return true;

  OnBeforeUserAction();
  if (MoveCursorTo(event.location(), true))
    SchedulePaint();
  OnAfterUserAction();
  return true;
}

void NativeTextfieldViews::OnMouseReleased(const MouseEvent& event) {
  OnBeforeUserAction();
  // Cancel suspected drag initiations, the user was clicking in the selection.
  if (initiating_drag_ && MoveCursorTo(event.location(), false))
    SchedulePaint();
  initiating_drag_ = false;
  OnAfterUserAction();
}

bool NativeTextfieldViews::OnKeyPressed(const KeyEvent& event) {
  // OnKeyPressed/OnKeyReleased/OnFocus/OnBlur will never be invoked on
  // NativeTextfieldViews as it will never gain focus.
  NOTREACHED();
  return false;
}

bool NativeTextfieldViews::OnKeyReleased(const KeyEvent& event) {
  NOTREACHED();
  return false;
}

bool NativeTextfieldViews::GetDropFormats(
    int* formats,
    std::set<OSExchangeData::CustomFormat>* custom_formats) {
  if (!textfield_->IsEnabled() || textfield_->read_only())
    return false;
  // TODO(msw): Can we support URL, FILENAME, etc.?
  *formats = ui::OSExchangeData::STRING;
  return true;
}

bool NativeTextfieldViews::CanDrop(const OSExchangeData& data) {
  return textfield_->IsEnabled() && !textfield_->read_only() &&
         data.HasString();
}

int NativeTextfieldViews::OnDragUpdated(const DropTargetEvent& event) {
  // TODO(msw): retain unfocused selection, render secondary cursor...
  DCHECK(CanDrop(event.data()));
  if (initiating_drag_) {
    if (IsPointInSelection(event.location()))
      return ui::DragDropTypes::DRAG_NONE;
    return event.IsControlDown() ? ui::DragDropTypes::DRAG_COPY :
                                   ui::DragDropTypes::DRAG_MOVE;
  }
  return ui::DragDropTypes::DRAG_COPY | ui::DragDropTypes::DRAG_MOVE;
}

int NativeTextfieldViews::OnPerformDrop(const DropTargetEvent& event) {
  DCHECK(CanDrop(event.data()));
  DCHECK(!initiating_drag_ || !IsPointInSelection(event.location()));
  OnBeforeUserAction();
  skip_input_method_cancel_composition_ = true;

  size_t drop_destination = FindCursorPosition(event.location());
  string16 text;
  event.data().GetString(&text);

  // We'll delete the current selection for a drag and drop within this view.
  bool move = initiating_drag_ && !event.IsControlDown() &&
              event.source_operations() & ui::DragDropTypes::DRAG_MOVE;
  if (move) {
    ui::Range selected_range;
    model_->GetSelectedRange(&selected_range);
    // Adjust the drop destination if it is on or after the current selection.
    if (selected_range.GetMax() <= drop_destination)
      drop_destination -= selected_range.length();
    else if (selected_range.GetMin() <= drop_destination)
      drop_destination = selected_range.GetMin();
    model_->DeleteSelectionAndInsertTextAt(text, drop_destination);
  } else {
    model_->MoveCursorTo(drop_destination, false);
    // Drop always inserts a text even if insert_ == false.
    model_->InsertText(text);
  }
  skip_input_method_cancel_composition_ = false;
  UpdateAfterChange(true, true);
  OnAfterUserAction();
  return move ? ui::DragDropTypes::DRAG_MOVE : ui::DragDropTypes::DRAG_COPY;
}

void NativeTextfieldViews::OnDragDone() {
  initiating_drag_ = false;
}

void NativeTextfieldViews::OnPaint(gfx::Canvas* canvas) {
  text_border_->set_has_focus(textfield_->HasFocus());
  OnPaintBackground(canvas);
  PaintTextAndCursor(canvas);
  if (textfield_->draw_border())
    OnPaintBorder(canvas);
}

void NativeTextfieldViews::OnFocus() {
  NOTREACHED();
}

void NativeTextfieldViews::OnBlur() {
  NOTREACHED();
}

gfx::NativeCursor NativeTextfieldViews::GetCursor(const MouseEvent& event) {
  bool text = !initiating_drag_ && (event.type() == ui::ET_MOUSE_DRAGGED ||
                                    !IsPointInSelection(event.location()));
#if defined(OS_WIN)
  static HCURSOR ibeam = LoadCursor(NULL, IDC_IBEAM);
  static HCURSOR arrow = LoadCursor(NULL, IDC_ARROW);
  return text ? ibeam : arrow;
#else
  return text ? gfx::GetCursor(GDK_XTERM) : NULL;
#endif
}

/////////////////////////////////////////////////////////////////
// NativeTextfieldViews, ContextMenuController overrides:
void NativeTextfieldViews::ShowContextMenuForView(View* source,
                                                  const gfx::Point& p,
                                                  bool is_mouse_gesture) {
  InitContextMenuIfRequired();
  context_menu_menu_->RunContextMenuAt(p);
}

/////////////////////////////////////////////////////////////////
// NativeTextfieldViews, views::DragController overrides:
void NativeTextfieldViews::WriteDragDataForView(views::View* sender,
                                                const gfx::Point& press_pt,
                                                OSExchangeData* data) {
  DCHECK_NE(ui::DragDropTypes::DRAG_NONE,
            GetDragOperationsForView(sender, press_pt));
  data->SetString(GetSelectedText());
}

int NativeTextfieldViews::GetDragOperationsForView(views::View* sender,
                                                   const gfx::Point& p) {
  if (!textfield_->IsEnabled() || !IsPointInSelection(p))
    return ui::DragDropTypes::DRAG_NONE;
  if (sender == this && !textfield_->read_only())
    return ui::DragDropTypes::DRAG_MOVE | ui::DragDropTypes::DRAG_COPY;
  return ui::DragDropTypes::DRAG_COPY;
}

bool NativeTextfieldViews::CanStartDragForView(View* sender,
                                               const gfx::Point& press_pt,
                                               const gfx::Point& p) {
  return IsPointInSelection(press_pt);
}

/////////////////////////////////////////////////////////////////
// NativeTextfieldViews, NativeTextifieldWrapper overrides:

string16 NativeTextfieldViews::GetText() const {
  return model_->text();
}

void NativeTextfieldViews::UpdateText() {
  model_->SetText(textfield_->text());
  UpdateCursorBoundsAndTextOffset();
  SchedulePaint();
}

void NativeTextfieldViews::AppendText(const string16& text) {
  if (text.empty())
    return;
  model_->Append(text);
  UpdateCursorBoundsAndTextOffset();
  SchedulePaint();
}

string16 NativeTextfieldViews::GetSelectedText() const {
  return model_->GetSelectedText();
}

void NativeTextfieldViews::SelectAll() {
  model_->SelectAll();
  SchedulePaint();
}

void NativeTextfieldViews::ClearSelection() {
  model_->ClearSelection();
  SchedulePaint();
}

void NativeTextfieldViews::UpdateBorder() {
  if (textfield_->draw_border()) {
    gfx::Insets insets = GetInsets();
    textfield_->SetHorizontalMargins(insets.left(), insets.right());
    textfield_->SetVerticalMargins(insets.top(), insets.bottom());
  } else {
    textfield_->SetHorizontalMargins(0, 0);
    textfield_->SetVerticalMargins(0, 0);
  }
}

void NativeTextfieldViews::UpdateTextColor() {
  SchedulePaint();
}

void NativeTextfieldViews::UpdateBackgroundColor() {
  // TODO(oshima): Background has to match the border's shape.
  set_background(
      Background::CreateSolidBackground(textfield_->background_color()));
  SchedulePaint();
}

void NativeTextfieldViews::UpdateReadOnly() {
  SchedulePaint();
  OnTextInputTypeChanged();
}

void NativeTextfieldViews::UpdateFont() {
  UpdateCursorBoundsAndTextOffset();
}

void NativeTextfieldViews::UpdateIsPassword() {
  model_->set_is_password(textfield_->IsPassword());
  UpdateCursorBoundsAndTextOffset();
  SchedulePaint();
  OnTextInputTypeChanged();
}

void NativeTextfieldViews::UpdateEnabled() {
  SetEnabled(textfield_->IsEnabled());
  SchedulePaint();
  OnTextInputTypeChanged();
}

gfx::Insets NativeTextfieldViews::CalculateInsets() {
  return GetInsets();
}

void NativeTextfieldViews::UpdateHorizontalMargins() {
  int left, right;
  if (!textfield_->GetHorizontalMargins(&left, &right))
    return;
  gfx::Insets inset = GetInsets();

  text_border_->SetInsets(inset.top(), left, inset.bottom(), right);
  UpdateCursorBoundsAndTextOffset();
}

void NativeTextfieldViews::UpdateVerticalMargins() {
  int top, bottom;
  if (!textfield_->GetVerticalMargins(&top, &bottom))
    return;
  gfx::Insets inset = GetInsets();

  text_border_->SetInsets(top, inset.left(), bottom, inset.right());
  UpdateCursorBoundsAndTextOffset();
}

bool NativeTextfieldViews::SetFocus() {
  return false;
}

View* NativeTextfieldViews::GetView() {
  return this;
}

gfx::NativeView NativeTextfieldViews::GetTestingHandle() const {
  NOTREACHED();
  return NULL;
}

bool NativeTextfieldViews::IsIMEComposing() const {
  return model_->HasCompositionText();
}

void NativeTextfieldViews::GetSelectedRange(ui::Range* range) const {
  model_->GetSelectedRange(range);
}

void NativeTextfieldViews::SelectRange(const ui::Range& range) {
  model_->SelectRange(range);
  UpdateCursorBoundsAndTextOffset();
  SchedulePaint();
}

size_t NativeTextfieldViews::GetCursorPosition() const {
  return model_->cursor_pos();
}

bool NativeTextfieldViews::HandleKeyPressed(const KeyEvent& e) {
  TextfieldController* controller = textfield_->GetController();
  bool handled = false;
  if (controller)
    handled = controller->HandleKeyEvent(textfield_, e);
  return handled || HandleKeyEvent(e);
}

bool NativeTextfieldViews::HandleKeyReleased(const KeyEvent& e) {
  return true;
}

void NativeTextfieldViews::HandleFocus() {
  is_cursor_visible_ = true;
  SchedulePaint();
  OnCaretBoundsChanged();
  // Start blinking cursor.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      cursor_timer_.NewRunnableMethod(&NativeTextfieldViews::UpdateCursor),
      kCursorVisibleTimeMs);
}

void NativeTextfieldViews::HandleBlur() {
  // Stop blinking cursor.
  cursor_timer_.RevokeAll();
  if (is_cursor_visible_) {
    is_cursor_visible_ = false;
    RepaintCursor();
  }
}

TextInputClient* NativeTextfieldViews::GetTextInputClient() {
  return textfield_->read_only() ? NULL : this;
}

/////////////////////////////////////////////////////////////////
// NativeTextfieldViews, ui::SimpleMenuModel::Delegate overrides:

bool NativeTextfieldViews::IsCommandIdChecked(int command_id) const {
  return true;
}

bool NativeTextfieldViews::IsCommandIdEnabled(int command_id) const {
  bool editable = !textfield_->read_only();
  string16 result;
  switch (command_id) {
    case IDS_APP_CUT:
      return editable && model_->HasSelection();
    case IDS_APP_COPY:
      return model_->HasSelection();
    case IDS_APP_PASTE:
      ViewsDelegate::views_delegate->GetClipboard()
          ->ReadText(ui::Clipboard::BUFFER_STANDARD, &result);
      return editable && !result.empty();
    case IDS_APP_DELETE:
      return editable && model_->HasSelection();
    case IDS_APP_SELECT_ALL:
      return true;
    default:
      NOTREACHED();
      return false;
  }
}

bool NativeTextfieldViews::GetAcceleratorForCommandId(int command_id,
    ui::Accelerator* accelerator) {
  return false;
}

void NativeTextfieldViews::ExecuteCommand(int command_id) {
  bool text_changed = false;
  bool editable = !textfield_->read_only();
  OnBeforeUserAction();
  switch (command_id) {
    case IDS_APP_CUT:
      if (editable)
        text_changed = model_->Cut();
      break;
    case IDS_APP_COPY:
      model_->Copy();
      break;
    case IDS_APP_PASTE:
      if (editable)
        text_changed = Paste();
      break;
    case IDS_APP_DELETE:
      if (editable)
        text_changed = model_->Delete();
      break;
    case IDS_APP_SELECT_ALL:
      SelectAll();
      break;
    default:
      NOTREACHED() << "unknown command: " << command_id;
      break;
  }

  // The cursor must have changed if text changed during cut/paste/delete.
  UpdateAfterChange(text_changed, text_changed);
  OnAfterUserAction();
}

// static
bool NativeTextfieldViews::IsTextfieldViewsEnabled() {
#if defined(TOUCH_UI)
  return true;
#else
  return textfield_view_enabled || RootView::IsPureViews();
#endif
}

// static
void NativeTextfieldViews::SetEnableTextfieldViews(bool enabled) {
  textfield_view_enabled = enabled;
}

void NativeTextfieldViews::OnBoundsChanged(const gfx::Rect& previous_bounds) {
  UpdateCursorBoundsAndTextOffset();
}


///////////////////////////////////////////////////////////////////////////////
// NativeTextfieldViews, TextInputClient implementation, private:

void NativeTextfieldViews::SetCompositionText(
    const ui::CompositionText& composition) {
  if (GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE)
    return;

  OnBeforeUserAction();
  skip_input_method_cancel_composition_ = true;
  model_->SetCompositionText(composition);
  skip_input_method_cancel_composition_ = false;
  UpdateAfterChange(true, true);
  OnAfterUserAction();
}

void NativeTextfieldViews::ConfirmCompositionText() {
  if (!model_->HasCompositionText())
    return;

  OnBeforeUserAction();
  skip_input_method_cancel_composition_ = true;
  model_->ConfirmCompositionText();
  skip_input_method_cancel_composition_ = false;
  UpdateAfterChange(true, true);
  OnAfterUserAction();
}

void NativeTextfieldViews::ClearCompositionText() {
  if (!model_->HasCompositionText())
    return;

  OnBeforeUserAction();
  skip_input_method_cancel_composition_ = true;
  model_->ClearCompositionText();
  skip_input_method_cancel_composition_ = false;
  UpdateAfterChange(true, true);
  OnAfterUserAction();
}

void NativeTextfieldViews::InsertText(const string16& text) {
  // TODO(suzhe): Filter invalid characters.
  if (GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE || text.empty())
    return;

  OnBeforeUserAction();
  skip_input_method_cancel_composition_ = true;
  if (insert_)
    model_->InsertText(text);
  else
    model_->ReplaceText(text);
  skip_input_method_cancel_composition_ = false;
  UpdateAfterChange(true, true);
  OnAfterUserAction();
}

void NativeTextfieldViews::InsertChar(char16 ch, int flags) {
  if (GetTextInputType() == ui::TEXT_INPUT_TYPE_NONE ||
      !ShouldInsertChar(ch, flags)) {
    return;
  }

  OnBeforeUserAction();
  skip_input_method_cancel_composition_ = true;
  if (insert_)
    model_->InsertChar(ch);
  else
    model_->ReplaceChar(ch);
  skip_input_method_cancel_composition_ = false;
  UpdateAfterChange(true, true);
  OnAfterUserAction();
}

ui::TextInputType NativeTextfieldViews::GetTextInputType() {
  if (textfield_->read_only() || !textfield_->IsEnabled())
    return ui::TEXT_INPUT_TYPE_NONE;
  else if (textfield_->IsPassword())
    return ui::TEXT_INPUT_TYPE_PASSWORD;
  return ui::TEXT_INPUT_TYPE_TEXT;
}

gfx::Rect NativeTextfieldViews::GetCaretBounds() {
  return cursor_bounds_;
}

bool NativeTextfieldViews::HasCompositionText() {
  return model_->HasCompositionText();
}

bool NativeTextfieldViews::GetTextRange(ui::Range* range) {
  // We don't allow the input method to retrieve or delete content from a
  // password box.
  if (GetTextInputType() != ui::TEXT_INPUT_TYPE_TEXT)
    return false;

  model_->GetTextRange(range);
  return true;
}

bool NativeTextfieldViews::GetCompositionTextRange(ui::Range* range) {
  if (GetTextInputType() != ui::TEXT_INPUT_TYPE_TEXT)
    return false;

  model_->GetCompositionTextRange(range);
  return true;
}

bool NativeTextfieldViews::GetSelectionRange(ui::Range* range) {
  if (GetTextInputType() != ui::TEXT_INPUT_TYPE_TEXT)
    return false;

  model_->GetSelectedRange(range);
  return true;
}

bool NativeTextfieldViews::SetSelectionRange(const ui::Range& range) {
  if (GetTextInputType() != ui::TEXT_INPUT_TYPE_TEXT || !range.IsValid())
    return false;

  OnBeforeUserAction();
  SelectRange(range);
  OnAfterUserAction();
  return true;
}

bool NativeTextfieldViews::DeleteRange(const ui::Range& range) {
  if (GetTextInputType() != ui::TEXT_INPUT_TYPE_TEXT || range.is_empty())
    return false;

  OnBeforeUserAction();
  model_->SelectRange(range);
  if (model_->HasSelection()) {
    model_->DeleteSelection();
    UpdateAfterChange(true, true);
  }
  OnAfterUserAction();
  return true;
}

bool NativeTextfieldViews::GetTextFromRange(
    const ui::Range& range,
    const base::Callback<void(const string16&)>& callback) {
  if (GetTextInputType() != ui::TEXT_INPUT_TYPE_TEXT || range.is_empty())
    return false;

  callback.Run(model_->GetTextFromRange(range));
  return true;
}

void NativeTextfieldViews::OnInputMethodChanged() {
  NOTIMPLEMENTED();
}

bool NativeTextfieldViews::ChangeTextDirectionAndLayoutAlignment(
    base::i18n::TextDirection direction) {
  NOTIMPLEMENTED();
  return false;
}

View* NativeTextfieldViews::GetOwnerViewOfTextInputClient() {
  return textfield_;
}

void NativeTextfieldViews::OnCompositionTextConfirmedOrCleared() {
  if (skip_input_method_cancel_composition_)
    return;
  DCHECK(textfield_->GetInputMethod());
  textfield_->GetInputMethod()->CancelComposition(textfield_);
}

const gfx::Font& NativeTextfieldViews::GetFont() const {
  return textfield_->font();
}

SkColor NativeTextfieldViews::GetTextColor() const {
  return textfield_->text_color();
}

void NativeTextfieldViews::UpdateCursor() {
  is_cursor_visible_ = !is_cursor_visible_;
  RepaintCursor();
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      cursor_timer_.NewRunnableMethod(&NativeTextfieldViews::UpdateCursor),
      is_cursor_visible_ ? kCursorVisibleTimeMs : kCursorInvisibleTimeMs);
}

void NativeTextfieldViews::RepaintCursor() {
  gfx::Rect r = cursor_bounds_;
  r.Inset(-1, -1, -1, -1);
  SchedulePaintInRect(r);
}

void NativeTextfieldViews::UpdateCursorBoundsAndTextOffset() {
  if (bounds().IsEmpty())
    return;

  gfx::Insets insets = GetInsets();

  int width = bounds().width() - insets.width();

  // TODO(oshima): bidi
  const gfx::Font& font = GetFont();
  int full_width = font.GetStringWidth(model_->GetVisibleText());
  int cursor_height = std::min(height() - insets.height(), font.GetHeight());

  cursor_bounds_ = model_->GetCursorBounds(font);
  cursor_bounds_.set_y((height() - cursor_height) / 2);
  cursor_bounds_.set_height(cursor_height);

  int x_right = text_offset_ + cursor_bounds_.right();
  int x_left = text_offset_ + cursor_bounds_.x();

  if (full_width < width) {
    // Show all text whenever the text fits to the size.
    text_offset_ = 0;
  } else if (x_right > width) {
    // when the cursor overflows to the right
    text_offset_ = width - cursor_bounds_.right();
  } else if (x_left < 0) {
    // when the cursor overflows to the left
    text_offset_ = -cursor_bounds_.x();
  } else if (full_width > width && text_offset_ + full_width < width) {
    // when the cursor moves within the textfield with the text
    // longer than the field.
    text_offset_ = width - full_width;
  } else {
    // move cursor freely.
  }
  // shift cursor bounds to fit insets.
  cursor_bounds_.set_x(cursor_bounds_.x() + text_offset_ + insets.left());

  OnCaretBoundsChanged();
}

void NativeTextfieldViews::PaintTextAndCursor(gfx::Canvas* canvas) {
  gfx::Insets insets = GetInsets();

  canvas->Save();
  canvas->ClipRectInt(insets.left(), insets.top(),
                      width() - insets.width(), height() - insets.height());

  // TODO(oshima): bidi support
  // TODO(varunjain): re-implement this so only that dirty text is painted.
  TextfieldViewsModel::TextFragments fragments;
  model_->GetFragments(&fragments);
  int x_offset = text_offset_ + insets.left();
  int y = insets.top();
  int text_height = height() - insets.height();
  SkColor selection_color =
      textfield_->HasFocus() ?
      kFocusedSelectionColor : kUnfocusedSelectionColor;
  SkColor text_color =
      textfield_->read_only() ? kReadonlyTextColor : GetTextColor();

  for (TextfieldViewsModel::TextFragments::const_iterator iter =
           fragments.begin();
       iter != fragments.end();
       iter++) {
    string16 text = model_->GetVisibleText(iter->start, iter->end);

    gfx::Font font = GetFont();
    if (iter->underline)
      font = font.DeriveFont(0, font.GetStyle() | gfx::Font::UNDERLINED);

    // TODO(oshima): This does not give the accurate position due to
    // kerning. Figure out how webkit does this with skia.
    int width = font.GetStringWidth(text);

    if (iter->selected) {
      canvas->FillRectInt(selection_color, x_offset, y, width, text_height);
      canvas->DrawStringInt(text, font, kSelectedTextColor,
                            x_offset, y, width, text_height);
    } else {
      canvas->DrawStringInt(text, font, text_color,
                            x_offset, y, width, text_height);
    }
    x_offset += width;
  }
  canvas->Restore();

  if (textfield_->IsEnabled() && is_cursor_visible_ &&
      !model_->HasSelection()) {
    // Paint Cursor. Replace cursor is drawn as rectangle for now.
    canvas->DrawRectInt(kCursorColor,
                        cursor_bounds_.x(),
                        cursor_bounds_.y(),
                        insert_ ? 0 : cursor_bounds_.width(),
                        cursor_bounds_.height());
  }
}

bool NativeTextfieldViews::HandleKeyEvent(const KeyEvent& key_event) {
  // TODO(oshima): Refactor and consolidate with ExecuteCommand.
  if (key_event.type() == ui::ET_KEY_PRESSED) {
    ui::KeyboardCode key_code = key_event.key_code();
    // TODO(oshima): shift-tab does not work. Figure out why and fix.
    if (key_code == ui::VKEY_TAB)
      return false;

    OnBeforeUserAction();
    bool editable = !textfield_->read_only();
    bool selection = key_event.IsShiftDown();
    bool control = key_event.IsControlDown();
    bool text_changed = false;
    bool cursor_changed = false;
    switch (key_code) {
      case ui::VKEY_Z:
        if (control && editable)
          cursor_changed = text_changed = model_->Undo();
        break;
      case ui::VKEY_Y:
        if (control && editable)
          cursor_changed = text_changed = model_->Redo();
        break;
      case ui::VKEY_A:
        if (control) {
          model_->SelectAll();
          cursor_changed = true;
        }
        break;
      case ui::VKEY_X:
        if (control && editable)
          cursor_changed = text_changed = model_->Cut();
        break;
      case ui::VKEY_C:
        if (control)
          model_->Copy();
        break;
      case ui::VKEY_V:
        if (control && editable)
          cursor_changed = text_changed = Paste();
        break;
      case ui::VKEY_RIGHT:
        control ? model_->MoveCursorToNextWord(selection)
            : model_->MoveCursorRight(selection);
        cursor_changed = true;
        break;
      case ui::VKEY_LEFT:
        control ? model_->MoveCursorToPreviousWord(selection)
            : model_->MoveCursorLeft(selection);
        cursor_changed = true;
        break;
      case ui::VKEY_END:
        model_->MoveCursorToEnd(selection);
        cursor_changed = true;
        break;
      case ui::VKEY_HOME:
        model_->MoveCursorToHome(selection);
        cursor_changed = true;
        break;
      case ui::VKEY_BACK:
        if (!editable)
          break;
        if (!model_->HasSelection()) {
          if (selection && control) {
            // If both shift and control are pressed, then erase upto the
            // beginning of the buffer in ChromeOS. In windows, do nothing.
#if defined(OS_WIN)
            break;
#else
            model_->MoveCursorToHome(true);
#endif
          } else if (control) {
            // If only control is pressed, then erase the previous word.
            model_->MoveCursorToPreviousWord(true);
          }
        }
        text_changed = model_->Backspace();
        cursor_changed = true;
        break;
      case ui::VKEY_DELETE:
        if (!editable)
          break;
        if (!model_->HasSelection()) {
          if (selection && control) {
            // If both shift and control are pressed, then erase upto the
            // end of the buffer in ChromeOS. In windows, do nothing.
#if defined(OS_WIN)
            break;
#else
            model_->MoveCursorToEnd(true);
#endif
          } else if (control) {
            // If only control is pressed, then erase the next word.
            model_->MoveCursorToNextWord(true);
          }
        }
        cursor_changed = text_changed = model_->Delete();
        break;
      case ui::VKEY_INSERT:
        insert_ = !insert_;
        cursor_changed = true;
        break;
      default:
        break;
    }

    // We must have input method in order to support text input.
    DCHECK(textfield_->GetInputMethod());

    UpdateAfterChange(text_changed, cursor_changed);
    OnAfterUserAction();
    return (text_changed || cursor_changed);
  }
  return false;
}

size_t NativeTextfieldViews::FindCursorPosition(const gfx::Point& point) const {
  // TODO(oshima): BIDI/i18n support.
  gfx::Font font = GetFont();
  gfx::Insets insets = GetInsets();
  string16 text = model_->GetVisibleText();
  int left = 0;
  int left_pos = 0;
  int right = font.GetStringWidth(text);
  int right_pos = text.length();

  int x = point.x() - insets.left() - text_offset_;
  if (x <= left) return left_pos;
  if (x >= right) return right_pos;
  // binary searching the cursor position.
  // TODO(oshima): use the center of character instead of edge.
  // Binary search may not work for language like arabic.
  while (std::abs(static_cast<long>(right_pos - left_pos) > 1)) {
    int pivot_pos = left_pos + (right_pos - left_pos) / 2;
    int pivot = font.GetStringWidth(text.substr(0, pivot_pos));
    if (pivot < x) {
      left = pivot;
      left_pos = pivot_pos;
    } else if (pivot == x) {
      return pivot_pos;
    } else {
      right = pivot;
      right_pos = pivot_pos;
    }
  }
  return left_pos;
}

bool NativeTextfieldViews::IsPointInSelection(const gfx::Point& point) const {
  ui::Range range;
  GetSelectedRange(&range);
  size_t pos = FindCursorPosition(point);
  return (pos >= range.GetMin() && pos < range.GetMax());
}

bool NativeTextfieldViews::MoveCursorTo(const gfx::Point& point, bool select) {
  size_t pos = FindCursorPosition(point);
  if (model_->MoveCursorTo(pos, select)) {
    UpdateCursorBoundsAndTextOffset();
    return true;
  }
  return false;
}

void NativeTextfieldViews::PropagateTextChange() {
  textfield_->SyncText();
}

void NativeTextfieldViews::UpdateAfterChange(bool text_changed,
                                             bool cursor_changed) {
  if (text_changed)
    PropagateTextChange();
  if (cursor_changed) {
    is_cursor_visible_ = true;
    RepaintCursor();
  }
  if (text_changed || cursor_changed) {
    UpdateCursorBoundsAndTextOffset();
    SchedulePaint();
  }
}

void NativeTextfieldViews::InitContextMenuIfRequired() {
  if (context_menu_menu_.get())
    return;
  context_menu_contents_.reset(new ui::SimpleMenuModel(this));
  context_menu_contents_->AddItemWithStringId(IDS_APP_CUT, IDS_APP_CUT);
  context_menu_contents_->AddItemWithStringId(IDS_APP_COPY, IDS_APP_COPY);
  context_menu_contents_->AddItemWithStringId(IDS_APP_PASTE, IDS_APP_PASTE);
  context_menu_contents_->AddItemWithStringId(IDS_APP_DELETE, IDS_APP_DELETE);
  context_menu_contents_->AddSeparator();
  context_menu_contents_->AddItemWithStringId(IDS_APP_SELECT_ALL,
                                              IDS_APP_SELECT_ALL);
  context_menu_menu_.reset(new Menu2(context_menu_contents_.get()));
}

void NativeTextfieldViews::OnTextInputTypeChanged() {
  // TODO(suzhe): changed from DCHECK. See http://crbug.com/81320.
  if (textfield_->GetInputMethod())
    textfield_->GetInputMethod()->OnTextInputTypeChanged(textfield_);
}

void NativeTextfieldViews::OnCaretBoundsChanged() {
  // TODO(suzhe): changed from DCHECK. See http://crbug.com/81320.
  if (textfield_->GetInputMethod())
    textfield_->GetInputMethod()->OnCaretBoundsChanged(textfield_);
}

void NativeTextfieldViews::OnBeforeUserAction() {
  TextfieldController* controller = textfield_->GetController();
  if (controller)
    controller->OnBeforeUserAction(textfield_);
}

void NativeTextfieldViews::OnAfterUserAction() {
  TextfieldController* controller = textfield_->GetController();
  if (controller)
    controller->OnAfterUserAction(textfield_);
}

bool NativeTextfieldViews::Paste() {
  const bool success = model_->Paste();

  // Calls TextfieldController::ContentsChanged() explicitly if the paste action
  // did not change the content at all. See http://crbug.com/79002
  if (success && model_->text() == textfield_->text()) {
    TextfieldController* controller = textfield_->GetController();
    if (controller)
      controller->ContentsChanged(textfield_, textfield_->text());
  }
  return success;
}

// static
bool NativeTextfieldViews::ShouldInsertChar(char16 ch, int flags) {
  // Filter out all control characters, including tab and new line characters,
  // and all characters with Alt modifier. But we need to allow characters with
  // AltGr modifier.
  // On Windows AltGr is represented by Alt+Ctrl, and on Linux it's a different
  // flag that we don't care about.
  return ((ch >= 0x20 && ch < 0x7F) || ch > 0x9F) &&
      (flags & ~(ui::EF_SHIFT_DOWN | ui::EF_CAPS_LOCK_DOWN)) != ui::EF_ALT_DOWN;
}

}  // namespace views
