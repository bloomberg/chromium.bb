// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_JS_API_HANDLER_H_
#define ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_JS_API_HANDLER_H_

#include <vector>

#include "android_webview/browser/js_java_interaction/js_reply_proxy.h"
#include "android_webview/common/js_java_interaction/interfaces.mojom.h"
#include "base/strings/string16.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "mojo/public/cpp/bindings/pending_associated_receiver.h"
#include "mojo/public/cpp/system/message_pipe.h"

namespace content {
class RenderFrameHost;
}

namespace android_webview {

// Implementation of mojo::JsToJavaMessaging interface. Receives PostMessage()
// call from renderer JsBinding.
class JsApiHandler : public mojom::JsToJavaMessaging {
 public:
  explicit JsApiHandler(content::RenderFrameHost* rfh);
  ~JsApiHandler() override;

  // Binds mojo.
  void BindPendingReceiver(
      mojo::PendingAssociatedReceiver<mojom::JsToJavaMessaging>
          pending_receiver);

  // mojom::JsToJavaMessaging implementation.
  void PostMessage(const base::string16& message,
                   std::vector<mojo::ScopedMessagePipeHandle> ports) override;
  void SetJavaToJsMessaging(mojo::PendingRemote<mojom::JavaToJsMessaging>
                                java_to_js_messaging) override;

 private:
  std::unique_ptr<JsReplyProxy> reply_proxy_;
  content::RenderFrameHost* render_frame_host_;
  mojo::AssociatedRemote<mojom::JsJavaConfigurator> configurator_remote_;
  mojo::AssociatedReceiver<mojom::JsToJavaMessaging> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(JsApiHandler);
};

}  // namespace android_webview

#endif  // ANDROID_WEBVIEW_BROWSER_JS_JAVA_INTERACTION_JS_API_HANDLER_H_
