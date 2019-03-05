// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/public/web/web_console_message.h"

#include "third_party/blink/renderer/bindings/core/v8/source_location.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_binding_for_core.h"
#include "third_party/blink/renderer/core/dom/document.h"
#include "third_party/blink/renderer/core/dom/dom_node_ids.h"
#include "third_party/blink/renderer/core/execution_context/execution_context.h"
#include "third_party/blink/renderer/core/inspector/console_message.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {

void WebConsoleMessage::LogWebConsoleMessage(v8::Local<v8::Context> context,
                                             const WebConsoleMessage& message) {
  MessageLevel web_core_message_level = kInfoMessageLevel;
  switch (message.level) {
    case mojom::ConsoleMessageLevel::kVerbose:
      web_core_message_level = kVerboseMessageLevel;
      break;
    case mojom::ConsoleMessageLevel::kInfo:
      web_core_message_level = kInfoMessageLevel;
      break;
    case mojom::ConsoleMessageLevel::kWarning:
      web_core_message_level = kWarningMessageLevel;
      break;
    case mojom::ConsoleMessageLevel::kError:
      web_core_message_level = kErrorMessageLevel;
      break;
  }

  MessageSource message_source = message.nodes.empty()
                                     ? kOtherMessageSource
                                     : kRecommendationMessageSource;

  auto* execution_context = ToExecutionContext(context);
  if (!execution_context)  // Can happen in unittests.
    return;

  ConsoleMessage* console_message = ConsoleMessage::Create(
      message_source, web_core_message_level, message.text,
      SourceLocation::Create(message.url, message.line_number,
                             message.column_number, nullptr));

  if (auto* document = DynamicTo<Document>(execution_context)) {
    if (auto* frame = document->GetFrame()) {
      Vector<DOMNodeId> nodes;
      for (const WebNode& web_node : message.nodes)
        nodes.push_back(DOMNodeIds::IdForNode(&(*web_node)));
      console_message->SetNodes(frame, std::move(nodes));
    }
  }

  execution_context->AddConsoleMessage(console_message);
}

}  // namespace blink
