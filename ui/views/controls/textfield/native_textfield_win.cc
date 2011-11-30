// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/views/controls/textfield/native_textfield_win.h"

#include <algorithm>

#include "base/i18n/case_conversion.h"
#include "base/i18n/rtl.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "base/win/windows_version.h"
#include "grit/ui_strings.h"
#include "skia/ext/skia_utils_win.h"
#include "ui/base/accessibility/accessible_view_state.h"
#include "ui/base/clipboard/clipboard.h"
#include "ui/base/clipboard/scoped_clipboard_writer.h"
#include "ui/base/keycodes/keyboard_code_conversion_win.h"
#include "ui/base/keycodes/keyboard_codes.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/l10n_util_win.h"
#include "ui/base/range/range.h"
#include "ui/base/win/mouse_wheel_util.h"
#include "ui/gfx/native_theme_win.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/menu/menu_2.h"
#include "ui/views/controls/menu/menu_win.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/controls/textfield/native_textfield_views.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/textfield/textfield_controller.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/metrics.h"
#include "ui/views/widget/widget.h"
#include "views/views_delegate.h"

namespace views {

///////////////////////////////////////////////////////////////////////////////
// Helper classes

NativeTextfieldWin::ScopedFreeze::ScopedFreeze(NativeTextfieldWin* edit,
                                               ITextDocument* text_object_model)
    : edit_(edit),
      text_object_model_(text_object_model) {
  // Freeze the screen.
  if (text_object_model_) {
    long count;
    text_object_model_->Freeze(&count);
  }
}

NativeTextfieldWin::ScopedFreeze::~ScopedFreeze() {
  // Unfreeze the screen.
  if (text_object_model_) {
    long count;
    text_object_model_->Unfreeze(&count);
    if (count == 0) {
      // We need to UpdateWindow() here instead of InvalidateRect() because, as
      // far as I can tell, the edit likes to synchronously erase its background
      // when unfreezing, thus requiring us to synchronously redraw if we don't
      // want flicker.
      edit_->UpdateWindow();
    }
  }
}

NativeTextfieldWin::ScopedSuspendUndo::ScopedSuspendUndo(
    ITextDocument* text_object_model)
    : text_object_model_(text_object_model) {
  // Suspend Undo processing.
  if (text_object_model_)
    text_object_model_->Undo(tomSuspend, NULL);
}

NativeTextfieldWin::ScopedSuspendUndo::~ScopedSuspendUndo() {
  // Resume Undo processing.
  if (text_object_model_)
    text_object_model_->Undo(tomResume, NULL);
}

///////////////////////////////////////////////////////////////////////////////
// NativeTextfieldWin

bool NativeTextfieldWin::did_load_library_ = false;

NativeTextfieldWin::NativeTextfieldWin(Textfield* textfield)
    : textfield_(textfield),
      tracking_double_click_(false),
      double_click_time_(0),
      can_discard_mousemove_(false),
      contains_mouse_(false),
      ime_discard_composition_(false),
      ime_composition_start_(0),
      ime_composition_length_(0),
      container_view_(new NativeViewHost),
      bg_color_(0) {
  if (!did_load_library_)
    did_load_library_ = !!LoadLibrary(L"riched20.dll");

  DWORD style = kDefaultEditStyle | ES_AUTOHSCROLL;
  if (textfield_->style() & Textfield::STYLE_PASSWORD)
    style |= ES_PASSWORD;

  if (textfield_->read_only())
    style |= ES_READONLY;

  // Make sure we apply RTL related extended window styles if necessary.
  DWORD ex_style = l10n_util::GetExtendedStyles();

  RECT r = {0, 0, textfield_->width(), textfield_->height()};
  Create(textfield_->GetWidget()->GetNativeView(), r, NULL, style, ex_style);

  if (textfield_->style() & Textfield::STYLE_LOWERCASE) {
    DCHECK((textfield_->style() & Textfield::STYLE_PASSWORD) == 0);
    SetEditStyle(SES_LOWERCASE, SES_LOWERCASE);
  }

  // Set up the text_object_model_.
  base::win::ScopedComPtr<IRichEditOle, &IID_IRichEditOle> ole_interface;
  ole_interface.Attach(GetOleInterface());
  if (ole_interface)
    text_object_model_.QueryFrom(ole_interface);

  InitializeAccessibilityInfo();
}

NativeTextfieldWin::~NativeTextfieldWin() {
  if (IsWindow())
    DestroyWindow();
}

// static
bool NativeTextfieldWin::IsDoubleClick(const POINT& origin,
                                       const POINT& current,
                                       DWORD elapsed_time) {
  // The CXDOUBLECLK and CYDOUBLECLK system metrics describe the width and
  // height of a rectangle around the origin position, inside of which clicks
  // within the double click time are considered double clicks.
  return (elapsed_time <= GetDoubleClickTime()) &&
      (abs(current.x - origin.x) <= (GetSystemMetrics(SM_CXDOUBLECLK) / 2)) &&
      (abs(current.y - origin.y) <= (GetSystemMetrics(SM_CYDOUBLECLK) / 2));
}

// static
bool NativeTextfieldWin::IsNumPadDigit(int key_code, bool extended_key) {
  if (key_code >= VK_NUMPAD0 && key_code <= VK_NUMPAD9)
    return true;

  // Check for num pad keys without NumLock.
  // Note: there is no easy way to know if a the key that was pressed comes from
  //       the num pad or the rest of the keyboard.  Investigating how
  //       TranslateMessage() generates the WM_KEYCHAR from an
  //       ALT + <NumPad sequences> it appears it looks at the extended key flag
  //       (which is on if the key pressed comes from one of the 3 clusters to
  //       the left of the numeric keypad).  So we use it as well.
  return !extended_key &&
            ((key_code >= VK_PRIOR && key_code <= VK_DOWN) ||  // All keys but 5
                                                               // and 0.
            (key_code == VK_CLEAR) ||  // Key 5.
            (key_code == VK_INSERT));  // Key 0.
}

void NativeTextfieldWin::AttachHack() {
  // See the code in textfield.cc that calls this for why this is here.
  container_view_->set_focus_view(textfield_);
  container_view_->Attach(m_hWnd);
}

////////////////////////////////////////////////////////////////////////////////
// NativeTextfieldWin, NativeTextfieldWrapper implementation:

string16 NativeTextfieldWin::GetText() const {
  int len = GetTextLength() + 1;
  string16 str;
  if (len > 1)
    GetWindowText(WriteInto(&str, len), len);
  // The text get from GetWindowText() might be wrapped with explicit bidi
  // control characters. Refer to UpdateText() for detail. Without such
  // wrapping, in RTL chrome, a pure LTR string ending with parenthesis will
  // not be displayed correctly in a textfield. For example, "Yahoo!" will be
  // displayed as "!Yahoo", and "Google (by default)" will be displayed as
  // "(Google (by default".
  return base::i18n::StripWrappingBidiControlCharacters(str);
}

void NativeTextfieldWin::UpdateText() {
  string16 text = textfield_->text();
  // Adjusting the string direction before setting the text in order to make
  // sure both RTL and LTR strings are displayed properly.
  base::i18n::AdjustStringForLocaleDirection(&text);
  if (textfield_->style() & Textfield::STYLE_LOWERCASE)
    text = base::i18n::ToLower(text);
  SetWindowText(text.c_str());
  UpdateAccessibleValue(text);
}

void NativeTextfieldWin::AppendText(const string16& text) {
  int text_length = GetWindowTextLength();
  ::SendMessage(m_hWnd, TBM_SETSEL, true, MAKELPARAM(text_length, text_length));
  ::SendMessage(m_hWnd, EM_REPLACESEL, false,
                reinterpret_cast<LPARAM>(text.c_str()));
}

string16 NativeTextfieldWin::GetSelectedText() const {
  CHARRANGE sel;
  GetSel(sel);
  string16 str;
  if (sel.cpMin != sel.cpMax)
    GetSelText(WriteInto(&str, sel.cpMax - sel.cpMin + 1));
  return str;
}

void NativeTextfieldWin::SelectAll() {
  // Select from the end to the front so that the first part of the text is
  // always visible.
  SetSel(GetTextLength(), 0);
}

void NativeTextfieldWin::ClearSelection() {
  SetSel(GetTextLength(), GetTextLength());
}

void NativeTextfieldWin::UpdateBorder() {
  SetWindowPos(NULL, 0, 0, 0, 0,
               SWP_NOMOVE | SWP_FRAMECHANGED | SWP_NOACTIVATE |
               SWP_NOOWNERZORDER | SWP_NOSIZE);
}

void NativeTextfieldWin::UpdateTextColor() {
  CHARFORMAT cf = {0};
  cf.dwMask = CFM_COLOR;
  cf.crTextColor = textfield_->use_default_text_color() ?
      GetSysColor(textfield_->read_only() ? COLOR_GRAYTEXT : COLOR_WINDOWTEXT) :
      skia::SkColorToCOLORREF(textfield_->text_color());
  CRichEditCtrl::SetDefaultCharFormat(cf);
}

void NativeTextfieldWin::UpdateBackgroundColor() {
  if (!textfield_->use_default_background_color()) {
    bg_color_ = skia::SkColorToCOLORREF(textfield_->background_color());
  } else {
    bg_color_ = GetSysColor(textfield_->read_only() ? COLOR_3DFACE
                                                    : COLOR_WINDOW);
  }
  CRichEditCtrl::SetBackgroundColor(bg_color_);
}

void NativeTextfieldWin::UpdateReadOnly() {
  SendMessage(m_hWnd, EM_SETREADONLY, textfield_->read_only(), 0);
  UpdateAccessibleState(STATE_SYSTEM_READONLY, textfield_->read_only());
}

void NativeTextfieldWin::UpdateFont() {
  SendMessage(m_hWnd, WM_SETFONT,
              reinterpret_cast<WPARAM>(textfield_->font().GetNativeFont()),
              TRUE);
  // Setting the font blows away any text color we've set, so reset it.
  UpdateTextColor();
}

void NativeTextfieldWin::UpdateIsPassword() {
  // TODO: Need to implement for Windows.
  UpdateAccessibleState(STATE_SYSTEM_PROTECTED, textfield_->IsPassword());
}

void NativeTextfieldWin::UpdateEnabled() {
  SendMessage(m_hWnd, WM_ENABLE, textfield_->IsEnabled(), 0);
  UpdateAccessibleState(STATE_SYSTEM_UNAVAILABLE, !textfield_->IsEnabled());
}

gfx::Insets NativeTextfieldWin::CalculateInsets() {
  // NOTE: One would think GetThemeMargins would return the insets we should
  // use, but it doesn't. The margins returned by GetThemeMargins are always
  // 0.

  // This appears to be the insets used by Windows.
  return gfx::Insets(3, 3, 3, 3);
}

void NativeTextfieldWin::UpdateHorizontalMargins() {
  int left, right;
  if (!textfield_->GetHorizontalMargins(&left, &right))
    return;

  // SendMessage expects the two values to be packed into one using MAKELONG
  // so we truncate to 16 bits if necessary.
  SendMessage(m_hWnd, EM_SETMARGINS,
              EC_LEFTMARGIN | EC_RIGHTMARGIN,
              MAKELONG(left  & 0xFFFF, right & 0xFFFF));
}

void NativeTextfieldWin::UpdateVerticalMargins() {
  int top, bottom;
  if (!textfield_->GetVerticalMargins(&top, &bottom))
    return;

  if (top == 0 && bottom == 0) {
    // Do nothing, default margins are 0 already.
    return;
  }
  // Non-zero margins case.
  NOTIMPLEMENTED();
}

bool NativeTextfieldWin::SetFocus() {
  // Focus the associated HWND.
  //container_view_->Focus();
  ::SetFocus(m_hWnd);
  return true;
}

View* NativeTextfieldWin::GetView() {
  return container_view_;
}

gfx::NativeView NativeTextfieldWin::GetTestingHandle() const {
  return m_hWnd;
}

bool NativeTextfieldWin::IsIMEComposing() const {
  // Retrieve the length of the composition string to check if an IME is
  // composing text. (If this length is > 0 then an IME is being used to compose
  // text.)
  HIMC imm_context = ImmGetContext(m_hWnd);
  if (!imm_context)
    return false;

  const int composition_size = ImmGetCompositionString(imm_context, GCS_COMPSTR,
                                                       NULL, 0);
  ImmReleaseContext(m_hWnd, imm_context);
  return composition_size > 0;
}

void NativeTextfieldWin::GetSelectedRange(ui::Range* range) const {
  NOTREACHED();
}

void NativeTextfieldWin::SelectRange(const ui::Range& range) {
  NOTREACHED();
}

void NativeTextfieldWin::GetSelectionModel(gfx::SelectionModel* sel) const {
  NOTREACHED();
}

void NativeTextfieldWin::SelectSelectionModel(const gfx::SelectionModel& sel) {
  NOTREACHED();
}

size_t NativeTextfieldWin::GetCursorPosition() const {
  NOTREACHED();
  return 0U;
}

bool NativeTextfieldWin::HandleKeyPressed(const views::KeyEvent& event) {
  return false;
}

bool NativeTextfieldWin::HandleKeyReleased(const views::KeyEvent& event) {
  return false;
}

void NativeTextfieldWin::HandleFocus() {
}

void NativeTextfieldWin::HandleBlur() {
}

ui::TextInputClient* NativeTextfieldWin::GetTextInputClient() {
  return NULL;
}

void NativeTextfieldWin::ApplyStyleRange(const gfx::StyleRange& style) {
  NOTREACHED();
}

void NativeTextfieldWin::ApplyDefaultStyle() {
  NOTREACHED();
}

void NativeTextfieldWin::ClearEditHistory() {
  NOTREACHED();
}

////////////////////////////////////////////////////////////////////////////////
// NativeTextfieldWin, ui::SimpleMenuModel::Delegate implementation:

bool NativeTextfieldWin::IsCommandIdChecked(int command_id) const {
  return false;
}

bool NativeTextfieldWin::IsCommandIdEnabled(int command_id) const {
  switch (command_id) {
    case IDS_APP_UNDO:       return !textfield_->read_only() && !!CanUndo();
    case IDS_APP_CUT:        return !textfield_->read_only() &&
                                    !textfield_->IsPassword() && !!CanCut();
    case IDS_APP_COPY:       return !!CanCopy() && !textfield_->IsPassword();
    case IDS_APP_PASTE:      return !textfield_->read_only() && !!CanPaste();
    case IDS_APP_SELECT_ALL: return !!CanSelectAll();
    default:                 NOTREACHED();
                             return false;
  }
}

bool NativeTextfieldWin::GetAcceleratorForCommandId(int command_id,
    ui::Accelerator* accelerator) {
  // The standard Ctrl-X, Ctrl-V and Ctrl-C are not defined as accelerators
  // anywhere so we need to check for them explicitly here.
  switch (command_id) {
    case IDS_APP_CUT:
      *accelerator = ui::Accelerator(ui::VKEY_X, false, true, false);
      return true;
    case IDS_APP_COPY:
      *accelerator = ui::Accelerator(ui::VKEY_C, false, true, false);
      return true;
    case IDS_APP_PASTE:
      *accelerator = ui::Accelerator(ui::VKEY_V, false, true, false);
      return true;
  }
  return container_view_->GetWidget()->GetAccelerator(command_id, accelerator);
}

void NativeTextfieldWin::ExecuteCommand(int command_id) {
  ScopedFreeze freeze(this, GetTextObjectModel());
  OnBeforePossibleChange();
  switch (command_id) {
    case IDS_APP_UNDO:       Undo();       break;
    case IDS_APP_CUT:        Cut();        break;
    case IDS_APP_COPY:       Copy();       break;
    case IDS_APP_PASTE:      Paste();      break;
    case IDS_APP_SELECT_ALL: SelectAll();  break;
    default:                 NOTREACHED(); break;
  }
  OnAfterPossibleChange(true);
}

void NativeTextfieldWin::InitializeAccessibilityInfo() {
  // Set the accessible state.
  accessibility_state_ = 0;

  base::win::ScopedComPtr<IAccPropServices> pAccPropServices;
  HRESULT hr = CoCreateInstance(CLSID_AccPropServices, NULL, CLSCTX_SERVER,
      IID_IAccPropServices, reinterpret_cast<void**>(&pAccPropServices));
  if (!SUCCEEDED(hr))
    return;

  VARIANT var;

  // Set the accessible role.
  var.vt = VT_I4;
  var.lVal = ROLE_SYSTEM_TEXT;
  hr = pAccPropServices->SetHwndProp(m_hWnd, OBJID_CLIENT,
      CHILDID_SELF, PROPID_ACC_ROLE, var);

  // Set the accessible name by getting the label text.
  View* parent = textfield_->parent();
  int label_index = parent->GetIndexOf(textfield_) - 1;
  if (label_index >= 0) {
    // Try to find the name of this text field.
    // We expect it to be a Label preceeding this view (if it exists).
    string16 name;
    View* label_view = parent->child_at(label_index);
    if (label_view->GetClassName() == Label::kViewClassName) {
      ui::AccessibleViewState state;
      label_view->GetAccessibleState(&state);
      hr = pAccPropServices->SetHwndPropStr(m_hWnd, OBJID_CLIENT,
          CHILDID_SELF, PROPID_ACC_NAME, state.name.c_str());
    }
  }
}

void NativeTextfieldWin::UpdateAccessibleState(uint32 state_flag,
                                               bool set_value) {
  base::win::ScopedComPtr<IAccPropServices> pAccPropServices;
  HRESULT hr = CoCreateInstance(CLSID_AccPropServices, NULL, CLSCTX_SERVER,
      IID_IAccPropServices, reinterpret_cast<void**>(&pAccPropServices));
  if (!SUCCEEDED(hr))
    return;

  VARIANT var;
  var.vt = VT_I4;
  var.lVal = set_value ? accessibility_state_ | state_flag
      : accessibility_state_ & ~state_flag;
  hr = pAccPropServices->SetHwndProp(m_hWnd, OBJID_CLIENT,
      CHILDID_SELF, PROPID_ACC_STATE, var);

  ::NotifyWinEvent(EVENT_OBJECT_STATECHANGE, m_hWnd, OBJID_CLIENT,
                   CHILDID_SELF);
}

void NativeTextfieldWin::UpdateAccessibleValue(const string16& value) {
  base::win::ScopedComPtr<IAccPropServices> pAccPropServices;
  HRESULT hr = CoCreateInstance(CLSID_AccPropServices, NULL, CLSCTX_SERVER,
      IID_IAccPropServices, reinterpret_cast<void**>(&pAccPropServices));
  if (!SUCCEEDED(hr))
    return;

  hr = pAccPropServices->SetHwndPropStr(m_hWnd, OBJID_CLIENT,
      CHILDID_SELF, PROPID_ACC_VALUE, value.c_str());

  ::NotifyWinEvent(EVENT_OBJECT_VALUECHANGE, m_hWnd, OBJID_CLIENT,
                   CHILDID_SELF);
}

////////////////////////////////////////////////////////////////////////////////
// NativeTextfieldWin, private:

void NativeTextfieldWin::OnChar(TCHAR ch, UINT repeat_count, UINT flags) {
  HandleKeystroke();
}

void NativeTextfieldWin::OnContextMenu(HWND window, const POINT& point) {
  POINT p(point);
  if (point.x == -1 || point.y == -1) {
    GetCaretPos(&p);
    MapWindowPoints(HWND_DESKTOP, &p, 1);
  }
  BuildContextMenu();
  context_menu_->RunContextMenuAt(gfx::Point(p));
}

void NativeTextfieldWin::OnCopy() {
  if (textfield_->IsPassword())
    return;

  const string16 text(GetSelectedText());
  if (!text.empty() && ViewsDelegate::views_delegate) {
    ui::ScopedClipboardWriter scw(
        ViewsDelegate::views_delegate->GetClipboard());
    scw.WriteText(text);
  }
}

void NativeTextfieldWin::OnCut() {
  if (textfield_->read_only() || textfield_->IsPassword())
    return;

  OnCopy();

  // This replace selection will have no effect (even on the undo stack) if the
  // current selection is empty.
  ReplaceSel(L"", true);
}

LRESULT NativeTextfieldWin::OnImeChar(UINT message,
                                      WPARAM wparam,
                                      LPARAM lparam) {
  // http://crbug.com/7707: a rich-edit control may crash when it receives a
  // WM_IME_CHAR message while it is processing a WM_IME_COMPOSITION message.
  // Since view controls don't need WM_IME_CHAR messages, we prevent WM_IME_CHAR
  // messages from being dispatched to view controls via the CallWindowProc()
  // call.
  return 0;
}

LRESULT NativeTextfieldWin::OnImeStartComposition(UINT message,
                                                  WPARAM wparam,
                                                  LPARAM lparam) {
  // Users may press alt+shift or control+shift keys to change their keyboard
  // layouts. So, we retrieve the input locale identifier everytime we start
  // an IME composition.
  int language_id = PRIMARYLANGID(GetKeyboardLayout(0));
  ime_discard_composition_ =
      language_id == LANG_JAPANESE || language_id == LANG_CHINESE;
  ime_composition_start_ = 0;
  ime_composition_length_ = 0;

  return DefWindowProc(message, wparam, lparam);
}

LRESULT NativeTextfieldWin::OnImeComposition(UINT message,
                                             WPARAM wparam,
                                             LPARAM lparam) {
  text_before_change_.clear();
  LRESULT result = DefWindowProc(message, wparam, lparam);

  ime_composition_start_ = 0;
  ime_composition_length_ = 0;
  if (ime_discard_composition_) {
    // Call IMM32 functions to retrieve the position and the length of the
    // ongoing composition string and notify the OnAfterPossibleChange()
    // function that it should discard the composition string from a search
    // string. We should not call IMM32 functions in the function because it
    // is called when an IME is not composing a string.
    HIMC imm_context = ImmGetContext(m_hWnd);
    if (imm_context) {
      CHARRANGE selection;
      GetSel(selection);
      const int cursor_position =
          ImmGetCompositionString(imm_context, GCS_CURSORPOS, NULL, 0);
      if (cursor_position >= 0)
        ime_composition_start_ = selection.cpMin - cursor_position;

      const int composition_size =
          ImmGetCompositionString(imm_context, GCS_COMPSTR, NULL, 0);
      if (composition_size >= 0)
        ime_composition_length_ = composition_size / sizeof(wchar_t);

      ImmReleaseContext(m_hWnd, imm_context);
    }
  }

  // If we allow OnAfterPossibleChange() to redraw the text, it will do this by
  // setting the edit's text directly, which can cancel the current IME
  // composition or cause other adverse affects. So we set |should_redraw_text|
  // to false.
  OnAfterPossibleChange(false);
  return result;
}

LRESULT NativeTextfieldWin::OnImeEndComposition(UINT message,
                                                WPARAM wparam,
                                                LPARAM lparam) {
  // Bug 11863: Korean IMEs send a WM_IME_ENDCOMPOSITION message without
  // sending any WM_IME_COMPOSITION messages when a user deletes all
  // composition characters, i.e. a composition string becomes empty. To handle
  // this case, we need to update the find results when a composition is
  // finished or canceled.
  textfield_->SyncText();
  return DefWindowProc(message, wparam, lparam);
}

void NativeTextfieldWin::OnKeyDown(TCHAR key, UINT repeat_count, UINT flags) {
  // NOTE: Annoyingly, ctrl-alt-<key> generates WM_KEYDOWN rather than
  // WM_SYSKEYDOWN, so we need to check (flags & KF_ALTDOWN) in various places
  // in this function even with a WM_SYSKEYDOWN handler.

  switch (key) {

    // Ignore Return
    case VK_RETURN:
      return;

    // Hijacking Editing Commands
    //
    // We hijack the keyboard short-cuts for Cut, Copy, and Paste here so that
    // they go through our clipboard routines.  This allows us to be smarter
    // about how we interact with the clipboard and avoid bugs in the
    // CRichEditCtrl.  If we didn't hijack here, the edit control would handle
    // these internally with sending the WM_CUT, WM_COPY, or WM_PASTE messages.
    //
    // Cut:   Shift-Delete and Ctrl-x are treated as cut.  Ctrl-Shift-Delete and
    //        Ctrl-Shift-x are not treated as cut even though the underlying
    //        CRichTextEdit would treat them as such.
    // Copy:  Ctrl-c is treated as copy.  Shift-Ctrl-c is not.
    // Paste: Shift-Insert and Ctrl-v are tread as paste.  Ctrl-Shift-Insert and
    //        Ctrl-Shift-v are not.
    //
    // This behavior matches most, but not all Windows programs, and largely
    // conforms to what users expect.

    case VK_DELETE:
    case 'X':
      if ((flags & KF_ALTDOWN) ||
          (GetKeyState((key == 'X') ? VK_CONTROL : VK_SHIFT) >= 0))
        break;
      if (GetKeyState((key == 'X') ? VK_SHIFT : VK_CONTROL) >= 0) {
        ScopedFreeze freeze(this, GetTextObjectModel());
        OnBeforePossibleChange();
        Cut();
        OnAfterPossibleChange(true);
      }
      return;

    case 'C':
      if ((flags & KF_ALTDOWN) || (GetKeyState(VK_CONTROL) >= 0))
        break;
      if (GetKeyState(VK_SHIFT) >= 0)
        Copy();
      return;

    case VK_INSERT:
      // Ignore insert by itself, so we don't turn overtype mode on/off.
      if (!(flags & KF_ALTDOWN) && (GetKeyState(VK_SHIFT) >= 0) &&
          (GetKeyState(VK_CONTROL) >= 0))
        return;
      // Fall through to the next case (ie. Shift-Insert == Ctrl-V).
    case 'V':
      if ((flags & KF_ALTDOWN) ||
          (GetKeyState((key == 'V') ? VK_CONTROL : VK_SHIFT) >= 0))
        break;
      if (GetKeyState((key == 'V') ? VK_SHIFT : VK_CONTROL) >= 0) {
        ScopedFreeze freeze(this, GetTextObjectModel());
        OnBeforePossibleChange();
        Paste();
        OnAfterPossibleChange(true);
      }
      return;

    case 0xbb:  // Ctrl-'='.  Triggers subscripting, even in plain text mode.
                // We don't use VK_OEM_PLUS in case the macro isn't defined.
                // (e.g., we don't have this symbol in embeded environment).
      return;

    case VK_PROCESSKEY:
      // This key event is consumed by an IME.
      // We ignore this event because an IME sends WM_IME_COMPOSITION messages
      // when it updates the CRichEditCtrl text.
      return;
  }

  // CRichEditCtrl changes its text on WM_KEYDOWN instead of WM_CHAR for many
  // different keys (backspace, ctrl-v, ...), so we call this in both cases.
  HandleKeystroke();
}

void NativeTextfieldWin::OnLButtonDblClk(UINT keys, const CPoint& point) {
  // Save the double click info for later triple-click detection.
  tracking_double_click_ = true;
  double_click_point_ = point;
  double_click_time_ = GetCurrentMessage()->time;

  ScopedFreeze freeze(this, GetTextObjectModel());
  OnBeforePossibleChange();
  DefWindowProc(WM_LBUTTONDBLCLK, keys,
                MAKELPARAM(ClipXCoordToVisibleText(point.x, false), point.y));
  OnAfterPossibleChange(true);
}

void NativeTextfieldWin::OnLButtonDown(UINT keys, const CPoint& point) {
  // Check for triple click, then reset tracker.  Should be safe to subtract
  // double_click_time_ from the current message's time even if the timer has
  // wrapped in between.
  const bool is_triple_click = tracking_double_click_ &&
      IsDoubleClick(double_click_point_, point,
                    GetCurrentMessage()->time - double_click_time_);
  tracking_double_click_ = false;

  ScopedFreeze freeze(this, GetTextObjectModel());
  OnBeforePossibleChange();
  DefWindowProc(WM_LBUTTONDOWN, keys,
                MAKELPARAM(ClipXCoordToVisibleText(point.x, is_triple_click),
                           point.y));
  OnAfterPossibleChange(true);
}

void NativeTextfieldWin::OnLButtonUp(UINT keys, const CPoint& point) {
  ScopedFreeze freeze(this, GetTextObjectModel());
  OnBeforePossibleChange();
  DefWindowProc(WM_LBUTTONUP, keys,
                MAKELPARAM(ClipXCoordToVisibleText(point.x, false), point.y));
  OnAfterPossibleChange(true);
}

void NativeTextfieldWin::OnMouseLeave() {
  SetContainsMouse(false);
}

LRESULT NativeTextfieldWin::OnMouseWheel(UINT message,
                                         WPARAM w_param,
                                         LPARAM l_param) {
  // Reroute the mouse-wheel to the window under the mouse pointer if
  // applicable.
  if (ui::RerouteMouseWheel(m_hWnd, w_param, l_param))
    return 0;
  return DefWindowProc(message, w_param, l_param);
}

void NativeTextfieldWin::OnMouseMove(UINT keys, const CPoint& point) {
  SetContainsMouse(true);
  // Clamp the selection to the visible text so the user can't drag to select
  // the "phantom newline".  In theory we could achieve this by clipping the X
  // coordinate, but in practice the edit seems to behave nondeterministically
  // with similar sequences of clipped input coordinates fed to it.  Maybe it's
  // reading the mouse cursor position directly?
  //
  // This solution has a minor visual flaw, however: if there's a visible
  // cursor at the edge of the text (only true when there's no selection),
  // dragging the mouse around outside that edge repaints the cursor on every
  // WM_MOUSEMOVE instead of allowing it to blink normally.  To fix this, we
  // special-case this exact case and discard the WM_MOUSEMOVE messages instead
  // of passing them along.
  //
  // But even this solution has a flaw!  (Argh.)  In the case where the user
  // has a selection that starts at the edge of the edit, and proceeds to the
  // middle of the edit, and the user is dragging back past the start edge to
  // remove the selection, there's a redraw problem where the change between
  // having the last few bits of text still selected and having nothing
  // selected can be slow to repaint (which feels noticeably strange).  This
  // occurs if you only let the edit receive a single WM_MOUSEMOVE past the
  // edge of the text.  I think on each WM_MOUSEMOVE the edit is repainting its
  // previous state, then updating its internal variables to the new state but
  // not repainting.  To fix this, we allow one more WM_MOUSEMOVE through after
  // the selection has supposedly been shrunk to nothing; this makes the edit
  // redraw the selection quickly so it feels smooth.
  CHARRANGE selection;
  GetSel(selection);
  const bool possibly_can_discard_mousemove =
      (selection.cpMin == selection.cpMax) &&
      (((selection.cpMin == 0) &&
        (ClipXCoordToVisibleText(point.x, false) > point.x)) ||
       ((selection.cpMin == GetTextLength()) &&
        (ClipXCoordToVisibleText(point.x, false) < point.x)));
  if (!can_discard_mousemove_ || !possibly_can_discard_mousemove) {
    can_discard_mousemove_ = possibly_can_discard_mousemove;
    ScopedFreeze freeze(this, GetTextObjectModel());
    OnBeforePossibleChange();
    // Force the Y coordinate to the center of the clip rect.  The edit
    // behaves strangely when the cursor is dragged vertically: if the cursor
    // is in the middle of the text, drags inside the clip rect do nothing,
    // and drags outside the clip rect act as if the cursor jumped to the
    // left edge of the text.  When the cursor is at the right edge, drags of
    // just a few pixels vertically end up selecting the "phantom newline"...
    // sometimes.
    RECT r;
    GetRect(&r);
    DefWindowProc(WM_MOUSEMOVE, keys,
                  MAKELPARAM(point.x, (r.bottom - r.top) / 2));
    OnAfterPossibleChange(true);
  }
}

int NativeTextfieldWin::OnNCCalcSize(BOOL w_param, LPARAM l_param) {
  content_insets_.Set(0, 0, 0, 0);
  if (textfield_->draw_border())
    content_insets_ = CalculateInsets();
  if (w_param) {
    NCCALCSIZE_PARAMS* nc_params =
        reinterpret_cast<NCCALCSIZE_PARAMS*>(l_param);
    nc_params->rgrc[0].left += content_insets_.left();
    nc_params->rgrc[0].right -= content_insets_.right();
    nc_params->rgrc[0].top += content_insets_.top();
    nc_params->rgrc[0].bottom -= content_insets_.bottom();
  } else {
    RECT* rect = reinterpret_cast<RECT*>(l_param);
    rect->left += content_insets_.left();
    rect->right -= content_insets_.right();
    rect->top += content_insets_.top();
    rect->bottom -= content_insets_.bottom();
  }
  return 0;
}

void NativeTextfieldWin::OnNCPaint(HRGN region) {
  if (!textfield_->draw_border())
    return;

  HDC hdc = GetWindowDC();

  CRect window_rect;
  GetWindowRect(&window_rect);
  // Convert to be relative to 0x0.
  window_rect.MoveToXY(0, 0);

  ExcludeClipRect(hdc,
                  window_rect.left + content_insets_.left(),
                  window_rect.top + content_insets_.top(),
                  window_rect.right - content_insets_.right(),
                  window_rect.bottom - content_insets_.bottom());

  HBRUSH brush = CreateSolidBrush(bg_color_);
  FillRect(hdc, &window_rect, brush);
  DeleteObject(brush);

  int part;
  int state;

  if (base::win::GetVersion() < base::win::VERSION_VISTA) {
    part = EP_EDITTEXT;

    if (!textfield_->IsEnabled())
      state = ETS_DISABLED;
    else if (textfield_->read_only())
      state = ETS_READONLY;
    else if (!contains_mouse_)
      state = ETS_NORMAL;
    else
      state = ETS_HOT;
  } else {
    part = EP_EDITBORDER_HVSCROLL;

    if (!textfield_->IsEnabled())
      state = EPSHV_DISABLED;
    else if (GetFocus() == m_hWnd)
      state = EPSHV_FOCUSED;
    else if (contains_mouse_)
      state = EPSHV_HOT;
    else
      state = EPSHV_NORMAL;
    // Vista doesn't appear to have a unique state for readonly.
  }

  int classic_state =
      (!textfield_->IsEnabled() || textfield_->read_only()) ? DFCS_INACTIVE : 0;

  gfx::NativeThemeWin::instance()->PaintTextField(hdc, part, state,
                                                  classic_state, &window_rect,
                                                  bg_color_, false, true);

  // NOTE: I tried checking the transparent property of the theme and invoking
  // drawParentBackground, but it didn't seem to make a difference.

  ReleaseDC(hdc);
}

void NativeTextfieldWin::OnNonLButtonDown(UINT keys, const CPoint& point) {
  // Interestingly, the edit doesn't seem to cancel triple clicking when the
  // x-buttons (which usually means "thumb buttons") are pressed, so we only
  // call this for M and R down.
  tracking_double_click_ = false;
  SetMsgHandled(false);
}

void NativeTextfieldWin::OnPaste() {
  if (textfield_->read_only() || !ViewsDelegate::views_delegate)
    return;

  ui::Clipboard* clipboard = ViewsDelegate::views_delegate->GetClipboard();
  if (!clipboard->IsFormatAvailable(ui::Clipboard::GetPlainTextWFormatType(),
                                    ui::Clipboard::BUFFER_STANDARD))
    return;

  string16 clipboard_str;
  clipboard->ReadText(ui::Clipboard::BUFFER_STANDARD, &clipboard_str);
  if (!clipboard_str.empty()) {
    string16 collapsed(CollapseWhitespace(clipboard_str, false));
    if (textfield_->style() & Textfield::STYLE_LOWERCASE)
      collapsed = base::i18n::ToLower(collapsed);
    // Force a Paste operation to trigger ContentsChanged, even if identical
    // contents are pasted into the text box. See http://crbug.com/79002
    ReplaceSel(L"", false);
    textfield_->SyncText();
    text_before_change_.clear();
    ReplaceSel(collapsed.c_str(), true);
  }
}

void NativeTextfieldWin::OnSetFocus(HWND hwnd) {
  SetMsgHandled(FALSE);  // We still want the default processing of the message.

  views::FocusManager* focus_manager = textfield_->GetFocusManager();
  if (!focus_manager) {
    NOTREACHED();
    return;
  }
  focus_manager->SetFocusedView(textfield_);
}

void NativeTextfieldWin::OnSysChar(TCHAR ch, UINT repeat_count, UINT flags) {
  // Nearly all alt-<xxx> combos result in beeping rather than doing something
  // useful, so we discard most.  Exceptions:
  //   * ctrl-alt-<xxx>, which is sometimes important, generates WM_CHAR instead
  //     of WM_SYSCHAR, so it doesn't need to be handled here.
  //   * alt-space gets translated by the default WM_SYSCHAR handler to a
  //     WM_SYSCOMMAND to open the application context menu, so we need to allow
  //     it through.
  if (ch == VK_SPACE)
    SetMsgHandled(false);
}

void NativeTextfieldWin::HandleKeystroke() {
  const MSG* msg = GetCurrentMessage();
  ScopedFreeze freeze(this, GetTextObjectModel());

  TextfieldController* controller = textfield_->GetController();
  bool handled = false;
  if (controller) {
    KeyEvent event(*msg);
    handled = controller->HandleKeyEvent(textfield_, event);
  }

  if (!handled) {
    OnBeforePossibleChange();

    if (msg->wParam == ui::VKEY_HOME || msg->wParam == ui::VKEY_END) {
      // DefWindowProc() might reset the keyboard layout when it receives a
      // keydown event for VKEY_HOME or VKEY_END. When the window was created
      // with WS_EX_LAYOUTRTL and the current keyboard layout is not a RTL one,
      // if the input text is pure LTR text, the layout changes to the first RTL
      // keyboard layout in keyboard layout queue; if the input text is
      // bidirectional text, the layout changes to the keyboard layout of the
      // first RTL character in input text. When the window was created without
      // WS_EX_LAYOUTRTL and the current keyboard layout is not a LTR one, if
      // the input text is pure RTL text, the layout changes to English; if the
      // input text is bidirectional text, the layout changes to the keyboard
      // layout of the first LTR character in input text. Such keyboard layout
      // change behavior is surprising and inconsistent with keyboard behavior
      // elsewhere, so reset the layout in this case.
      HKL layout = GetKeyboardLayout(0);
      DefWindowProc(msg->message, msg->wParam, msg->lParam);
      ActivateKeyboardLayout(layout, KLF_REORDER);
    } else {
      DefWindowProc(msg->message, msg->wParam, msg->lParam);
    }

    // CRichEditCtrl automatically turns on IMF_AUTOKEYBOARD when the user
    // inputs an RTL character, making it difficult for the user to control
    // what language is set as they type. Force this off to make the edit's
    // behavior more stable.
    const int lang_options = SendMessage(EM_GETLANGOPTIONS, 0, 0);
    if (lang_options & IMF_AUTOKEYBOARD)
      SendMessage(EM_SETLANGOPTIONS, 0, lang_options & ~IMF_AUTOKEYBOARD);

    OnAfterPossibleChange(true);
  }
}

void NativeTextfieldWin::OnBeforePossibleChange() {
  // Record our state.
  text_before_change_ = GetText();
}

void NativeTextfieldWin::OnAfterPossibleChange(bool should_redraw_text) {
  // Prevent the user from selecting the "phantom newline" at the end of the
  // edit.  If they try, we just silently move the end of the selection back to
  // the end of the real text.
  CHARRANGE new_sel;
  GetSel(new_sel);
  const int length = GetTextLength();
  if (new_sel.cpMax > length) {
    new_sel.cpMax = length;
    if (new_sel.cpMin > length)
      new_sel.cpMin = length;
    SetSel(new_sel);
  }

  string16 new_text(GetText());
  if (new_text != text_before_change_) {
    if (ime_discard_composition_ && ime_composition_start_ >= 0 &&
        ime_composition_length_ > 0) {
      // A string retrieved with a GetText() call contains a string being
      // composed by an IME. We remove the composition string from this search
      // string.
      new_text.erase(ime_composition_start_, ime_composition_length_);
      ime_composition_start_ = 0;
      ime_composition_length_ = 0;
      if (new_text.empty())
        return;
    }
    textfield_->SyncText();
    UpdateAccessibleValue(textfield_->text());

    if (should_redraw_text) {
      CHARRANGE original_sel;
      GetSel(original_sel);
      string16 text(GetText());
      ScopedSuspendUndo suspend_undo(GetTextObjectModel());

      SelectAll();
      ReplaceSel(reinterpret_cast<LPCTSTR>(text.c_str()), true);
      SetSel(original_sel);
    }
  }
}

LONG NativeTextfieldWin::ClipXCoordToVisibleText(LONG x,
                                                 bool is_triple_click) const {
  // Clip the X coordinate to the left edge of the text.  Careful:
  // PosFromChar(0) may return a negative X coordinate if the beginning of the
  // text has scrolled off the edit, so don't go past the clip rect's edge.
  PARAFORMAT2 pf2;
  GetParaFormat(pf2);
  // Calculation of the clipped coordinate is more complicated if the paragraph
  // layout is RTL layout, or if there is RTL characters inside the LTR layout
  // paragraph.
  bool ltr_text_in_ltr_layout = true;
  if ((pf2.wEffects & PFE_RTLPARA) ||
      base::i18n::StringContainsStrongRTLChars(GetText())) {
    ltr_text_in_ltr_layout = false;
  }
  const int length = GetTextLength();
  RECT r;
  GetRect(&r);
  // The values returned by PosFromChar() seem to refer always
  // to the left edge of the character's bounding box.
  const LONG first_position_x = PosFromChar(0).x;
  LONG min_x = first_position_x;
  if (!ltr_text_in_ltr_layout) {
    for (int i = 1; i < length; ++i)
      min_x = std::min(min_x, PosFromChar(i).x);
  }
  const LONG left_bound = std::max(r.left, min_x);

  // PosFromChar(length) is a phantom character past the end of the text. It is
  // not necessarily a right bound; in RTL controls it may be a left bound. So
  // treat it as a right bound only if it is to the right of the first
  // character.
  LONG right_bound = r.right;
  LONG end_position_x = PosFromChar(length).x;
  if (end_position_x >= first_position_x) {
    right_bound = std::min(right_bound, end_position_x);  // LTR case.
  }
  // For trailing characters that are 2 pixels wide of less (like "l" in some
  // fonts), we have a problem:
  //   * Clicks on any pixel within the character will place the cursor before
  //     the character.
  //   * Clicks on the pixel just after the character will not allow triple-
  //     click to work properly (true for any last character width).
  // So, we move to the last pixel of the character when this is a
  // triple-click, and moving to one past the last pixel in all other
  // scenarios.  This way, all clicks that can move the cursor will place it at
  // the end of the text, but triple-click will still work.
  if (x < left_bound) {
    return (is_triple_click && ltr_text_in_ltr_layout) ? left_bound - 1 :
                                                         left_bound;
  }
  if ((length == 0) || (x < right_bound))
    return x;
  return is_triple_click ? (right_bound - 1) : right_bound;
}

void NativeTextfieldWin::SetContainsMouse(bool contains_mouse) {
  if (contains_mouse == contains_mouse_)
    return;

  contains_mouse_ = contains_mouse;

  if (!textfield_->draw_border())
    return;

  if (contains_mouse_) {
    // Register for notification when the mouse leaves. Need to do this so
    // that we can reset contains mouse properly.
    TRACKMOUSEEVENT tme;
    tme.cbSize = sizeof(tme);
    tme.dwFlags = TME_LEAVE;
    tme.hwndTrack = m_hWnd;
    tme.dwHoverTime = 0;
    TrackMouseEvent(&tme);
  }
  RedrawWindow(NULL, NULL, RDW_INVALIDATE | RDW_FRAME);
}

ITextDocument* NativeTextfieldWin::GetTextObjectModel() const {
  if (!text_object_model_) {
    base::win::ScopedComPtr<IRichEditOle, &IID_IRichEditOle> ole_interface;
    ole_interface.Attach(GetOleInterface());
    if (ole_interface)
      text_object_model_.QueryFrom(ole_interface);
  }
  return text_object_model_;
}

void NativeTextfieldWin::BuildContextMenu() {
  if (context_menu_contents_.get())
    return;
  context_menu_contents_.reset(new ui::SimpleMenuModel(this));
  context_menu_contents_->AddItemWithStringId(IDS_APP_UNDO, IDS_APP_UNDO);
  context_menu_contents_->AddSeparator();
  context_menu_contents_->AddItemWithStringId(IDS_APP_CUT, IDS_APP_CUT);
  context_menu_contents_->AddItemWithStringId(IDS_APP_COPY, IDS_APP_COPY);
  context_menu_contents_->AddItemWithStringId(IDS_APP_PASTE, IDS_APP_PASTE);
  context_menu_contents_->AddSeparator();
  context_menu_contents_->AddItemWithStringId(IDS_APP_SELECT_ALL,
                                              IDS_APP_SELECT_ALL);
  context_menu_.reset(new Menu2(context_menu_contents_.get()));
}

////////////////////////////////////////////////////////////////////////////////
// NativeTextfieldWrapper, public:

// static
NativeTextfieldWrapper* NativeTextfieldWrapper::CreateWrapper(
    Textfield* field) {
  if (views::Widget::IsPureViews())
    return new NativeTextfieldViews(field);
  return new NativeTextfieldWin(field);
}

}  // namespace views
