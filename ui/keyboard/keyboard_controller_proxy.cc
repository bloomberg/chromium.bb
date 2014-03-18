// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/keyboard/keyboard_controller_proxy.h"

#include "base/values.h"
#include "content/public/browser/site_instance.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_delegate.h"
#include "content/public/browser/web_contents_observer.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/browser/web_ui.h"
#include "content/public/common/bindings_policy.h"
#include "ui/aura/layout_manager.h"
#include "ui/aura/window.h"
#include "ui/keyboard/keyboard_constants.h"

namespace {

// Converts ui::TextInputType to string.
std::string TextInputTypeToString(ui::TextInputType type) {
  switch (type) {
    case ui::TEXT_INPUT_TYPE_NONE:
      return "none";
    case ui::TEXT_INPUT_TYPE_PASSWORD:
      return "password";
    case ui::TEXT_INPUT_TYPE_EMAIL:
      return "email";
    case ui::TEXT_INPUT_TYPE_NUMBER:
      return "number";
    case ui::TEXT_INPUT_TYPE_TELEPHONE:
      return "tel";
    case ui::TEXT_INPUT_TYPE_URL:
      return "url";
    case ui::TEXT_INPUT_TYPE_DATE:
      return "date";
    case ui::TEXT_INPUT_TYPE_TEXT:
    case ui::TEXT_INPUT_TYPE_SEARCH:
    case ui::TEXT_INPUT_TYPE_DATE_TIME:
    case ui::TEXT_INPUT_TYPE_DATE_TIME_LOCAL:
    case ui::TEXT_INPUT_TYPE_MONTH:
    case ui::TEXT_INPUT_TYPE_TIME:
    case ui::TEXT_INPUT_TYPE_WEEK:
    case ui::TEXT_INPUT_TYPE_TEXT_AREA:
    case ui::TEXT_INPUT_TYPE_CONTENT_EDITABLE:
    case ui::TEXT_INPUT_TYPE_DATE_TIME_FIELD:
      return "text";
  }
  NOTREACHED();
  return "";
}

// The WebContentsDelegate for the keyboard.
// The delegate deletes itself when the keyboard is destroyed.
class KeyboardContentsDelegate : public content::WebContentsDelegate,
                                 public content::WebContentsObserver {
 public:
  KeyboardContentsDelegate(keyboard::KeyboardControllerProxy* proxy)
      : proxy_(proxy) {}
  virtual ~KeyboardContentsDelegate() {}

 private:
  // Overridden from content::WebContentsDelegate:
  virtual content::WebContents* OpenURLFromTab(
      content::WebContents* source,
      const content::OpenURLParams& params) OVERRIDE {
    source->GetController().LoadURL(
        params.url, params.referrer, params.transition, params.extra_headers);
    Observe(source);
    return source;
  }

  virtual bool IsPopupOrPanel(
      const content::WebContents* source) const OVERRIDE {
    return true;
  }

  virtual void MoveContents(content::WebContents* source,
                            const gfx::Rect& pos) OVERRIDE {
    aura::Window* keyboard = proxy_->GetKeyboardWindow();
    gfx::Rect bounds = keyboard->bounds();
    int new_height = pos.height();
    bounds.set_y(bounds.y() + bounds.height() - new_height);
    bounds.set_height(new_height);
    proxy_->set_resizing_from_contents(true);
    keyboard->SetBounds(bounds);
    proxy_->set_resizing_from_contents(false);
  }

  // Overridden from content::WebContentsDelegate:
  virtual void RequestMediaAccessPermission(content::WebContents* web_contents,
      const content::MediaStreamRequest& request,
      const content::MediaResponseCallback& callback) OVERRIDE {
    proxy_->RequestAudioInput(web_contents, request, callback);
  }

  // Overridden from content::WebContentsObserver:
  virtual void WebContentsDestroyed(content::WebContents* contents) OVERRIDE {
    delete this;
  }

  keyboard::KeyboardControllerProxy* proxy_;

  DISALLOW_COPY_AND_ASSIGN(KeyboardContentsDelegate);
};

}  // namespace

namespace keyboard {

KeyboardControllerProxy::KeyboardControllerProxy()
    : default_url_(kKeyboardWebUIURL), resizing_from_contents_(false) {
}

KeyboardControllerProxy::~KeyboardControllerProxy() {
}

const GURL& KeyboardControllerProxy::GetVirtualKeyboardUrl() {
  return override_url_.is_valid() ? override_url_ : default_url_;
}

void KeyboardControllerProxy::SetOverrideContentUrl(const GURL& url) {
  if (override_url_ == url)
    return;

  override_url_ = url;
  // Restores the keyboard window size to default.
  aura::Window* container = GetKeyboardWindow()->parent();
  if (container) {
    container->layout_manager()->OnWindowResized();
    LoadContents(GetVirtualKeyboardUrl());
  }
}

void KeyboardControllerProxy::LoadContents(const GURL& url) {
  if (keyboard_contents_) {
    content::OpenURLParams params(
        url,
        content::Referrer(),
        SINGLETON_TAB,
        content::PAGE_TRANSITION_AUTO_TOPLEVEL,
        false);
    keyboard_contents_->OpenURL(params);
  }
}

aura::Window* KeyboardControllerProxy::GetKeyboardWindow() {
  if (!keyboard_contents_) {
    content::BrowserContext* context = GetBrowserContext();
    keyboard_contents_.reset(content::WebContents::Create(
        content::WebContents::CreateParams(context,
            content::SiteInstance::CreateForURL(context,
                                                GetVirtualKeyboardUrl()))));
    keyboard_contents_->SetDelegate(new KeyboardContentsDelegate(this));
    SetupWebContents(keyboard_contents_.get());
    LoadContents(GetVirtualKeyboardUrl());
  }

  return keyboard_contents_->GetView()->GetNativeView();
}

bool KeyboardControllerProxy::HasKeyboardWindow() const {
  return keyboard_contents_;
}

void KeyboardControllerProxy::ShowKeyboardContainer(aura::Window* container) {
  GetKeyboardWindow()->Show();
  container->Show();
}

void KeyboardControllerProxy::HideKeyboardContainer(aura::Window* container) {
  container->Hide();
  GetKeyboardWindow()->Hide();
}

void KeyboardControllerProxy::SetUpdateInputType(ui::TextInputType type) {
  content::WebUI* webui = keyboard_contents_ ?
      keyboard_contents_->GetCommittedWebUI() : NULL;

  if (webui &&
      (0 != (webui->GetBindings() & content::BINDINGS_POLICY_WEB_UI))) {
    // Only call OnTextInputBoxFocused function if it is a web ui keyboard,
    // not an extension based keyboard.
    base::DictionaryValue input_context;
    input_context.SetString("type", TextInputTypeToString(type));
    webui->CallJavascriptFunction("OnTextInputBoxFocused", input_context);
  }
}

void KeyboardControllerProxy::EnsureCaretInWorkArea() {
}

void KeyboardControllerProxy::LoadSystemKeyboard() {
  DCHECK(keyboard_contents_);
  if (keyboard_contents_->GetURL() != default_url_) {
    // TODO(bshe): The height of system virtual keyboard and IME virtual
    // keyboard may different. The height needs to be restored too.
    LoadContents(default_url_);
  }
}

void KeyboardControllerProxy::ReloadKeyboardIfNeeded() {
  DCHECK(keyboard_contents_);
  if (keyboard_contents_->GetURL() != GetVirtualKeyboardUrl()) {
    LoadContents(GetVirtualKeyboardUrl());
  }
}

void KeyboardControllerProxy::SetupWebContents(content::WebContents* contents) {
}

}  // namespace keyboard
