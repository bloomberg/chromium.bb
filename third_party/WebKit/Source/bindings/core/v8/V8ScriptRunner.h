/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef V8ScriptRunner_h
#define V8ScriptRunner_h

#include <stdint.h>

#include "bindings/core/v8/ReferrerScriptInfo.h"
#include "bindings/core/v8/ScriptSourceLocationType.h"
#include "bindings/core/v8/ScriptValue.h"
#include "bindings/core/v8/V8CacheOptions.h"
#include "core/CoreExport.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/V8BindingMacros.h"
#include "platform/loader/fetch/AccessControlStatus.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/TextPosition.h"
#include "platform/wtf/text/WTFString.h"
#include "v8/include/v8.h"

namespace WTF {
class TextEncoding;
}  // namespace WTF

namespace blink {

class CachedMetadata;
class CachedMetadataHandler;
class ExecutionContext;
class ScriptSourceCode;

class CORE_EXPORT V8ScriptRunner final {
  STATIC_ONLY(V8ScriptRunner);

 public:
  enum class OpaqueMode {
    kOpaque,
    kNotOpaque,
  };

  // For the following methods, the caller sites have to hold
  // a HandleScope and a ContextScope.
  // CachedMetadataHandler is set when metadata caching is supported.
  static v8::MaybeLocal<v8::Script> CompileScript(ScriptState*,
                                                  const ScriptSourceCode&,
                                                  AccessControlStatus,
                                                  V8CacheOptions,
                                                  const ReferrerScriptInfo&);
  static v8::MaybeLocal<v8::Module> CompileModule(v8::Isolate*,
                                                  const String& source,
                                                  const String& file_name,
                                                  AccessControlStatus,
                                                  const TextPosition&,
                                                  const ReferrerScriptInfo&);
  static v8::MaybeLocal<v8::Value> RunCompiledScript(v8::Isolate*,
                                                     v8::Local<v8::Script>,
                                                     ExecutionContext*);
  static v8::MaybeLocal<v8::Value> CompileAndRunInternalScript(
      v8::Isolate*,
      ScriptState*,
      const ScriptSourceCode&);
  static v8::MaybeLocal<v8::Value> RunCompiledInternalScript(
      v8::Isolate*,
      v8::Local<v8::Script>);
  static v8::MaybeLocal<v8::Value> CallAsConstructor(
      v8::Isolate*,
      v8::Local<v8::Object>,
      ExecutionContext*,
      int argc = 0,
      v8::Local<v8::Value> argv[] = nullptr);
  static v8::MaybeLocal<v8::Value> CallInternalFunction(
      v8::Isolate*,
      v8::Local<v8::Function>,
      v8::Local<v8::Value> receiver,
      int argc,
      v8::Local<v8::Value> info[]);
  static v8::MaybeLocal<v8::Value> CallFunction(v8::Local<v8::Function>,
                                                ExecutionContext*,
                                                v8::Local<v8::Value> receiver,
                                                int argc,
                                                v8::Local<v8::Value> info[],
                                                v8::Isolate*);
  static v8::MaybeLocal<v8::Value> EvaluateModule(v8::Isolate*,
                                                  v8::Local<v8::Module>,
                                                  v8::Local<v8::Context>);

  // Only to be used from ScriptModule::ReportException().
  static void ReportExceptionForModule(v8::Isolate*,
                                       v8::Local<v8::Value> exception,
                                       const String& file_name,
                                       const TextPosition&);

  static uint32_t TagForParserCache(CachedMetadataHandler*);
  static uint32_t TagForCodeCache(CachedMetadataHandler*);
  static void SetCacheTimeStamp(CachedMetadataHandler*);

  // Utilities for calling functions added to the V8 extras binding object.

  template <size_t N>
  static v8::MaybeLocal<v8::Value> CallExtra(ScriptState* script_state,
                                             const char* name,
                                             v8::Local<v8::Value> (&args)[N]) {
    return CallExtraHelper(script_state, name, N, args);
  }

  template <size_t N>
  static v8::Local<v8::Value> CallExtraOrCrash(
      ScriptState* script_state,
      const char* name,
      v8::Local<v8::Value> (&args)[N]) {
    return CallExtraHelper(script_state, name, N, args).ToLocalChecked();
  }

  // Reports an exception to the message handler, as if it were an uncaught
  // exception. Can only be called on the main thread.
  //
  // TODO(adamk): This should live on V8ThrowException, but it depends on
  // V8Initializer and so can't trivially move to platform/bindings.
  static void ReportException(v8::Isolate*, v8::Local<v8::Value> exception);

  static scoped_refptr<CachedMetadata> GenerateFullCodeCache(
      ScriptState*,
      const String& script_string,
      const String& file_name,
      const WTF::TextEncoding&,
      OpaqueMode);

 private:
  static v8::MaybeLocal<v8::Value> CallExtraHelper(ScriptState*,
                                                   const char* name,
                                                   size_t num_args,
                                                   v8::Local<v8::Value>* args);
};

}  // namespace blink

#endif  // V8ScriptRunner_h
