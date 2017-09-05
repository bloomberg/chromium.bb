// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/SourceLocation.h"

#include <memory>
#include "bindings/core/v8/V8BindingForCore.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/ScriptableDocumentParser.h"
#include "core/html/HTMLFrameOwnerElement.h"
#include "core/inspector/ThreadDebugger.h"
#include "core/inspector/V8InspectorString.h"
#include "platform/ScriptForbiddenScope.h"
#include "platform/bindings/V8BindingMacros.h"
#include "platform/bindings/V8PerIsolateData.h"
#include "platform/instrumentation/tracing/TracedValue.h"
#include "platform/wtf/PtrUtil.h"

namespace blink {

namespace {

std::unique_ptr<v8_inspector::V8StackTrace> CaptureStackTrace(bool full) {
  v8::Isolate* isolate = v8::Isolate::GetCurrent();
  ThreadDebugger* debugger = ThreadDebugger::From(isolate);
  if (!debugger || !isolate->InContext())
    return nullptr;
  ScriptForbiddenScope::AllowUserAgentScript allow_scripting;
  return debugger->GetV8Inspector()->captureStackTrace(full);
}
}

// static
std::unique_ptr<SourceLocation> SourceLocation::Capture(
    const String& url,
    unsigned line_number,
    unsigned column_number) {
  std::unique_ptr<v8_inspector::V8StackTrace> stack_trace =
      CaptureStackTrace(false);
  if (stack_trace && !stack_trace->isEmpty())
    return SourceLocation::CreateFromNonEmptyV8StackTrace(
        std::move(stack_trace), 0);
  return SourceLocation::Create(url, line_number, column_number,
                                std::move(stack_trace));
}

// static
std::unique_ptr<SourceLocation> SourceLocation::Capture(
    ExecutionContext* execution_context) {
  std::unique_ptr<v8_inspector::V8StackTrace> stack_trace =
      CaptureStackTrace(false);
  if (stack_trace && !stack_trace->isEmpty())
    return SourceLocation::CreateFromNonEmptyV8StackTrace(
        std::move(stack_trace), 0);

  Document* document = execution_context && execution_context->IsDocument()
                           ? ToDocument(execution_context)
                           : nullptr;
  if (document) {
    unsigned line_number = 0;
    if (document->GetScriptableDocumentParser() &&
        !document->IsInDocumentWrite()) {
      if (document->GetScriptableDocumentParser()->IsParsingAtLineNumber())
        line_number =
            document->GetScriptableDocumentParser()->LineNumber().OneBasedInt();
    }
    return SourceLocation::Create(document->Url().GetString(), line_number, 0,
                                  std::move(stack_trace));
  }

  return SourceLocation::Create(
      execution_context ? execution_context->Url().GetString() : String(), 0, 0,
      std::move(stack_trace));
}

// static
std::unique_ptr<SourceLocation> SourceLocation::FromMessage(
    v8::Isolate* isolate,
    v8::Local<v8::Message> message,
    ExecutionContext* execution_context) {
  v8::Local<v8::StackTrace> stack = message->GetStackTrace();
  std::unique_ptr<v8_inspector::V8StackTrace> stack_trace = nullptr;
  ThreadDebugger* debugger = ThreadDebugger::From(isolate);
  if (debugger)
    stack_trace = debugger->GetV8Inspector()->createStackTrace(stack);

  int script_id = message->GetScriptOrigin().ScriptID()->Value();
  if (!stack.IsEmpty() && stack->GetFrameCount() > 0) {
    int top_script_id = stack->GetFrame(0)->GetScriptId();
    if (top_script_id == script_id)
      script_id = 0;
  }

  int line_number = 0;
  int column_number = 0;
  if (message->GetLineNumber(isolate->GetCurrentContext()).To(&line_number) &&
      message->GetStartColumn(isolate->GetCurrentContext()).To(&column_number))
    ++column_number;

  if ((!script_id || !line_number) && stack_trace && !stack_trace->isEmpty())
    return SourceLocation::CreateFromNonEmptyV8StackTrace(
        std::move(stack_trace), 0);

  String url = ToCoreStringWithUndefinedOrNullCheck(
      message->GetScriptOrigin().ResourceName());
  if (url.IsEmpty())
    url = execution_context->Url();
  return SourceLocation::Create(url, line_number, column_number,
                                std::move(stack_trace), script_id);
}

// static
std::unique_ptr<SourceLocation> SourceLocation::Create(
    const String& url,
    unsigned line_number,
    unsigned column_number,
    std::unique_ptr<v8_inspector::V8StackTrace> stack_trace,
    int script_id) {
  return WTF::WrapUnique(new SourceLocation(url, line_number, column_number,
                                            std::move(stack_trace), script_id));
}

// static
std::unique_ptr<SourceLocation> SourceLocation::CreateFromNonEmptyV8StackTrace(
    std::unique_ptr<v8_inspector::V8StackTrace> stack_trace,
    int script_id) {
  // Retrieve the data before passing the ownership to SourceLocation.
  String url = ToCoreString(stack_trace->topSourceURL());
  unsigned line_number = stack_trace->topLineNumber();
  unsigned column_number = stack_trace->topColumnNumber();
  return WTF::WrapUnique(new SourceLocation(url, line_number, column_number,
                                            std::move(stack_trace), script_id));
}

// static
std::unique_ptr<SourceLocation> SourceLocation::FromFunction(
    v8::Local<v8::Function> function) {
  if (!function.IsEmpty())
    return SourceLocation::Create(
        ToCoreStringWithUndefinedOrNullCheck(
            function->GetScriptOrigin().ResourceName()),
        function->GetScriptLineNumber() + 1,
        function->GetScriptColumnNumber() + 1, nullptr, function->ScriptId());
  return SourceLocation::Create(String(), 0, 0, nullptr, 0);
}

// static
std::unique_ptr<SourceLocation> SourceLocation::CaptureWithFullStackTrace() {
  std::unique_ptr<v8_inspector::V8StackTrace> stack_trace =
      CaptureStackTrace(true);
  if (stack_trace && !stack_trace->isEmpty())
    return SourceLocation::CreateFromNonEmptyV8StackTrace(
        std::move(stack_trace), 0);
  return SourceLocation::Create(String(), 0, 0, nullptr, 0);
}

SourceLocation::SourceLocation(
    const String& url,
    unsigned line_number,
    unsigned column_number,
    std::unique_ptr<v8_inspector::V8StackTrace> stack_trace,
    int script_id)
    : url_(url),
      line_number_(line_number),
      column_number_(column_number),
      stack_trace_(std::move(stack_trace)),
      script_id_(script_id) {}

SourceLocation::~SourceLocation() {}

void SourceLocation::ToTracedValue(TracedValue* value, const char* name) const {
  if (!stack_trace_ || stack_trace_->isEmpty())
    return;
  value->BeginArray(name);
  value->BeginDictionary();
  value->SetString("functionName",
                   ToCoreString(stack_trace_->topFunctionName()));
  value->SetString("scriptId", ToCoreString(stack_trace_->topScriptId()));
  value->SetString("url", ToCoreString(stack_trace_->topSourceURL()));
  value->SetInteger("lineNumber", stack_trace_->topLineNumber());
  value->SetInteger("columnNumber", stack_trace_->topColumnNumber());
  value->EndDictionary();
  value->EndArray();
}

std::unique_ptr<SourceLocation> SourceLocation::Clone() const {
  return WTF::WrapUnique(new SourceLocation(
      url_.IsolatedCopy(), line_number_, column_number_,
      stack_trace_ ? stack_trace_->clone() : nullptr, script_id_));
}

std::unique_ptr<v8_inspector::protocol::Runtime::API::StackTrace>
SourceLocation::BuildInspectorObject() const {
  return stack_trace_ ? stack_trace_->buildInspectorObject() : nullptr;
}

String SourceLocation::ToString() const {
  if (!stack_trace_)
    return String();
  return ToCoreString(stack_trace_->toString());
}

}  // namespace blink
