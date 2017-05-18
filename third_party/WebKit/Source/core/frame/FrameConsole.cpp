/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/frame/FrameConsole.h"

#include <memory>
#include "bindings/core/v8/SourceLocation.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/ConsoleMessage.h"
#include "core/inspector/ConsoleMessageStorage.h"
#include "core/inspector/MainThreadDebugger.h"
#include "core/loader/DocumentLoader.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "platform/loader/fetch/ResourceError.h"
#include "platform/loader/fetch/ResourceResponse.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

FrameConsole::FrameConsole(LocalFrame& frame) : frame_(&frame) {}

void FrameConsole::AddMessage(ConsoleMessage* console_message) {
  // PlzNavigate: when trying to commit a navigation, the SourceLocation
  // information for how the request was triggered has been stored in the
  // provisional DocumentLoader. Use it instead.
  DocumentLoader* provisional_loader =
      frame_->Loader().ProvisionalDocumentLoader();
  if (provisional_loader) {
    std::unique_ptr<SourceLocation> source_location =
        provisional_loader->CopySourceLocation();
    if (source_location) {
      console_message = ConsoleMessage::Create(
          console_message->Source(), console_message->Level(),
          console_message->Message(), std::move(source_location));
    }
  }

  if (AddMessageToStorage(console_message))
    ReportMessageToClient(console_message->Source(), console_message->Level(),
                          console_message->Message(),
                          console_message->Location());
}

bool FrameConsole::AddMessageToStorage(ConsoleMessage* console_message) {
  if (!frame_->GetDocument() || !frame_->GetPage())
    return false;
  frame_->GetPage()->GetConsoleMessageStorage().AddConsoleMessage(
      frame_->GetDocument(), console_message);
  return true;
}

void FrameConsole::ReportMessageToClient(MessageSource source,
                                         MessageLevel level,
                                         const String& message,
                                         SourceLocation* location) {
  if (source == kNetworkMessageSource)
    return;

  String url = location->Url();
  String stack_trace;
  if (source == kConsoleAPIMessageSource) {
    if (!frame_->GetPage())
      return;
    if (frame_->GetChromeClient().ShouldReportDetailedMessageForSource(*frame_,
                                                                       url)) {
      std::unique_ptr<SourceLocation> full_location =
          SourceLocation::CaptureWithFullStackTrace();
      if (!full_location->IsUnknown())
        stack_trace = full_location->ToString();
    }
  } else {
    if (!location->IsUnknown() &&
        frame_->GetChromeClient().ShouldReportDetailedMessageForSource(*frame_,
                                                                       url))
      stack_trace = location->ToString();
  }

  frame_->GetChromeClient().AddMessageToConsole(
      frame_, source, level, message, location->LineNumber(), url, stack_trace);
}

void FrameConsole::AddSingletonMessage(ConsoleMessage* console_message) {
  if (singleton_messages_.Contains(console_message->Message()))
    return;
  singleton_messages_.insert(console_message->Message());
  AddMessage(console_message);
}

void FrameConsole::ReportResourceResponseReceived(
    DocumentLoader* loader,
    unsigned long request_identifier,
    const ResourceResponse& response) {
  if (!loader)
    return;
  if (response.HttpStatusCode() < 400)
    return;
  if (response.WasFallbackRequiredByServiceWorker())
    return;
  String message =
      "Failed to load resource: the server responded with a status of " +
      String::Number(response.HttpStatusCode()) + " (" +
      response.HttpStatusText() + ')';
  ConsoleMessage* console_message = ConsoleMessage::CreateForRequest(
      kNetworkMessageSource, kErrorMessageLevel, message,
      response.Url().GetString(), request_identifier);
  AddMessage(console_message);
}

void FrameConsole::DidFailLoading(unsigned long request_identifier,
                                  const ResourceError& error) {
  if (error.IsCancellation())  // Report failures only.
    return;
  StringBuilder message;
  message.Append("Failed to load resource");
  if (!error.LocalizedDescription().IsEmpty()) {
    message.Append(": ");
    message.Append(error.LocalizedDescription());
  }
  AddMessageToStorage(ConsoleMessage::CreateForRequest(
      kNetworkMessageSource, kErrorMessageLevel, message.ToString(),
      error.FailingURL(), request_identifier));
}

DEFINE_TRACE(FrameConsole) {
  visitor->Trace(frame_);
}

}  // namespace blink
