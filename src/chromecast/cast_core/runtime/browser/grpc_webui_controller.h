// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_CAST_CORE_RUNTIME_BROWSER_GRPC_WEBUI_CONTROLLER_H_
#define CHROMECAST_CAST_CORE_RUNTIME_BROWSER_GRPC_WEBUI_CONTROLLER_H_

#include <memory>
#include <vector>

#include "base/containers/flat_map.h"
#include "base/values.h"
#include "content/public/browser/web_ui_controller.h"
#include "third_party/cast_core/public/src/proto/v2/core_application_service.grpc.pb.h"

namespace content {
class BrowserContext;
class WebContents;
class WebUI;
}  // namespace content

namespace chromecast {

class CastWebUIMessageHandler;

// WebUI is a part of the Chrome Web Technologies stack used to load UIs for
// chrome: URLs. GrpcWebUIController is used for rendering Backdrop (IdleScreen)
// on devices running CastCore receiver + Web runtime.
class GrpcWebUIController : public content::WebUIController {
 public:
  // |webui| stays alive for the lifetime of the Backdrop app. Every time the
  // screen switches from casting to idle screen (Backdrop), a new WebUI
  // instance is created.
  GrpcWebUIController(
      content::WebUI* webui,
      const std::string host,
      cast::v2::CoreApplicationService::Stub core_app_service_stub);
  ~GrpcWebUIController() override;

  // Creates an instance of GrpcWebUIController.
  // Implementation is inside GrpcExtensionWebUI.
  static std::unique_ptr<GrpcWebUIController> Create(
      content::WebUI* webui,
      const std::string host,
      cast::v2::CoreApplicationService::Stub core_app_service_stub);

 protected:
  content::WebContents* web_contents() const;
  content::BrowserContext* browser_context() const;

 private:
  content::WebContents* const web_contents_;
  content::BrowserContext* const browser_context_;
  void AddWebviewSupport();
  void OnWebUIReady(const base::ListValue* args);
  void InvokeCallback(const std::string& message, const base::ListValue* args);
  void RegisterMessageCallbacks();

  // Callbacks from javascript
  void StartPingNotify(const base::ListValue* args);
  void StopPingNotify(const base::ListValue* args);
  void SetOobeFinished(const base::ListValue* args);
  void RecordAction(const base::ListValue* args);
  void LaunchTutorial(const base::ListValue* args);
  void GetQRCode(const base::ListValue* args);
  void CallJavascriptFunction(const std::string& function,
                              std::vector<base::Value> args);

  // Pointer to the generic message handler owned by the ctor provided|webui|.
  // The message handler is guaranteed to outlive GrpcWebUIController since
  // |this| is the first member to be deleted in the Web UI.
  CastWebUIMessageHandler* message_handler_;
};

}  // namespace chromecast

#endif  // CHROMECAST_CAST_CORE_RUNTIME_BROWSER_GRPC_WEBUI_CONTROLLER_H_
