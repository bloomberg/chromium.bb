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
#include "ui/aura/window.h"
#include "ui/keyboard/keyboard_constants.h"

namespace {

// Converts ui::TextInputType to string. The returned string should consistent
// with the type enum of chrome.input.ime.InputContext.
std::string TextInputTypeToString(ui::TextInputType type) {
  switch (type) {
    // TODO(bshe): Supports more input types.
    case ui::TEXT_INPUT_TYPE_PASSWORD:
      return "password";
    case ui::TEXT_INPUT_TYPE_NUMBER:
      return "number";
    default:
      return "text";
  }
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

KeyboardControllerProxy::KeyboardControllerProxy() {
}

KeyboardControllerProxy::~KeyboardControllerProxy() {
}

aura::Window* KeyboardControllerProxy::GetKeyboardWindow() {
  if (!keyboard_contents_) {
    content::BrowserContext* context = GetBrowserContext();
    GURL url(kKeyboardWebUIURL);
    keyboard_contents_.reset(content::WebContents::Create(
        content::WebContents::CreateParams(context,
            content::SiteInstance::CreateForURL(context, url))));
    keyboard_contents_->SetDelegate(new KeyboardContentsDelegate(this));
    SetupWebContents(keyboard_contents_.get());

    content::OpenURLParams params(url,
                                  content::Referrer(),
                                  SINGLETON_TAB,
                                  content::PAGE_TRANSITION_AUTO_TOPLEVEL,
                                  false);
    keyboard_contents_->OpenURL(params);
  }

  return keyboard_contents_->GetView()->GetNativeView();
}

void KeyboardControllerProxy::ShowKeyboardContainer(aura::Window* container) {
  container->Show();
}

void KeyboardControllerProxy::HideKeyboardContainer(aura::Window* container) {
  container->Hide();
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

void KeyboardControllerProxy::SetupWebContents(content::WebContents* contents) {
}

}  // namespace keyboard
