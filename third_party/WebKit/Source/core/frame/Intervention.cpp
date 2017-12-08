// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/Intervention.h"

#include "core/frame/FrameConsole.h"
#include "core/frame/InterventionReport.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameClient.h"
#include "core/frame/Report.h"
#include "core/frame/ReportingContext.h"
#include "core/inspector/ConsoleMessage.h"
#include "public/platform/Platform.h"
#include "public/platform/reporting.mojom-blink.h"
#include "services/service_manager/public/cpp/connector.h"

namespace blink {

// static
void Intervention::GenerateReport(const LocalFrame* frame,
                                  const String& message) {
  if (!frame)
    return;

  // Send the message to the console.
  frame->Console().AddMessage(ConsoleMessage::Create(
      kInterventionMessageSource, kWarningMessageLevel, message));

  if (!frame->Client())
    return;

  Document* document = frame->GetDocument();

  // Construct the intervention report.
  InterventionReport* body =
      new InterventionReport(message, SourceLocation::Capture());
  Report* report =
      new Report("intervention", document->Url().GetString(), body);

  // Send the intervention report to any ReportingObservers.
  ReportingContext* reporting_context = ReportingContext::From(document);
  if (reporting_context->ObserverExists())
    reporting_context->QueueReport(report);

  // Send the intervention report to the Reporting API.
  mojom::blink::ReportingServiceProxyPtr service;
  Platform* platform = Platform::Current();
  platform->GetConnector()->BindInterface(platform->GetBrowserServiceName(),
                                          &service);
  service->QueueInterventionReport(document->Url(), message, body->sourceFile(),
                                   body->lineNumber(), body->columnNumber());
}

}  // namespace blink
