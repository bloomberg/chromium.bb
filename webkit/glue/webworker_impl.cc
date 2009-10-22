// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "base/compiler_specific.h"

#include "DedicatedWorkerContext.h"
#include "DedicatedWorkerThread.h"
#include "GenericWorkerTask.h"
#include "KURL.h"
#include "MessageEvent.h"
#include "MessagePort.h"
#include "MessagePortChannel.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "SerializedScriptValue.h"
#include "SubstituteData.h"
#include <wtf/MainThread.h>
#include <wtf/Threading.h>

#undef LOG

#include "webkit/api/public/WebFrameClient.h"
#include "webkit/api/public/WebMessagePortChannel.h"
#include "webkit/api/public/WebScreenInfo.h"
#include "webkit/api/public/WebString.h"
#include "webkit/api/public/WebURL.h"
#include "webkit/api/public/WebView.h"
#include "webkit/api/public/WebWorkerClient.h"
#include "webkit/api/src/PlatformMessagePortChannel.h"
#include "webkit/api/src/WebDataSourceImpl.h"
#include "webkit/glue/empty_webframeclient.h"
#include "webkit/glue/glue_util.h"
#include "webkit/glue/webframe_impl.h"
#include "webkit/glue/webpreferences.h"
#include "webkit/glue/webworker_impl.h"

using WebKit::WebCursorInfo;
using WebKit::WebDataSource;
using WebKit::WebDataSourceImpl;
using WebKit::WebFrame;
using WebKit::WebFrameClient;
using WebKit::WebMessagePortChannel;
using WebKit::WebMessagePortChannelArray;
using WebKit::WebNavigationPolicy;
using WebKit::WebRect;
using WebKit::WebScreenInfo;
using WebKit::WebString;
using WebKit::WebURL;
using WebKit::WebWorker;
using WebKit::WebWorkerClient;
using WebKit::WebView;

#if ENABLE(WORKERS)

// Dummy WebViewDelegate - we only need it in Worker process to load a
// 'shadow page' which will initialize WebCore loader.
class WorkerWebFrameClient : public webkit_glue::EmptyWebFrameClient {
 public:
  // Tell the loader to load the data into the 'shadow page' synchronously,
  // so we can grab the resulting Document right after load.
  virtual void didCreateDataSource(WebFrame* frame, WebDataSource* ds) {
    static_cast<WebDataSourceImpl*>(ds)->setDeferMainResourceDataLoad(false);
  }

  // Lazy allocate and leak this instance.
  static WorkerWebFrameClient* GetSharedInstance() {
    static WorkerWebFrameClient client;
    return &client;
  }

 private:
  WorkerWebFrameClient() {}
};

namespace WebKit {

WebWorker* WebWorker::create(WebWorkerClient* client) {
  return new WebWorkerImpl(client);
}

}

// This function is called on the main thread to force to initialize some static
// values used in WebKit before any worker thread is started. This is because in
// our worker processs, we do not run any WebKit code in main thread and thus
// when multiple workers try to start at the same time, we might hit crash due
// to contention for initializing static values.
void InitializeWebKitStaticValues() {
  static bool initialized = false;
  if (!initialized) {
    initialized= true;
    // Note that we have to pass a URL with valid protocol in order to follow
    // the path to do static value initializations.
    WTF::RefPtr<WebCore::SecurityOrigin> origin =
        WebCore::SecurityOrigin::create(WebCore::KURL(WebCore::ParsedURLString,
                                                      "http://localhost"));
    origin.release();
  }
}

WebWorkerImpl::WebWorkerImpl(WebWorkerClient* client)
 : client_(client),
   web_view_(NULL),
   asked_to_terminate_(false) {
  InitializeWebKitStaticValues();
}

WebWorkerImpl::~WebWorkerImpl() {
  web_view_->close();
}

void WebWorkerImpl::PostMessageToWorkerContextTask(
    WebCore::ScriptExecutionContext* context,
    WebWorkerImpl* this_ptr,
    const WebCore::String& message,
    WTF::PassOwnPtr<WebCore::MessagePortChannelArray> channels) {
  ASSERT(context->isWorkerContext());
  WebCore::DedicatedWorkerContext* worker_context =
      static_cast<WebCore::DedicatedWorkerContext*>(context);

  WTF::OwnPtr<WebCore::MessagePortArray> ports =
      WebCore::MessagePort::entanglePorts(*context, channels.release());
  WTF::RefPtr<WebCore::SerializedScriptValue> serialized_message =
      WebCore::SerializedScriptValue::create(message);
  worker_context->dispatchEvent(
      WebCore::MessageEvent::create(ports.release(),
                                    serialized_message.release()));

  this_ptr->confirmMessageFromWorkerObject(
      worker_context->hasPendingActivity());
}

// WebWorker -------------------------------------------------------------------

void WebWorkerImpl::startWorkerContext(const WebURL& script_url,
                                       const WebString& user_agent,
                                       const WebString& source_code) {
  // Create 'shadow page'. This page is never displayed, it is used to proxy the
  // loading requests from the worker context to the rest of WebKit and Chromium
  // infrastructure.
  ASSERT(!web_view_);
  web_view_ = WebView::create(NULL);
  WebPreferences().Apply(web_view_);
  web_view_->initializeMainFrame(WorkerWebFrameClient::GetSharedInstance());

  WebFrameImpl* web_frame = static_cast<WebFrameImpl*>(web_view_->mainFrame());

  // Construct substitute data source for the 'shadow page'. We only need it
  // to have same origin as the worker so the loading checks work correctly.
  WebCore::CString content("");
  int len = static_cast<int>(content.length());
  RefPtr<WebCore::SharedBuffer> buf(
      WebCore::SharedBuffer::create(content.data(), len));
  WebCore::SubstituteData subst_data(buf,
                                     WebCore::String("text/html"),
                                     WebCore::String("UTF-8"),
                                     WebCore::KURL());
  WebCore::ResourceRequest request(webkit_glue::GURLToKURL(script_url),
                                   WebCore::CString());
  web_frame->frame()->loader()->load(request, subst_data, false);

  // This document will be used as 'loading context' for the worker.
  loading_document_ = web_frame->frame()->document();

  worker_thread_ = WebCore::DedicatedWorkerThread::create(
      webkit_glue::WebURLToKURL(script_url),
      webkit_glue::WebStringToString(user_agent),
      webkit_glue::WebStringToString(source_code),
      *this,
      *this);

  // Worker initialization means a pending activity.
  reportPendingActivity(true);

  worker_thread_->start();
}

void WebWorkerImpl::terminateWorkerContext() {
  if (asked_to_terminate_)
    return;
  asked_to_terminate_ = true;

  if (worker_thread_)
    worker_thread_->stop();
}

void WebWorkerImpl::postMessageToWorkerContext(
    const WebString& message,
    const WebMessagePortChannelArray& webchannels) {

  WTF::OwnPtr<WebCore::MessagePortChannelArray> channels;
  if (webchannels.size()) {
    channels = new WebCore::MessagePortChannelArray(webchannels.size());
    for (size_t i = 0; i < webchannels.size(); ++i) {
      RefPtr<WebCore::PlatformMessagePortChannel> platform_channel =
          WebCore::PlatformMessagePortChannel::create(webchannels[i]);
      webchannels[i]->setClient(platform_channel.get());
      (*channels)[i] = WebCore::MessagePortChannel::create(platform_channel);
    }
  }

  worker_thread_->runLoop().postTask(WebCore::createCallbackTask(
      &PostMessageToWorkerContextTask,
      this,
      webkit_glue::WebStringToString(message),
      channels.release()));
}

void WebWorkerImpl::workerObjectDestroyed() {
  // Worker object in the renderer was destroyed, perhaps a result of GC.
  // For us, it's a signal to start terminating the WorkerContext too.
  // TODO(dimich): when 'kill a worker' html5 spec algorithm is implemented, it
  // should be used here instead of 'terminate a worker'.
  terminateWorkerContext();
}

void WebWorkerImpl::clientDestroyed() {
  client_ = NULL;
}

void WebWorkerImpl::DispatchTaskToMainThread(
    PassRefPtr<WebCore::ScriptExecutionContext::Task> task) {
  return WTF::callOnMainThread(InvokeTaskMethod, task.releaseRef());
}

void WebWorkerImpl::InvokeTaskMethod(void* param) {
  WebCore::ScriptExecutionContext::Task* task =
      static_cast<WebCore::ScriptExecutionContext::Task*>(param);
  task->performTask(NULL);
  task->deref();
}

// WorkerObjectProxy -----------------------------------------------------------

void WebWorkerImpl::postMessageToWorkerObject(
    WTF::PassRefPtr<WebCore::SerializedScriptValue> message,
    WTF::PassOwnPtr<WebCore::MessagePortChannelArray> channels) {
  DispatchTaskToMainThread(WebCore::createCallbackTask(
      &PostMessageTask,
      this,
      message->toString(),
      channels));
}

void WebWorkerImpl::PostMessageTask(
    WebCore::ScriptExecutionContext* context,
    WebWorkerImpl* this_ptr,
    WebCore::String message,
    WTF::PassOwnPtr<WebCore::MessagePortChannelArray> channels) {
  if (!this_ptr->client_)
    return;

  WebMessagePortChannelArray web_channels(
      channels.get() ? channels->size() : 0);
  for (size_t i = 0; i < web_channels.size(); ++i) {
    web_channels[i] = (*channels)[i]->channel()->webChannelRelease();
    web_channels[i]->setClient(0);
  }

  this_ptr->client_->postMessageToWorkerObject(
      webkit_glue::StringToWebString(message), web_channels);
}

void WebWorkerImpl::postExceptionToWorkerObject(
    const WebCore::String& error_message,
    int line_number,
    const WebCore::String& source_url) {
  DispatchTaskToMainThread(WebCore::createCallbackTask(
      &PostExceptionTask,
      this,
      error_message,
      line_number,
      source_url));
}

void WebWorkerImpl::PostExceptionTask(
      WebCore::ScriptExecutionContext* context,
      WebWorkerImpl* this_ptr,
      const WebCore::String& error_message,
      int line_number,
      const WebCore::String& source_url) {
  if (!this_ptr->client_)
    return;

  this_ptr->client_->postExceptionToWorkerObject(
      webkit_glue::StringToWebString(error_message),
      line_number,
      webkit_glue::StringToWebString(source_url));
}

void WebWorkerImpl::postConsoleMessageToWorkerObject(
    WebCore::MessageDestination destination,
    WebCore::MessageSource source,
    WebCore::MessageType type,
    WebCore::MessageLevel level,
    const WebCore::String& message,
    int line_number,
    const WebCore::String& source_url) {
  DispatchTaskToMainThread(WebCore::createCallbackTask(
      &PostConsoleMessageTask,
      this,
      static_cast<int>(destination),
      static_cast<int>(source),
      static_cast<int>(type),
      static_cast<int>(level),
      message,
      line_number,
      source_url));
}

void WebWorkerImpl::PostConsoleMessageTask(
    WebCore::ScriptExecutionContext* context,
    WebWorkerImpl* this_ptr,
    int destination,
    int source,
    int type,
    int level,
    const WebCore::String& message,
    int line_number,
    const WebCore::String& source_url) {
  if (!this_ptr->client_)
    return;

  this_ptr->client_->postConsoleMessageToWorkerObject(
      destination,
      source,
      type,
      level,
      webkit_glue::StringToWebString(message),
      line_number,
      webkit_glue::StringToWebString(source_url));
}

void WebWorkerImpl::confirmMessageFromWorkerObject(bool has_pending_activity) {
  DispatchTaskToMainThread(WebCore::createCallbackTask(
      &ConfirmMessageTask,
      this,
      has_pending_activity));
}

void WebWorkerImpl::ConfirmMessageTask(
    WebCore::ScriptExecutionContext* context,
    WebWorkerImpl* this_ptr,
    bool has_pending_activity) {
  if (!this_ptr->client_)
    return;

  this_ptr->client_->confirmMessageFromWorkerObject(has_pending_activity);
}

void WebWorkerImpl::reportPendingActivity(bool has_pending_activity) {
  DispatchTaskToMainThread(WebCore::createCallbackTask(
      &ReportPendingActivityTask,
      this,
      has_pending_activity));
}

void WebWorkerImpl::ReportPendingActivityTask(
    WebCore::ScriptExecutionContext* context,
    WebWorkerImpl* this_ptr,
    bool has_pending_activity) {
  if (!this_ptr->client_)
    return;

  this_ptr->client_->reportPendingActivity(has_pending_activity);
}

void WebWorkerImpl::workerContextDestroyed() {
  DispatchTaskToMainThread(WebCore::createCallbackTask(
      &WorkerContextDestroyedTask,
      this));
}

// WorkerLoaderProxy -----------------------------------------------------------

void WebWorkerImpl::postTaskToLoader(
    PassRefPtr<WebCore::ScriptExecutionContext::Task> task) {
  ASSERT(loading_document_->isDocument());
  loading_document_->postTask(task);
}

void WebWorkerImpl::postTaskForModeToWorkerContext(
    PassRefPtr<WebCore::ScriptExecutionContext::Task> task,
    const WebCore::String& mode) {
  worker_thread_->runLoop().postTaskForMode(task, mode);
}

void WebWorkerImpl::WorkerContextDestroyedTask(
    WebCore::ScriptExecutionContext* context,
    WebWorkerImpl* this_ptr) {
  if (this_ptr->client_)
    this_ptr->client_->workerContextDestroyed();

  // The lifetime of this proxy is controlled by the worker context.
  delete this_ptr;
}

#else

namespace WebKit {

WebWorker* WebWorker::create(WebWorkerClient* client) {
  return NULL;
}

}

#endif
