// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/events/event_ack_data.h"

#include "base/callback.h"
#include "base/guid.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/service_worker_context.h"

namespace extensions {

namespace {

// If successful, returns GUID of the external request.
std::unique_ptr<std::string> StartExternalRequestOnIO(
    content::ServiceWorkerContext* context,
    int64_t version_id,
    int event_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  auto request_uuid = std::make_unique<std::string>(base::GenerateGUID());
  if (!context->StartingExternalRequest(version_id, *request_uuid))
    return nullptr;
  return request_uuid;
}

void FinishExternalRequestOnIO(content::ServiceWorkerContext* context,
                               int64_t version_id,
                               const std::string& request_uuid,
                               base::OnceClosure failure_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::IO);
  if (!context->FinishedExternalRequest(version_id, request_uuid))
    std::move(failure_callback).Run();
}

}  // namespace

EventAckData::EventAckData() : weak_factory_(this) {}
EventAckData::~EventAckData() = default;

void EventAckData::IncrementInflightEvent(
    content::ServiceWorkerContext* context,
    int render_process_id,
    int64_t version_id,
    int event_id) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  // TODO(lazyboy): |context| isn't safe to access on IO thread inside
  // StartExternalRequestOnIO, fix this once https://crbug.com/875376 is fixed.
  content::BrowserThread::PostTaskAndReplyWithResult(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&StartExternalRequestOnIO, context, version_id, event_id),
      base::BindOnce(&EventAckData::DidStartExternalRequest,
                     weak_factory_.GetWeakPtr(), render_process_id, event_id));
}

void EventAckData::DecrementInflightEvent(
    content::ServiceWorkerContext* context,
    int render_process_id,
    int64_t version_id,
    int event_id,
    base::OnceClosure failure_callback) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

  auto request_info_iter = unacked_events_.find(event_id);
  if (request_info_iter == unacked_events_.end() ||
      request_info_iter->second.second != render_process_id) {
    std::move(failure_callback).Run();
    return;
  }

  std::string request_uuid = request_info_iter->second.first;
  unacked_events_.erase(request_info_iter);

  // TODO(lazyboy): |context| isn't safe to access on IO thread inside
  // FinishExternalRequestOnIO, fix this once https://crbug.com/875376 is fixed.
  content::BrowserThread::PostTask(
      content::BrowserThread::IO, FROM_HERE,
      base::BindOnce(&FinishExternalRequestOnIO, context, version_id,
                     request_uuid, std::move(failure_callback)));
}

void EventAckData::DidStartExternalRequest(
    int render_process_id,
    int event_id,
    std::unique_ptr<std::string> uuid_result) {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  if (!uuid_result)
    return;
  // TODO(lazyboy): Clean up |unacked_events_| if RenderProcessHost died before
  // it got a chance to ack |event_id|. This shouldn't happen in rare cases.
  auto insert_result = unacked_events_.insert(std::make_pair(
      event_id, std::make_pair(*uuid_result, render_process_id)));
  DCHECK(insert_result.second) << "EventAckData: Duplicate event_id.";
}

}  // namespace extensions
