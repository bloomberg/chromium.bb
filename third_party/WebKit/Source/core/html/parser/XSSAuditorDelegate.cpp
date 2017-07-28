/*
 * Copyright (C) 2013 Google, Inc. All Rights Reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/html/parser/XSSAuditorDelegate.h"

#include "core/dom/Document.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/UseCounter.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/loader/DocumentLoader.h"
#include "core/loader/FrameLoader.h"
#include "core/loader/NavigationScheduler.h"
#include "core/loader/PingLoader.h"
#include "platform/json/JSONValues.h"
#include "platform/loader/fetch/ResourceError.h"
#include "platform/network/EncodedFormData.h"
#include "platform/weborigin/SecurityOrigin.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

String XSSInfo::BuildConsoleError() const {
  StringBuilder message;
  message.Append("The XSS Auditor ");
  message.Append(did_block_entire_page_ ? "blocked access to"
                                        : "refused to execute a script in");
  message.Append(" '");
  message.Append(original_url_);
  message.Append("' because ");
  message.Append(did_block_entire_page_ ? "the source code of a script"
                                        : "its source code");
  message.Append(" was found within the request.");

  if (did_send_xss_protection_header_)
    message.Append(
        " The server sent an 'X-XSS-Protection' header requesting this "
        "behavior.");
  else
    message.Append(
        " The auditor was enabled as the server did not send an "
        "'X-XSS-Protection' header.");

  return message.ToString();
}

bool XSSInfo::IsSafeToSendToAnotherThread() const {
  return original_url_.IsSafeToSendToAnotherThread();
}

XSSAuditorDelegate::XSSAuditorDelegate(Document* document)
    : document_(document), did_send_notifications_(false) {
  DCHECK(IsMainThread());
  DCHECK(document_);
}

DEFINE_TRACE(XSSAuditorDelegate) {
  visitor->Trace(document_);
}

RefPtr<EncodedFormData> XSSAuditorDelegate::GenerateViolationReport(
    const XSSInfo& xss_info) {
  DCHECK(IsMainThread());

  FrameLoader& frame_loader = document_->GetFrame()->Loader();
  String http_body;
  if (frame_loader.GetDocumentLoader()) {
    if (EncodedFormData* form_data =
            frame_loader.GetDocumentLoader()->OriginalRequest().HttpBody())
      http_body = form_data->FlattenToString();
  }

  std::unique_ptr<JSONObject> report_details = JSONObject::Create();
  report_details->SetString("request-url", xss_info.original_url_);
  report_details->SetString("request-body", http_body);

  std::unique_ptr<JSONObject> report_object = JSONObject::Create();
  report_object->SetObject("xss-report", std::move(report_details));

  return EncodedFormData::Create(report_object->ToJSONString().Utf8().data());
}

void XSSAuditorDelegate::DidBlockScript(const XSSInfo& xss_info) {
  DCHECK(IsMainThread());

  UseCounter::Count(document_, xss_info.did_block_entire_page_
                                   ? WebFeature::kXSSAuditorBlockedEntirePage
                                   : WebFeature::kXSSAuditorBlockedScript);

  document_->AddConsoleMessage(ConsoleMessage::Create(
      kJSMessageSource, kErrorMessageLevel, xss_info.BuildConsoleError()));

  LocalFrame* local_frame = document_->GetFrame();
  FrameLoader& frame_loader = local_frame->Loader();
  if (xss_info.did_block_entire_page_)
    frame_loader.StopAllLoaders();

  if (!did_send_notifications_ && local_frame->Client()) {
    did_send_notifications_ = true;

    local_frame->Client()->DidDetectXSS(document_->Url(),
                                        xss_info.did_block_entire_page_);

    if (!report_url_.IsEmpty())
      PingLoader::SendViolationReport(local_frame, report_url_,
                                      GenerateViolationReport(xss_info),
                                      PingLoader::kXSSAuditorViolationReport);
  }

  if (xss_info.did_block_entire_page_) {
    local_frame->GetNavigationScheduler().SchedulePageBlock(
        document_, ResourceError::BLOCKED_BY_XSS_AUDITOR);
  }
}

}  // namespace blink
