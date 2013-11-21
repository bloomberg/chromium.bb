// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/base/ime/remote_input_method_win.h"

#include "base/observer_list.h"
#include "base/strings/utf_string_conversions.h"
#include "ui/base/ime/input_method.h"
#include "ui/base/ime/input_method_delegate.h"
#include "ui/base/ime/input_method_observer.h"
#include "ui/base/ime/remote_input_method_delegate_win.h"
#include "ui/base/ime/text_input_client.h"
#include "ui/base/ime/win/tsf_input_scope.h"
#include "ui/events/event.h"
#include "ui/events/event_utils.h"
#include "ui/gfx/rect.h"

namespace ui {
namespace {

const LANGID kFallbackLangID =
    MAKELANGID(LANG_NEUTRAL, SUBLANG_UI_CUSTOM_DEFAULT);

InputMethod* g_public_interface_ = NULL;
RemoteInputMethodPrivateWin* g_private_interface_ = NULL;

void RegisterInstance(InputMethod* public_interface,
                      RemoteInputMethodPrivateWin* private_interface) {
  CHECK(g_public_interface_ == NULL)
      << "Only one instance is supported at the same time";
  CHECK(g_private_interface_ == NULL)
      << "Only one instance is supported at the same time";
  g_public_interface_ = public_interface;
  g_private_interface_ = private_interface;
}

RemoteInputMethodPrivateWin* GetPrivate(InputMethod* public_interface) {
  if (g_public_interface_ != public_interface)
    return NULL;
  return g_private_interface_;
}

void UnregisterInstance(InputMethod* public_interface) {
  RemoteInputMethodPrivateWin* private_interface = GetPrivate(public_interface);
  if (g_public_interface_ == public_interface &&
      g_private_interface_ == private_interface) {
    g_public_interface_ = NULL;
    g_private_interface_ = NULL;
  }
}

std::string GetLocaleString(LCID Locale_id, LCTYPE locale_type) {
  wchar_t buffer[16] = {};

  //|chars_written| includes NUL terminator.
  const int chars_written =
      GetLocaleInfo(Locale_id, locale_type, buffer, arraysize(buffer));
  if (chars_written <= 1 || arraysize(buffer) < chars_written)
    return std::string();
  std::string result;
  WideToUTF8(buffer, chars_written - 1, &result);
  return result;
}

std::vector<int32> GetInputScopesAsInt(TextInputType text_input_type,
                                       TextInputMode text_input_mode) {
  std::vector<int32> result;
  // An empty vector represents |text_input_type| is TEXT_INPUT_TYPE_NONE.
  if (text_input_type == TEXT_INPUT_TYPE_NONE)
    return result;

  const std::vector<InputScope>& input_scopes =
      tsf_inputscope::GetInputScopes(text_input_type, text_input_mode);
  result.reserve(input_scopes.size());
  for (size_t i = 0; i < input_scopes.size(); ++i)
    result.push_back(static_cast<int32>(input_scopes[i]));
  return result;
}

std::vector<gfx::Rect> GetCompositionCharacterBounds(
    const TextInputClient* client) {
  if (!client)
    return std::vector<gfx::Rect>();

  if (!client->HasCompositionText()) {
    std::vector<gfx::Rect> caret;
    caret.push_back(client->GetCaretBounds());
    return caret;
  }

  std::vector<gfx::Rect> bounds;
  for (uint32 i = 0;; ++i) {
    gfx::Rect rect;
    if (!client->GetCompositionCharacterBounds(i, &rect))
      break;
    bounds.push_back(rect);
  }
  return bounds;
}

class RemoteInputMethodWin : public InputMethod,
                             public RemoteInputMethodPrivateWin {
 public:
  explicit RemoteInputMethodWin(internal::InputMethodDelegate* delegate)
      : delegate_(delegate),
        remote_delegate_(NULL),
        text_input_client_(NULL),
        current_input_type_(ui::TEXT_INPUT_TYPE_NONE),
        current_input_mode_(ui::TEXT_INPUT_MODE_DEFAULT),
        is_candidate_popup_open_(false),
        is_ime_(false),
        langid_(kFallbackLangID) {
    RegisterInstance(this, this);
  }

  virtual ~RemoteInputMethodWin() {
    FOR_EACH_OBSERVER(InputMethodObserver,
                      observer_list_,
                      OnInputMethodDestroyed(this));
    UnregisterInstance(this);
  }

 private:
  // Overridden from InputMethod:
  virtual void SetDelegate(internal::InputMethodDelegate* delegate) OVERRIDE {
    delegate_ = delegate;
  }

  virtual void Init(bool focused) OVERRIDE {
    if (focused)
      OnFocus();
  }

  virtual void OnFocus() OVERRIDE {
    FOR_EACH_OBSERVER(InputMethodObserver,
                      observer_list_,
                      OnFocus());
  }

  virtual void OnBlur() OVERRIDE {
    FOR_EACH_OBSERVER(InputMethodObserver,
                      observer_list_,
                      OnBlur());
  }

  virtual bool OnUntranslatedIMEMessage(const base::NativeEvent& event,
                                        NativeEventResult* result) OVERRIDE {
    FOR_EACH_OBSERVER(InputMethodObserver,
                      observer_list_,
                      OnUntranslatedIMEMessage(event));
    return false;
  }

  virtual void SetFocusedTextInputClient(TextInputClient* client) OVERRIDE {
    std::vector<int32> prev_input_scopes;
    std::swap(input_scopes_, prev_input_scopes);
    std::vector<gfx::Rect> prev_bounds;
    std::swap(composition_character_bounds_, prev_bounds);
    if (client) {
      input_scopes_ = GetInputScopesAsInt(client->GetTextInputType(),
                                          client->GetTextInputMode());
      composition_character_bounds_ = GetCompositionCharacterBounds(client);
      current_input_type_ = client->GetTextInputType();
      current_input_mode_ = client->GetTextInputMode();
    }

    const bool text_input_client_changed = text_input_client_ != client;
    text_input_client_ = client;
    if (text_input_client_changed) {
      FOR_EACH_OBSERVER(InputMethodObserver,
                        observer_list_,
                        OnTextInputStateChanged(client));
    }

    if (!remote_delegate_ || (prev_input_scopes == input_scopes_ &&
                              prev_bounds == composition_character_bounds_))
      return;
    remote_delegate_->OnTextInputClientUpdated(input_scopes_,
                                               composition_character_bounds_);
  }

  virtual void DetachTextInputClient(TextInputClient* client) OVERRIDE {
    if (text_input_client_ != client)
      return;
    SetFocusedTextInputClient(NULL);
  }

  virtual TextInputClient* GetTextInputClient() const OVERRIDE {
    return text_input_client_;
  }

  virtual bool DispatchKeyEvent(const ui::KeyEvent& event) OVERRIDE {
    if (event.HasNativeEvent()) {
      const base::NativeEvent& native_key_event = event.native_event();
      if (native_key_event.message != WM_CHAR)
        return false;
      if (!text_input_client_)
        return false;
      text_input_client_->InsertChar(
          static_cast<char16>(native_key_event.wParam),
          ui::GetModifiersFromKeyState());
      return true;
    }

    if (event.is_char()) {
      if (text_input_client_) {
        text_input_client_->InsertChar(event.key_code(),
                                       ui::GetModifiersFromKeyState());
      }
      return true;
    }
    if (!delegate_)
      return false;
    return delegate_->DispatchFabricatedKeyEventPostIME(event.type(),
                                                        event.key_code(),
                                                        event.flags());
  }

  virtual void OnTextInputTypeChanged(const TextInputClient* client) OVERRIDE {
    if (!text_input_client_ || text_input_client_ != client)
      return;
    const ui::TextInputType prev_type = current_input_type_;
    const ui::TextInputMode prev_mode = current_input_mode_;
    current_input_type_ = client->GetTextInputType();
    current_input_mode_ = client->GetTextInputMode();

    std::vector<int32> prev_input_scopes;
    std::swap(input_scopes_, prev_input_scopes);
    input_scopes_ = GetInputScopesAsInt(client->GetTextInputType(),
                                        client->GetTextInputMode());
    if (input_scopes_ != prev_input_scopes && remote_delegate_) {
      remote_delegate_->OnTextInputClientUpdated(
          input_scopes_, composition_character_bounds_);
    }
    if (current_input_type_ != prev_type || current_input_mode_ != prev_mode) {
      FOR_EACH_OBSERVER(InputMethodObserver,
                        observer_list_,
                        OnTextInputTypeChanged(client));
    }
  }

  virtual void OnCaretBoundsChanged(const TextInputClient* client) OVERRIDE {
    if (!text_input_client_ || text_input_client_ != client)
      return;
    std::vector<gfx::Rect> prev_rects;
    std::swap(composition_character_bounds_, prev_rects);
    composition_character_bounds_ = GetCompositionCharacterBounds(client);
    if (composition_character_bounds_ != prev_rects && remote_delegate_) {
      remote_delegate_->OnTextInputClientUpdated(
          input_scopes_, composition_character_bounds_);
    }
    FOR_EACH_OBSERVER(InputMethodObserver,
                      observer_list_,
                      OnCaretBoundsChanged(client));
  }

  virtual void CancelComposition(const TextInputClient* client) OVERRIDE {
    if (CanSendRemoteNotification(client))
      remote_delegate_->CancelComposition();
  }

  virtual void OnInputLocaleChanged() OVERRIDE {
    FOR_EACH_OBSERVER(InputMethodObserver,
                      observer_list_,
                      OnInputLocaleChanged());
  }

  virtual std::string GetInputLocale() OVERRIDE {
    const LCID locale_id = MAKELCID(langid_, SORT_DEFAULT);
    std::string language =
        GetLocaleString(locale_id, LOCALE_SISO639LANGNAME);
    if (SUBLANGID(langid_) == SUBLANG_NEUTRAL || language.empty())
      return language;
    const std::string& region =
        GetLocaleString(locale_id, LOCALE_SISO3166CTRYNAME);
    if (region.empty())
      return language;
    return language.append(1, '-').append(region);
  }

  virtual base::i18n::TextDirection GetInputTextDirection() OVERRIDE {
    switch (PRIMARYLANGID(langid_)) {
      case LANG_ARABIC:
      case LANG_HEBREW:
      case LANG_PERSIAN:
      case LANG_SYRIAC:
      case LANG_UIGHUR:
      case LANG_URDU:
        return base::i18n::RIGHT_TO_LEFT;
      default:
        return base::i18n::LEFT_TO_RIGHT;
    }
  }

  virtual bool IsActive() OVERRIDE {
    return true;  // always turned on
  }

  virtual TextInputType GetTextInputType() const OVERRIDE {
    return text_input_client_ ? text_input_client_->GetTextInputType()
                              : TEXT_INPUT_TYPE_NONE;
  }

  virtual TextInputMode GetTextInputMode() const OVERRIDE {
    return text_input_client_ ? text_input_client_->GetTextInputMode()
                              : TEXT_INPUT_MODE_DEFAULT;
  }

  virtual bool CanComposeInline() const OVERRIDE {
    return text_input_client_ ? text_input_client_->CanComposeInline() : true;
  }

  virtual bool IsCandidatePopupOpen() const OVERRIDE {
    return is_candidate_popup_open_;
  }

  virtual void AddObserver(InputMethodObserver* observer) OVERRIDE {
    observer_list_.AddObserver(observer);
  }

  virtual void RemoveObserver(InputMethodObserver* observer) OVERRIDE {
    observer_list_.RemoveObserver(observer);
  }

  // Overridden from RemoteInputMethodPrivateWin:
  virtual void SetRemoteDelegate(
      internal::RemoteInputMethodDelegateWin* delegate) OVERRIDE{
    remote_delegate_ = delegate;

    // Sync initial state.
    if (remote_delegate_) {
      remote_delegate_->OnTextInputClientUpdated(
          input_scopes_, composition_character_bounds_);
    }
  }

  virtual void OnCandidatePopupChanged(bool visible) OVERRIDE {
    is_candidate_popup_open_ = visible;
  }

  virtual void OnInputSourceChanged(LANGID langid, bool /*is_ime*/) OVERRIDE {
    // Note: Currently |is_ime| is not utilized yet.
    const bool changed = (langid_ != langid);
    langid_ = langid;
    if (changed && GetTextInputClient())
      GetTextInputClient()->OnInputMethodChanged();
  }

  bool CanSendRemoteNotification(
      const TextInputClient* text_input_client) const {
    return text_input_client_ &&
           text_input_client_ == text_input_client &&
           remote_delegate_;
  }

  ObserverList<InputMethodObserver> observer_list_;

  internal::InputMethodDelegate* delegate_;
  internal::RemoteInputMethodDelegateWin* remote_delegate_;

  TextInputClient* text_input_client_;
  ui::TextInputType current_input_type_;
  ui::TextInputMode current_input_mode_;
  std::vector<int32> input_scopes_;
  std::vector<gfx::Rect> composition_character_bounds_;
  bool is_candidate_popup_open_;
  bool is_ime_;
  LANGID langid_;

  DISALLOW_COPY_AND_ASSIGN(RemoteInputMethodWin);
};

}  // namespace

RemoteInputMethodPrivateWin::RemoteInputMethodPrivateWin() {}

scoped_ptr<InputMethod> CreateRemoteInputMethodWin(
    internal::InputMethodDelegate* delegate) {
  return scoped_ptr<InputMethod>(new RemoteInputMethodWin(delegate));
}

// static
RemoteInputMethodPrivateWin* RemoteInputMethodPrivateWin::Get(
    InputMethod* input_method) {
  return GetPrivate(input_method);
}

}  // namespace ui
