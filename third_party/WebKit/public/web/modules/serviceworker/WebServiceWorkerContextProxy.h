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

#ifndef WebServiceWorkerContextProxy_h
#define WebServiceWorkerContextProxy_h

#include "public/platform/modules/serviceworker/WebServiceWorker.h"
#include "public/platform/modules/serviceworker/WebServiceWorkerRegistration.h"

#include <memory>

namespace blink {

class MessagePortChannel;
struct WebBackgroundFetchSettledFetch;
struct WebCanMakePaymentEventData;
class WebDataConsumerHandle;
class WebServiceWorkerRequest;
class WebString;
struct WebNotificationData;
struct WebPaymentRequestEventData;
struct WebServiceWorkerClientInfo;
struct WebServiceWorkerError;
class WebURLResponse;

// A proxy interface to talk to the worker's GlobalScope implementation.
// All methods of this class must be called on the worker thread.
class WebServiceWorkerContextProxy {
 public:
  virtual ~WebServiceWorkerContextProxy() {}

  virtual void SetRegistration(
      std::unique_ptr<WebServiceWorkerRegistration::Handle>) = 0;

  virtual void DispatchActivateEvent(int event_id) = 0;

  enum class BackgroundFetchState { kPending, kSucceeded, kFailed };

  virtual void DispatchBackgroundFetchAbortEvent(int event_id,
                                                 const WebString& tag) = 0;
  virtual void DispatchBackgroundFetchClickEvent(
      int event_id,
      const WebString& tag,
      BackgroundFetchState status) = 0;
  virtual void DispatchBackgroundFetchFailEvent(
      int event_id,
      const WebString& tag,
      const WebVector<WebBackgroundFetchSettledFetch>& fetches) = 0;
  virtual void DispatchBackgroundFetchedEvent(
      int event_id,
      const WebString& tag,
      const WebVector<WebBackgroundFetchSettledFetch>& fetches) = 0;
  virtual void DispatchExtendableMessageEvent(
      int event_id,
      const WebString& message,
      const WebSecurityOrigin& source_origin,
      WebVector<MessagePortChannel>,
      const WebServiceWorkerClientInfo&) = 0;
  virtual void DispatchExtendableMessageEvent(
      int event_id,
      const WebString& message,
      const WebSecurityOrigin& source_origin,
      WebVector<MessagePortChannel>,
      std::unique_ptr<WebServiceWorker::Handle>) = 0;
  virtual void DispatchInstallEvent(int event_id) = 0;
  virtual void DispatchFetchEvent(int fetch_event_id,
                                  const WebServiceWorkerRequest& web_request,
                                  bool navigation_preload_sent) = 0;
  virtual void DispatchForeignFetchEvent(
      int fetch_event_id,
      const WebServiceWorkerRequest& web_request) = 0;
  virtual void DispatchNotificationClickEvent(int event_id,
                                              const WebString& notification_id,
                                              const WebNotificationData&,
                                              int action_index,
                                              const WebString& reply) = 0;
  virtual void DispatchNotificationCloseEvent(int event_id,
                                              const WebString& notification_id,
                                              const WebNotificationData&) = 0;
  virtual void DispatchPushEvent(int event_id, const WebString& data) = 0;

  virtual bool HasFetchEventHandler() = 0;

  enum LastChanceOption { kIsNotLastChance, kIsLastChance };

  // Once the ServiceWorker has finished handling the sync event,
  // didHandleSyncEvent is called on the context client.
  virtual void DispatchSyncEvent(int sync_event_id,
                                 const WebString& tag,
                                 LastChanceOption) = 0;

  virtual void DispatchAbortPaymentEvent(int event_id) = 0;

  virtual void DispatchCanMakePaymentEvent(
      int event_id,
      const WebCanMakePaymentEventData&) = 0;

  virtual void DispatchPaymentRequestEvent(
      int event_id,
      const WebPaymentRequestEventData&) = 0;

  virtual void OnNavigationPreloadResponse(
      int fetch_event_id,
      std::unique_ptr<WebURLResponse>,
      std::unique_ptr<WebDataConsumerHandle>) = 0;
  virtual void OnNavigationPreloadError(
      int fetch_event_id,
      std::unique_ptr<WebServiceWorkerError>) = 0;
  virtual void OnNavigationPreloadComplete(int fetch_event_id,
                                           double completion_time,
                                           int64_t encoded_data_length,
                                           int64_t encoded_body_length,
                                           int64_t decoded_body_length) = 0;
};

}  // namespace blink

#endif  // WebServiceWorkerContextProxy_h
