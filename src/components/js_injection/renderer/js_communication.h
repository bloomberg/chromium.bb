// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_JS_INJECTION_RENDERER_JS_COMMUNICATION_H_
#define COMPONENTS_JS_INJECTION_RENDERER_JS_COMMUNICATION_H_

#include <vector>

#include "base/strings/string16.h"
#include "components/js_injection/common/interfaces.mojom.h"
#include "content/public/renderer/render_frame_observer.h"
#include "content/public/renderer/render_frame_observer_tracker.h"
#include "mojo/public/cpp/bindings/associated_receiver.h"
#include "mojo/public/cpp/bindings/associated_remote.h"
#include "third_party/blink/public/platform/web_string.h"

namespace content {
class RenderFrame;
}

namespace js_injection {

class JsBinding;

class JsCommunication
    : public mojom::JsCommunication,
      public content::RenderFrameObserver,
      public content::RenderFrameObserverTracker<JsCommunication> {
 public:
  explicit JsCommunication(content::RenderFrame* render_frame);
  ~JsCommunication() override;

  // mojom::JsCommunication implementation
  void SetJsObjects(std::vector<mojom::JsObjectPtr> js_object_ptrs) override;
  void AddDocumentStartScript(
      mojom::DocumentStartJavaScriptPtr script_ptr) override;
  void RemoveDocumentStartScript(int32_t script_id) override;

  // RenderFrameObserver implementation
  void DidClearWindowObject() override;
  void WillReleaseScriptContext(v8::Local<v8::Context> context,
                                int32_t world_id) override;
  void OnDestruct() override;

  void RunScriptsAtDocumentStart();

  mojom::JsToBrowserMessaging* GetJsToJavaMessage(
      const base::string16& js_object_name);

 private:
  struct JsObjectInfo;
  struct DocumentStartJavaScript;

  void BindPendingReceiver(
      mojo::PendingAssociatedReceiver<mojom::JsCommunication> pending_receiver);

  using JsObjectMap = std::map<base::string16, std::unique_ptr<JsObjectInfo>>;
  JsObjectMap js_objects_;

  // In some cases DidClearWindowObject will be called twice in a row, we need
  // to prevent doing multiple injection in that case.
  bool inside_did_clear_window_object_ = false;

  std::vector<std::unique_ptr<DocumentStartJavaScript>> scripts_;
  std::vector<std::unique_ptr<JsBinding>> js_bindings_;

  // Associated with legacy IPC channel.
  mojo::AssociatedReceiver<mojom::JsCommunication> receiver_{this};

  DISALLOW_COPY_AND_ASSIGN(JsCommunication);
};

}  // namespace js_injection

#endif  // COMPONENTS_JS_INJECTION_RENDERER_JS_COMMUNICATION_H_
