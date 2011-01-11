// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "views/controls/textfield/native_textfield_views.h"

#include <algorithm>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "gfx/canvas.h"
#include "gfx/canvas_skia.h"
#include "gfx/insets.h"
#include "views/background.h"
#include "views/border.h"
#include "views/controls/textfield/native_textfield_gtk.h"
#include "views/controls/textfield/textfield.h"
#include "views/controls/textfield/textfield_views_model.h"
#include "views/event.h"

namespace {

// A global flag to switch the Textfield wrapper to TextfieldViews.
bool textfield_view_enabled = false;

// Color setttings for text, border, backgrounds and cursor.
// These are tentative, and should be derived from theme, system
// settings and current settings.
const SkColor kSelectedTextColor = SK_ColorWHITE;
const SkColor kReadonlyTextColor = SK_ColorDKGRAY;
const SkColor kFocusedSelectionColor = SK_ColorBLUE;
const SkColor kUnfocusedSelectionColor = SK_ColorLTGRAY;
const SkColor kFocusedBorderColor = SK_ColorCYAN;
const SkColor kDefaultBorderColor = SK_ColorGRAY;
const SkColor kCursorColor = SK_ColorBLACK;

// Parameters to control cursor blinking.
const int kCursorVisibleTimeMs = 800;
const int kCursorInvisibleTimeMs = 500;

// A switch to enable NativeTextfieldViews;
const char kEnableViewsBasedTextfieldSwitch[] = "enable-textfield-view";
}  // namespace

namespace views {

const char NativeTextfieldViews::kViewClassName[] =
    "views/NativeTextfieldViews";

NativeTextfieldViews::NativeTextfieldViews(Textfield* parent)
    : textfield_(parent),
      model_(new TextfieldViewsModel()),
      text_border_(new TextfieldBorder()),
      text_offset_(0),
      insert_(true),
      is_cursor_visible_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(cursor_timer_(this)) {
  set_border(text_border_);

  // Multiline is not supported.
  DCHECK_NE(parent->style(), Textfield::STYLE_MULTILINE);
  // Lowercase is not supported.
  DCHECK_NE(parent->style(), Textfield::STYLE_LOWERCASE);
}

NativeTextfieldViews::~NativeTextfieldViews() {
}

////////////////////////////////////////////////////////////////////////////////
// NativeTextfieldViews, View overrides:

bool NativeTextfieldViews::OnMousePressed(const views::MouseEvent& e) {
  textfield_->RequestFocus();
  size_t pos = FindCursorPosition(e.location());
  if (model_->MoveCursorTo(pos, false)) {
    UpdateCursorBoundsAndTextOffset();
    SchedulePaint();
  }
  return true;
}

bool NativeTextfieldViews::OnMouseDragged(const views::MouseEvent& e) {
  size_t pos = FindCursorPosition(e.location());
  if (model_->MoveCursorTo(pos, true)) {
    UpdateCursorBoundsAndTextOffset();
    SchedulePaint();
  }
  return true;
}

void NativeTextfieldViews::OnMouseReleased(const views::MouseEvent& e,
                                          bool canceled) {
}

bool NativeTextfieldViews::OnKeyPressed(const views::KeyEvent& e) {
  // OnKeyPressed/OnKeyReleased/WillGainFocus/DidGainFocus/WillLoseFocus
  // will never be invoked on NativeTextfieldViews as it will never
  // gain focus.
  NOTREACHED();
  return false;
}

bool NativeTextfieldViews::OnKeyReleased(const views::KeyEvent& e) {
  NOTREACHED();
  return false;
}

void NativeTextfieldViews::Paint(gfx::Canvas* canvas) {
  text_border_->set_has_focus(textfield_->HasFocus());
  PaintBackground(canvas);
  PaintTextAndCursor(canvas);
  if (textfield_->draw_border())
    PaintBorder(canvas);
}

void NativeTextfieldViews::DidChangeBounds(const gfx::Rect& previous,
                                           const gfx::Rect& current) {
  UpdateCursorBoundsAndTextOffset();
}

void NativeTextfieldViews::WillGainFocus() {
  NOTREACHED();
}

void NativeTextfieldViews::DidGainFocus() {
  NOTREACHED();
}

void NativeTextfieldViews::WillLoseFocus() {
  NOTREACHED();
}

/////////////////////////////////////////////////////////////////
// NativeTextfieldViews, NativeTextifieldWrapper overrides:

string16 NativeTextfieldViews::GetText() const {
  return model_->text();
}

void NativeTextfieldViews::UpdateText() {
  bool changed = model_->SetText(textfield_->text());
  UpdateCursorBoundsAndTextOffset();
  SchedulePaint();
  if (changed) {
    Textfield::Controller* controller = textfield_->GetController();
    if (controller)
      controller->ContentsChanged(textfield_, GetText());
  }
}

void NativeTextfieldViews::AppendText(const string16& text) {
  if (text.empty())
    return;
  model_->Append(text);
  UpdateCursorBoundsAndTextOffset();
  SchedulePaint();

  Textfield::Controller* controller = textfield_->GetController();
  if (controller)
    controller->ContentsChanged(textfield_, GetText());
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
}

void NativeTextfieldViews::UpdateFont() {
  UpdateCursorBoundsAndTextOffset();
}

void NativeTextfieldViews::UpdateIsPassword() {
  model_->set_is_password(textfield_->IsPassword());
  UpdateCursorBoundsAndTextOffset();
  SchedulePaint();
}

void NativeTextfieldViews::UpdateEnabled() {
  SetEnabled(textfield_->IsEnabled());
  SchedulePaint();
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
  return false;
}

bool NativeTextfieldViews::HandleKeyPressed(const views::KeyEvent& e) {
  Textfield::Controller* controller = textfield_->GetController();
  bool handled = false;
  if (controller) {
    handled = controller->HandleKeyEvent(textfield_, e);
  }
  return handled || HandleKeyEvent(e);
}

bool NativeTextfieldViews::HandleKeyReleased(const views::KeyEvent& e) {
  return true;
}

void NativeTextfieldViews::HandleWillGainFocus() {
}

void NativeTextfieldViews::HandleDidGainFocus() {
  is_cursor_visible_ = true;
  SchedulePaint();
  // Start blinking cursor.
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      cursor_timer_.NewRunnableMethod(&NativeTextfieldViews::UpdateCursor),
      kCursorVisibleTimeMs);
}

void NativeTextfieldViews::HandleWillLoseFocus() {
  // Stop blinking cursor.
  cursor_timer_.RevokeAll();
  if (is_cursor_visible_) {
    is_cursor_visible_ = false;
    RepaintCursor();
  }
}

// static
bool NativeTextfieldViews::IsTextfieldViewsEnabled() {
#if defined(TOUCH_UI)
  return true;
#else
  return textfield_view_enabled ||
      CommandLine::ForCurrentProcess()->HasSwitch(
          kEnableViewsBasedTextfieldSwitch);
#endif
}

// static
void NativeTextfieldViews::SetEnableTextfieldViews(bool enabled) {
  textfield_view_enabled = enabled;
}


///////////////////////////////////////////////////////////////////////////////
// NativeTextfieldViews private:

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
  SchedulePaint(r, false);
}

void NativeTextfieldViews::UpdateCursorBoundsAndTextOffset() {
  if (bounds().IsEmpty())
    return;

  gfx::Insets insets = GetInsets();

  int width = bounds().width() - insets.width();

  // TODO(oshima): bidi
  const gfx::Font& font = GetFont();
  int full_width = font.GetStringWidth(model_->GetVisibleText());
  cursor_bounds_ = model_->GetCursorBounds(font);
  cursor_bounds_.set_y(cursor_bounds_.y() + insets.top());

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
  } else if(full_width > width && text_offset_ + full_width < width) {
    // when the cursor moves within the textfield with the text
    // longer than the field.
    text_offset_ = width - full_width;
  } else {
    // move cursor freely.
  }
  // shift cursor bounds to fit insets.
  cursor_bounds_.set_x(cursor_bounds_.x() + text_offset_ + insets.left());
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
    string16 text = model_->GetVisibleText((*iter).begin, (*iter).end);
    // TODO(oshima): This does not give the accurate position due to
    // kerning. Figure out how webkit does this with skia.
    int width = GetFont().GetStringWidth(text);

    if ((*iter).selected) {
      canvas->FillRectInt(selection_color, x_offset, y, width, text_height);
      canvas->DrawStringInt(
          UTF16ToWide(text), GetFont(), kSelectedTextColor,
          x_offset, y, width, text_height);
    } else {
      canvas->DrawStringInt(
          UTF16ToWide(text), GetFont(), text_color,
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
  // TODO(oshima): handle IME.
  if (key_event.GetType() == views::Event::ET_KEY_PRESSED) {
    app::KeyboardCode key_code = key_event.GetKeyCode();
    // TODO(oshima): shift-tab does not work. Figure out why and fix.
    if (key_code == app::VKEY_TAB)
      return false;
    bool selection = key_event.IsShiftDown();
    bool control = key_event.IsControlDown();
    bool text_changed = false;
    bool cursor_changed = false;
    switch (key_code) {
      case app::VKEY_A:
        if (control) {
          model_->SelectAll();
          cursor_changed = true;
        }
        break;
      case app::VKEY_RIGHT:
        control ? model_->MoveCursorToNextWord(selection)
            : model_->MoveCursorRight(selection);
        cursor_changed = true;
        break;
      case app::VKEY_LEFT:
        control ? model_->MoveCursorToPreviousWord(selection)
            : model_->MoveCursorLeft(selection);
        cursor_changed = true;
        break;
      case app::VKEY_END:
        model_->MoveCursorToEnd(selection);
        cursor_changed = true;
        break;
      case app::VKEY_HOME:
        model_->MoveCursorToStart(selection);
        cursor_changed = true;
        break;
      case app::VKEY_BACK:
        if (!model_->HasSelection()) {
          if (selection && control) {
            // If both shift and control are pressed, then erase upto the
            // beginning of the buffer in ChromeOS. In windows, do nothing.
#if defined(OS_WIN)
            break;
#else
            model_->MoveCursorToStart(true);
#endif
          } else if (control) {
            // If only control is pressed, then erase the previous word.
            model_->MoveCursorToPreviousWord(true);
          }
        }
        text_changed = model_->Backspace();
        cursor_changed = true;
        break;
      case app::VKEY_DELETE:
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
        text_changed = model_->Delete();
        break;
      case app::VKEY_INSERT:
        insert_ = !insert_;
        cursor_changed = true;
        break;
      default:
        break;
    }
    char16 print_char = GetPrintableChar(key_event);
    if (!control && print_char) {
      if (insert_)
        model_->Insert(print_char);
      else
        model_->Replace(print_char);
      text_changed = true;
    }
    if (text_changed) {
      textfield_->SyncText();
      Textfield::Controller* controller = textfield_->GetController();
      if (controller)
        controller->ContentsChanged(textfield_, GetText());
    }
    if (cursor_changed) {
      is_cursor_visible_ = true;
      RepaintCursor();
    }
    if (text_changed || cursor_changed) {
      UpdateCursorBoundsAndTextOffset();
      SchedulePaint();
      return true;
    }
  }
  return false;
}

char16 NativeTextfieldViews::GetPrintableChar(const KeyEvent& key_event) {
  // TODO(oshima): IME, i18n support.
  // This only works for UCS-2 characters.
  app::KeyboardCode key_code = key_event.GetKeyCode();
  bool shift = key_event.IsShiftDown();
  bool upper = shift ^ key_event.IsCapsLockDown();
  // TODO(oshima): We should have a utility function
  // under app to convert a KeyboardCode to a printable character,
  // probably in keyboard_code_conversion{.h, _x
  switch (key_code) {
    case app::VKEY_NUMPAD0:
      return '0';
    case app::VKEY_NUMPAD1:
      return '1';
    case app::VKEY_NUMPAD2:
      return '2';
    case app::VKEY_NUMPAD3:
      return '3';
    case app::VKEY_NUMPAD4:
      return '4';
    case app::VKEY_NUMPAD5:
      return '5';
    case app::VKEY_NUMPAD6:
      return '6';
    case app::VKEY_NUMPAD7:
      return '7';
    case app::VKEY_NUMPAD8:
      return '8';
    case app::VKEY_NUMPAD9:
      return '9';
    case app::VKEY_MULTIPLY:
      return '*';
    case app::VKEY_ADD:
      return '+';
    case app::VKEY_SUBTRACT:
      return '-';
    case app::VKEY_DECIMAL:
      return '.';
    case app::VKEY_DIVIDE:
      return '/';
    case app::VKEY_SPACE:
      return ' ';
    case app::VKEY_0:
      return shift ? ')' : '0';
    case app::VKEY_1:
      return shift ? '!' : '1';
    case app::VKEY_2:
      return shift ? '@' : '2';
    case app::VKEY_3:
      return shift ? '#' : '3';
    case app::VKEY_4:
      return shift ? '$' : '4';
    case app::VKEY_5:
      return shift ? '%' : '5';
    case app::VKEY_6:
      return shift ? '^' : '6';
    case app::VKEY_7:
      return shift ? '&' : '7';
    case app::VKEY_8:
      return shift ? '*' : '8';
    case app::VKEY_9:
      return shift ? '(' : '9';

    case app::VKEY_A:
    case app::VKEY_B:
    case app::VKEY_C:
    case app::VKEY_D:
    case app::VKEY_E:
    case app::VKEY_F:
    case app::VKEY_G:
    case app::VKEY_H:
    case app::VKEY_I:
    case app::VKEY_J:
    case app::VKEY_K:
    case app::VKEY_L:
    case app::VKEY_M:
    case app::VKEY_N:
    case app::VKEY_O:
    case app::VKEY_P:
    case app::VKEY_Q:
    case app::VKEY_R:
    case app::VKEY_S:
    case app::VKEY_T:
    case app::VKEY_U:
    case app::VKEY_V:
    case app::VKEY_W:
    case app::VKEY_X:
    case app::VKEY_Y:
    case app::VKEY_Z:
      return (upper ? 'A' : 'a') + (key_code - app::VKEY_A);
    case app::VKEY_OEM_1:
      return shift ? ':' : ';';
    case app::VKEY_OEM_PLUS:
      return shift ? '+' : '=';
    case app::VKEY_OEM_COMMA:
      return shift ? '<' : ',';
    case app::VKEY_OEM_MINUS:
      return shift ? '_' : '-';
    case app::VKEY_OEM_PERIOD:
      return shift ? '>' : '.';
    case app::VKEY_OEM_2:
      return shift ? '?' : '/';
    case app::VKEY_OEM_3:
      return shift ? '~' : '`';
    case app::VKEY_OEM_4:
      return shift ? '}' : ']';
    case app::VKEY_OEM_5:
      return shift ? '|' : '\\';
    case app::VKEY_OEM_6:
      return shift ? '{' : '[';
    case app::VKEY_OEM_7:
      return shift ? '"' : '\'';
    default:
      return 0;
  }
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

///////////////////////////////////////////////////////////////////////////////
// NativeTextfieldWrapper:

// static
NativeTextfieldWrapper* NativeTextfieldWrapper::CreateWrapper(
    Textfield* field) {
  if (NativeTextfieldViews::IsTextfieldViewsEnabled()) {
    return new NativeTextfieldViews(field);
  } else {
    return new NativeTextfieldGtk(field);
  }
}

///////////////////////////////////////////////////////////////////////////////
//
// TextifieldBorder
//
///////////////////////////////////////////////////////////////////////////////

NativeTextfieldViews::TextfieldBorder::TextfieldBorder()
    : has_focus_(false),
      insets_(4, 4, 4, 4) {
}

void NativeTextfieldViews::TextfieldBorder::Paint(
    const View& view, gfx::Canvas* canvas) const {
  SkRect rect;
  rect.set(SkIntToScalar(0), SkIntToScalar(0),
           SkIntToScalar(view.width()), SkIntToScalar(view.height()));
  SkScalar corners[8] = {
    // top-left
    insets_.left(),
    insets_.top(),
    // top-right
    insets_.right(),
    insets_.top(),
    // bottom-right
    insets_.right(),
    insets_.bottom(),
    // bottom-left
    insets_.left(),
    insets_.bottom(),
  };
  SkPath path;
  path.addRoundRect(rect, corners);
  SkPaint paint;
  paint.setStyle(SkPaint::kStroke_Style);
  paint.setFlags(SkPaint::kAntiAlias_Flag);
  // TODO(oshima): Copy what WebKit does for focused border.
  paint.setColor(has_focus_ ? kFocusedBorderColor : kDefaultBorderColor);
  paint.setStrokeWidth(has_focus_ ? 2 : 1);

  canvas->AsCanvasSkia()->drawPath(path, paint);
}

void NativeTextfieldViews::TextfieldBorder::GetInsets(gfx::Insets* insets) const
{
  *insets = insets_;
}

void NativeTextfieldViews::TextfieldBorder::SetInsets(int top,
                                                      int left,
                                                      int bottom,
                                                      int right) {
  insets_.Set(top, left, bottom, right);
}

}  // namespace views
