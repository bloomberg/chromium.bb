// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBUI_WEB_UI_IMPL_H_
#define CONTENT_BROWSER_WEBUI_WEB_UI_IMPL_H_

#include <map>
#include <memory>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/common/content_export.h"
#include "content/common/web_ui.mojom.h"
#include "content/public/browser/web_ui.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"

namespace content {
class RenderFrameHost;
class RenderFrameHostImpl;
class WebContentsImpl;
class WebUIMainFrameObserver;

class CONTENT_EXPORT WebUIImpl : public WebUI,
                                 public mojom::WebUIHost,
                                 public base::SupportsWeakPtr<WebUIImpl> {
 public:
  explicit WebUIImpl(WebContentsImpl* contents,
                     RenderFrameHostImpl* frame_host);
  ~WebUIImpl() override;
  WebUIImpl(const WebUIImpl&) = delete;
  WebUIImpl& operator=(const WebUIImpl&) = delete;

  // Called when a RenderFrame is created for a WebUI (reload after a renderer
  // crash) or when a WebUI is created for a RenderFrame (i.e. navigating from
  // chrome://downloads to chrome://bookmarks) or when both are new (i.e.
  // opening a new tab).
  void WebUIRenderFrameCreated(RenderFrameHost* render_frame_host);

  // Called when a RenderFrame is reused for the same WebUI type (i.e. reload).
  void RenderFrameReused(RenderFrameHost* render_frame_host);

  // Called when the owning RenderFrameHost has started unloading.
  void RenderFrameHostUnloading();

  // Called when the renderer-side frame is destroyed, along with any mojo
  // connections to it. The browser can not attempt to communicate with the
  // renderer afterward.
  void RenderFrameDeleted();

  // Called right after AllowBindings is notified to a RenderFrame.
  void SetUpMojoConnection();

  // Called when a RenderFrame is deleted for a WebUI (i.e. a renderer crash).
  void TearDownMojoConnection();

  // Add a property to the WebUI binding object.
  void SetProperty(const std::string& name, const std::string& value);

  // WebUI implementation:
  WebContents* GetWebContents() override;
  WebUIController* GetController() override;
  void SetController(std::unique_ptr<WebUIController> controller) override;
  float GetDeviceScaleFactor() override;
  const std::u16string& GetOverriddenTitle() override;
  void OverrideTitle(const std::u16string& title) override;
  int GetBindings() override;
  void SetBindings(int bindings) override;
  const std::vector<std::string>& GetRequestableSchemes() override;
  void AddRequestableScheme(const char* scheme) override;
  void AddMessageHandler(std::unique_ptr<WebUIMessageHandler> handler) override;
  void RegisterMessageCallback(base::StringPiece message,
                               MessageCallback callback) override;
  void RegisterDeprecatedMessageCallback(
      base::StringPiece message,
      const DeprecatedMessageCallback& callback) override;
  void ProcessWebUIMessage(const GURL& source_url,
                           const std::string& message,
                           const base::ListValue& args) override;
  bool CanCallJavascript() override;
  void CallJavascriptFunctionUnsafe(const std::string& function_name) override;
  void CallJavascriptFunctionUnsafe(const std::string& function_name,
                                    const base::Value& arg) override;
  void CallJavascriptFunctionUnsafe(const std::string& function_name,
                                    const base::Value& arg1,
                                    const base::Value& arg2) override;
  void CallJavascriptFunctionUnsafe(const std::string& function_name,
                                    const base::Value& arg1,
                                    const base::Value& arg2,
                                    const base::Value& arg3) override;
  void CallJavascriptFunctionUnsafe(const std::string& function_name,
                                    const base::Value& arg1,
                                    const base::Value& arg2,
                                    const base::Value& arg3,
                                    const base::Value& arg4) override;
  void CallJavascriptFunctionUnsafe(
      const std::string& function_name,
      const std::vector<const base::Value*>& args) override;
  std::vector<std::unique_ptr<WebUIMessageHandler>>* GetHandlersForTesting()
      override;

  const mojo::AssociatedRemote<mojom::WebUI>& GetRemoteForTest() const {
    return remote_;
  }
  WebUIMainFrameObserver* GetWebUIMainFrameObserverForTest() const {
    return web_contents_observer_.get();
  }

  RenderFrameHostImpl* frame_host() const { return frame_host_; }

 private:
  friend class WebUIMainFrameObserver;

  // mojom::WebUIHost
  void Send(const std::string& message, base::Value args) override;

  // Execute a string of raw JavaScript on the page.
  void ExecuteJavascript(const std::u16string& javascript);

  // Called internally and by the owned WebUIMainFrameObserver.
  void DisallowJavascriptOnAllHandlers();

  // A map of message name -> message handling callback.
  std::map<std::string, MessageCallback> message_callbacks_;

  // A map of message name -> message handling callback.
  std::map<std::string, DeprecatedMessageCallback>
      deprecated_message_callbacks_;

  // Options that may be overridden by individual Web UI implementations. The
  // bool options default to false. See the public getters for more information.
  std::u16string overridden_title_;  // Defaults to empty string.
  int bindings_;  // The bindings from BindingsPolicy that should be enabled for
                  // this page.

  // The URL schemes that can be requested by this document.
  std::vector<std::string> requestable_schemes_;

  // RenderFrameHost associated with |this|.
  raw_ptr<RenderFrameHostImpl> frame_host_;

  // Non-owning pointer to the WebContentsImpl this WebUI is associated with.
  raw_ptr<WebContentsImpl> web_contents_;

  // The WebUIMessageHandlers we own.
  std::vector<std::unique_ptr<WebUIMessageHandler>> handlers_;

  // Notifies this WebUI about notifications in the main frame.
  std::unique_ptr<WebUIMainFrameObserver> web_contents_observer_;

  std::unique_ptr<WebUIController> controller_;

  mojo::AssociatedRemote<mojom::WebUI> remote_;
  mojo::AssociatedReceiver<mojom::WebUIHost> receiver_{this};
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBUI_WEB_UI_IMPL_H_
