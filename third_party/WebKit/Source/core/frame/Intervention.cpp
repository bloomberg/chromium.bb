// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/frame/Intervention.h"

#include "core/frame/FrameConsole.h"
#include "core/frame/InterventionReport.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/Report.h"
#include "core/frame/ReportingContext.h"
#include "core/inspector/ConsoleMessage.h"

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

  // Send the intervention report to any ReportingObservers.
  Document* document = frame->GetDocument();
  ReportingContext* reporting_context = ReportingContext::From(document);
  if (!reporting_context->ObserverExists())
    return;

  ReportBody* body = new InterventionReport(message, SourceLocation::Capture());
  Report* report =
      new Report("intervention", document->Url().GetString(), body);
  reporting_context->QueueReport(report);
}

}  // namespace blink
