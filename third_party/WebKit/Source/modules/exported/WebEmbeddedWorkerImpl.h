/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebEmbeddedWorkerImpl_h
#define WebEmbeddedWorkerImpl_h

#include <memory>
#include "core/workers/WorkerClients.h"
#include "modules/ModulesExport.h"
#include "platform/WebTaskRunner.h"
#include "platform/heap/Handle.h"
#include "public/platform/Platform.h"
#include "public/platform/WebContentSecurityPolicy.h"
#include "public/web/WebDevToolsAgentClient.h"
#include "public/web/WebEmbeddedWorker.h"
#include "public/web/WebEmbeddedWorkerStartData.h"
#include "public/web/WebFrameClient.h"

namespace blink {

class ThreadableLoadingContext;
class ServiceWorkerGlobalScopeProxy;
class ServiceWorkerInstalledScriptsManager;
class WebLocalFrameBase;
class WebView;
class WorkerInspectorProxy;
class WorkerScriptLoader;
class WorkerThread;

class MODULES_EXPORT WebEmbeddedWorkerImpl final
    : public WebEmbeddedWorker,
      public WebFrameClient,
      NON_EXPORTED_BASE(public WebDevToolsAgentClient) {
  WTF_MAKE_NONCOPYABLE(WebEmbeddedWorkerImpl);

 public:
  WebEmbeddedWorkerImpl(
      std::unique_ptr<WebServiceWorkerContextClient>,
      std::unique_ptr<WebServiceWorkerInstalledScriptsManager>,
      std::unique_ptr<WebContentSettingsClient>);
  ~WebEmbeddedWorkerImpl() override;

  // WebEmbeddedWorker overrides.
  void StartWorkerContext(const WebEmbeddedWorkerStartData&) override;
  void TerminateWorkerContext() override;
  void ResumeAfterDownload() override;
  void AttachDevTools(const WebString& host_id, int session_id) override;
  void ReattachDevTools(const WebString& host_id,
                        int session_id,
                        const WebString& saved_state) override;
  void DetachDevTools(int session_id) override;
  void DispatchDevToolsMessage(int session_id,
                               int call_id,
                               const WebString& method,
                               const WebString& message) override;
  void AddMessageToConsole(const WebConsoleMessage&) override;

  void PostMessageToPageInspector(int session_id, const WTF::String&);
  std::unique_ptr<blink::WebURLLoader> CreateURLLoader(
      const WebURLRequest& request,
      SingleThreadTaskRunner* task_runner) override {
    // TODO(yhirano): Stop using Platform::CreateURLLoader() here.
    return Platform::Current()->CreateURLLoader(request, task_runner);
  }

 private:
  void PrepareShadowPageForLoader();
  void LoadShadowPage();

  // WebFrameClient overrides.
  void FrameDetached(WebLocalFrame*, DetachType) override;
  void DidFinishDocumentLoad() override;

  // WebDevToolsAgentClient overrides.
  void SendProtocolMessage(int session_id,
                           int call_id,
                           const WebString&,
                           const WebString&) override;
  void ResumeStartup() override;
  WebDevToolsAgentClient::WebKitClientMessageLoop* CreateClientMessageLoop()
      override;

  void OnScriptLoaderFinished();
  void StartWorkerThread();

  WebEmbeddedWorkerStartData worker_start_data_;

  std::unique_ptr<WebServiceWorkerContextClient> worker_context_client_;

  // This is valid until the worker thread is created. After the worker thread
  // is created, this is passed to the worker thread.
  std::unique_ptr<ServiceWorkerInstalledScriptsManager>
      installed_scripts_manager_;

  // This is kept until startWorkerContext is called, and then passed on
  // to WorkerContext.
  std::unique_ptr<WebContentSettingsClient> content_settings_client_;

  // Kept around only while main script loading is ongoing.
  RefPtr<WorkerScriptLoader> main_script_loader_;

  std::unique_ptr<WorkerThread> worker_thread_;
  Persistent<ServiceWorkerGlobalScopeProxy> worker_global_scope_proxy_;
  Persistent<WorkerInspectorProxy> worker_inspector_proxy_;

  // 'shadow page' - created to proxy loading requests from the worker.
  // Both WebView and WebFrame objects are close()'ed (where they're
  // deref'ed) when this EmbeddedWorkerImpl is destructed, therefore they
  // are guaranteed to exist while this object is around.
  WebView* web_view_;

  Persistent<WebLocalFrameBase> main_frame_;
  Persistent<ThreadableLoadingContext> loading_context_;

  bool loading_shadow_page_;
  bool asked_to_terminate_;

  enum WaitingForDebuggerState { kWaitingForDebugger, kNotWaitingForDebugger };

  enum {
    kDontPauseAfterDownload,
    kDoPauseAfterDownload,
    kIsPausedAfterDownload
  } pause_after_download_state_;

  WaitingForDebuggerState waiting_for_debugger_state_;
};

extern template class WorkerClientsInitializer<WebEmbeddedWorkerImpl>;

}  // namespace blink

#endif  // WebEmbeddedWorkerImpl_h
