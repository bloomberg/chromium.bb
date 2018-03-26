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

#include "bindings/core/v8/V8ScriptRunner.h"

#include "bindings/core/v8/BindingSecurity.h"
#include "bindings/core/v8/ReferrerScriptInfo.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/ScriptStreamer.h"
#include "bindings/core/v8/V8BindingForCore.h"
#include "bindings/core/v8/V8GCController.h"
#include "bindings/core/v8/V8Initializer.h"
#include "build/build_config.h"
#include "core/dom/Document.h"
#include "core/dom/ExecutionContext.h"
#include "core/frame/LocalDOMWindow.h"
#include "core/frame/LocalFrame.h"
#include "core/inspector/InspectorTraceEvents.h"
#include "core/inspector/ThreadDebugger.h"
#include "core/probe/CoreProbes.h"
#include "platform/Histogram.h"
#include "platform/bindings/ScriptForbiddenScope.h"
#include "platform/bindings/V8ThrowException.h"
#include "platform/instrumentation/tracing/TraceEvent.h"
#include "platform/loader/fetch/CachedMetadata.h"
#include "platform/wtf/Assertions.h"
#include "platform/wtf/Optional.h"
#include "platform/wtf/Time.h"
#include "platform/wtf/text/TextEncoding.h"
#include "public/platform/Platform.h"
#include "public/web/WebSettings.h"

#if defined(OS_WIN)
#include <malloc.h>
#else
#include <alloca.h>
#endif

namespace blink {

namespace {

// Used to throw an exception before we exceed the C++ stack and crash.
// This limit was arrived at arbitrarily. crbug.com/449744
const int kMaxRecursionDepth = 44;

// In order to make sure all pending messages to be processed in
// v8::Function::Call, we don't call throwStackOverflowException
// directly. Instead, we create a v8::Function of
// throwStackOverflowException and call it.
void ThrowStackOverflowException(
    const v8::FunctionCallbackInfo<v8::Value>& info) {
  V8ThrowException::ThrowRangeError(info.GetIsolate(),
                                    "Maximum call stack size exceeded.");
}

void ThrowScriptForbiddenException(v8::Isolate* isolate) {
  V8ThrowException::ThrowError(isolate, "Script execution is forbidden.");
}

v8::Local<v8::Value> ThrowStackOverflowExceptionIfNeeded(v8::Isolate* isolate) {
  if (V8PerIsolateData::From(isolate)->IsHandlingRecursionLevelError()) {
    // If we are already handling a recursion level error, we should
    // not invoke v8::Function::Call.
    return v8::Undefined(isolate);
  }
  v8::MicrotasksScope microtasks_scope(
      isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
  V8PerIsolateData::From(isolate)->SetIsHandlingRecursionLevelError(true);

  ScriptForbiddenScope::AllowUserAgentScript allow_script;
  v8::Local<v8::Value> result =
      v8::Function::New(isolate->GetCurrentContext(),
                        ThrowStackOverflowException, v8::Local<v8::Value>(), 0,
                        v8::ConstructorBehavior::kThrow)
          .ToLocalChecked()
          ->Call(v8::Undefined(isolate), 0, nullptr);

  V8PerIsolateData::From(isolate)->SetIsHandlingRecursionLevelError(false);
  return result;
}

// Compile a script without any caching or compile options.
v8::MaybeLocal<v8::Script> CompileWithoutOptions(
    v8::ScriptCompiler::NoCacheReason no_cache_reason,
    v8::Isolate* isolate,
    v8::Local<v8::String> code,
    v8::ScriptOrigin origin,
    InspectorCompileScriptEvent::V8CacheResult*) {
  v8::ScriptCompiler::Source source(code, origin);
  return v8::ScriptCompiler::Compile(isolate->GetCurrentContext(), &source,
                                     v8::ScriptCompiler::kNoCompileOptions,
                                     no_cache_reason);
}

// Compile a script eagerly so that we can produce full code cache.
v8::MaybeLocal<v8::Script> CompileEager(
    v8::ScriptCompiler::NoCacheReason no_cache_reason,
    v8::Isolate* isolate,
    v8::Local<v8::String> code,
    v8::ScriptOrigin origin,
    InspectorCompileScriptEvent::V8CacheResult*) {
  v8::ScriptCompiler::Source source(code, origin);
  return v8::ScriptCompiler::Compile(isolate->GetCurrentContext(), &source,
                                     v8::ScriptCompiler::kEagerCompile,
                                     no_cache_reason);
}

// Compile a script, and consume a V8 cache that was generated previously.
static v8::MaybeLocal<v8::Script> CompileAndConsumeCache(
    SingleCachedMetadataHandler* cache_handler,
    scoped_refptr<CachedMetadata> cached_metadata,
    v8::ScriptCompiler::CompileOptions consume_options,
    v8::Isolate* isolate,
    v8::Local<v8::String> code,
    v8::ScriptOrigin origin,
    InspectorCompileScriptEvent::V8CacheResult* cache_result) {
  DCHECK(consume_options == v8::ScriptCompiler::kConsumeCodeCache);
  const char* data = cached_metadata->Data();
  int length = cached_metadata->size();
  v8::ScriptCompiler::CachedData* cached_data =
      new v8::ScriptCompiler::CachedData(
          reinterpret_cast<const uint8_t*>(data), length,
          v8::ScriptCompiler::CachedData::BufferNotOwned);
  v8::ScriptCompiler::Source source(code, origin, cached_data);
  v8::MaybeLocal<v8::Script> script = v8::ScriptCompiler::Compile(
      isolate->GetCurrentContext(), &source, consume_options);
  if (cached_data->rejected)
    cache_handler->ClearCachedMetadata(CachedMetadataHandler::kSendToPlatform);
  if (cache_result) {
    cache_result->consume_result = WTF::make_optional(
        InspectorCompileScriptEvent::V8CacheResult::ConsumeResult(
            consume_options, length, cached_data->rejected));
  }
  return script;
}

enum CacheTagKind { kCacheTagCode = 0, kCacheTagTimeStamp = 1, kCacheTagLast };

static const int kCacheTagKindSize = 1;

uint32_t CacheTag(CacheTagKind kind, const String& encoding) {
  static_assert((1 << kCacheTagKindSize) >= kCacheTagLast,
                "CacheTagLast must be large enough");

  static uint32_t v8_cache_data_version =
      v8::ScriptCompiler::CachedDataVersionTag() << kCacheTagKindSize;

  // A script can be (successfully) interpreted with different encodings,
  // depending on the page it appears in. The cache doesn't know anything
  // about encodings, but the cached data is specific to one encoding. If we
  // later load the script from the cache and interpret it with a different
  // encoding, the cached data is not valid for that encoding.
  return (v8_cache_data_version | kind) + StringHash::GetHash(encoding);
}

// Check previously stored timestamp.
bool IsResourceHotForCaching(SingleCachedMetadataHandler* cache_handler,
                             int hot_hours) {
  const double cache_within_seconds = hot_hours * 60 * 60;
  scoped_refptr<CachedMetadata> cached_metadata =
      cache_handler->GetCachedMetadata(
          V8ScriptRunner::TagForTimeStamp(cache_handler));
  if (!cached_metadata)
    return false;
  double time_stamp;
  const int size = sizeof(time_stamp);
  DCHECK_EQ(cached_metadata->size(), static_cast<unsigned long>(size));
  memcpy(&time_stamp, cached_metadata->Data(), size);
  return (WTF::CurrentTime() - time_stamp) < cache_within_seconds;
}

// Final compile call for a streamed compilation.
v8::MaybeLocal<v8::Script> PostStreamCompile(
    v8::ScriptCompiler::CompileOptions compile_options,
    SingleCachedMetadataHandler* cache_handler,
    ScriptStreamer* streamer,
    v8::Isolate* isolate,
    v8::Local<v8::String> code,
    v8::ScriptOrigin origin,
    InspectorCompileScriptEvent::V8CacheResult* cache_result) {
  v8::MaybeLocal<v8::Script> script = v8::ScriptCompiler::Compile(
      isolate->GetCurrentContext(), streamer->Source(), code, origin);
  return script;
}

typedef base::OnceCallback<v8::MaybeLocal<v8::Script>(
    v8::Isolate*,
    v8::Local<v8::String>,
    v8::ScriptOrigin,
    InspectorCompileScriptEvent::V8CacheResult*)>
    CompileFn;

// Select a compile function from any of the above depending on compile_options.
static CompileFn SelectCompileFunction(
    v8::ScriptCompiler::CompileOptions compile_options,
    SingleCachedMetadataHandler* cache_handler,
    v8::ScriptCompiler::NoCacheReason no_cache_reason) {
  switch (compile_options) {
    case v8::ScriptCompiler::kNoCompileOptions:
      return WTF::Bind(CompileWithoutOptions, no_cache_reason);
    case v8::ScriptCompiler::kEagerCompile:
      return WTF::Bind(CompileEager, no_cache_reason);
    case v8::ScriptCompiler::kConsumeCodeCache: {
      uint32_t code_cache_tag = V8ScriptRunner::TagForCodeCache(cache_handler);
      scoped_refptr<CachedMetadata> code_cache =
          cache_handler->GetCachedMetadata(code_cache_tag);
      DCHECK(code_cache);
      return WTF::Bind(CompileAndConsumeCache, WrapPersistent(cache_handler),
                       std::move(code_cache),
                       v8::ScriptCompiler::kConsumeCodeCache);
    }
    case v8::ScriptCompiler::kProduceCodeCache:
    case v8::ScriptCompiler::kProduceFullCodeCache:
    case v8::ScriptCompiler::kProduceParserCache:
    case v8::ScriptCompiler::kConsumeParserCache:
      NOTREACHED();
  }

  // All switch branches should return and we should never get here.
  // But some compilers aren't sure, hence this default.
  NOTREACHED();
  return WTF::Bind(CompileWithoutOptions, v8::ScriptCompiler::kNoCacheNoReason);
}

// Select a compile function for a streaming compile.
CompileFn SelectCompileFunction(
    v8::ScriptCompiler::CompileOptions compile_options,
    SingleCachedMetadataHandler* cache_handler,
    ScriptStreamer* streamer) {
  DCHECK(streamer->IsFinished());
  DCHECK(!streamer->StreamingSuppressed());

  // Streaming compilation may involve use of code cache.
  // TODO(leszeks): Add compile timer to streaming compilation.
  return WTF::Bind(PostStreamCompile, compile_options,
                   WrapPersistent(cache_handler), WrapPersistent(streamer));
}
}  // namespace

std::tuple<v8::ScriptCompiler::CompileOptions,
           V8ScriptRunner::ProduceCacheOptions,
           v8::ScriptCompiler::NoCacheReason>
V8ScriptRunner::GetCompileOptions(V8CacheOptions cache_options,
                                  const ScriptSourceCode& source) {
  static const int kMinimalCodeLength = 1024;
  static const int kHotHours = 72;
  v8::ScriptCompiler::NoCacheReason no_cache_reason;

  switch (source.SourceLocationType()) {
    case ScriptSourceLocationType::kInline:
      no_cache_reason = v8::ScriptCompiler::kNoCacheBecauseInlineScript;
      break;
    case ScriptSourceLocationType::kInlineInsideDocumentWrite:
      no_cache_reason = v8::ScriptCompiler::kNoCacheBecauseInDocumentWrite;
      break;
    case ScriptSourceLocationType::kExternalFile:
      no_cache_reason =
          v8::ScriptCompiler::kNoCacheBecauseResourceWithNoCacheHandler;
      break;
    // TODO(leszeks): Possibly differentiate between the other kinds of script
    // origin also.
    default:
      no_cache_reason = v8::ScriptCompiler::kNoCacheBecauseNoResource;
      break;
  }

  SingleCachedMetadataHandler* cache_handler = source.CacheHandler();
  if (!cache_handler) {
    return std::make_tuple(v8::ScriptCompiler::kNoCompileOptions,
                           ProduceCacheOptions::kNoProduceCache,
                           no_cache_reason);
  }

  if (cache_options == kV8CacheOptionsNone) {
    no_cache_reason = v8::ScriptCompiler::kNoCacheBecauseCachingDisabled;
    return std::make_tuple(v8::ScriptCompiler::kNoCompileOptions,
                           ProduceCacheOptions::kNoProduceCache,
                           no_cache_reason);
  }

  if (source.Source().length() < kMinimalCodeLength) {
    no_cache_reason = v8::ScriptCompiler::kNoCacheBecauseScriptTooSmall;
    return std::make_tuple(v8::ScriptCompiler::kNoCompileOptions,
                           ProduceCacheOptions::kNoProduceCache,
                           no_cache_reason);
  }

  uint32_t code_cache_tag = V8ScriptRunner::TagForCodeCache(cache_handler);
  scoped_refptr<CachedMetadata> code_cache =
      cache_handler->GetCachedMetadata(code_cache_tag);
  if (code_cache) {
    return std::make_tuple(v8::ScriptCompiler::kConsumeCodeCache,
                           ProduceCacheOptions::kNoProduceCache,
                           no_cache_reason);
  }

  switch (cache_options) {
    case kV8CacheOptionsDefault:
    case kV8CacheOptionsCode:
      if (!IsResourceHotForCaching(cache_handler, kHotHours)) {
        return std::make_tuple(v8::ScriptCompiler::kNoCompileOptions,
                               ProduceCacheOptions::kSetTimeStamp,
                               v8::ScriptCompiler::kNoCacheBecauseCacheTooCold);
      }
      return std::make_tuple(
          v8::ScriptCompiler::kNoCompileOptions,
          ProduceCacheOptions::kProduceCodeCache,
          v8::ScriptCompiler::kNoCacheBecauseDeferredProduceCodeCache);
    case kV8CacheOptionsCodeWithoutHeatCheck:
      return std::make_tuple(
          v8::ScriptCompiler::kNoCompileOptions,
          ProduceCacheOptions::kProduceCodeCache,
          v8::ScriptCompiler::kNoCacheBecauseDeferredProduceCodeCache);
    case kV8CacheOptionsFullCodeWithoutHeatCheck:
      return std::make_tuple(
          v8::ScriptCompiler::kEagerCompile,
          ProduceCacheOptions::kProduceCodeCache,
          v8::ScriptCompiler::kNoCacheBecauseDeferredProduceCodeCache);
    case kV8CacheOptionsNone:
      // Shouldn't happen, as this is handled above.
      // Case is here so that compiler can check all cases are handled.
      NOTREACHED();
      break;
  }

  // All switch branches should return and we should never get here.
  // But some compilers aren't sure, hence this default.
  NOTREACHED();
  return std::make_tuple(v8::ScriptCompiler::kNoCompileOptions,
                         ProduceCacheOptions::kNoProduceCache,
                         v8::ScriptCompiler::kNoCacheNoReason);
}

v8::MaybeLocal<v8::Script> V8ScriptRunner::CompileScript(
    ScriptState* script_state,
    const ScriptSourceCode& source,
    AccessControlStatus access_control_status,
    v8::ScriptCompiler::CompileOptions compile_options,
    v8::ScriptCompiler::NoCacheReason no_cache_reason,
    const ReferrerScriptInfo& referrer_info) {
  v8::Isolate* isolate = script_state->GetIsolate();
  if (source.Source().length() >= v8::String::kMaxLength) {
    V8ThrowException::ThrowError(isolate, "Source file too large.");
    return v8::Local<v8::Script>();
  }

  v8::Local<v8::String> code = V8String(isolate, source.Source());
  const String& file_name = source.Url();
  const TextPosition& script_start_position = source.StartPosition();
  ScriptStreamer* streamer = source.Streamer();
  SingleCachedMetadataHandler* cache_handler = source.CacheHandler();

  constexpr const char* kTraceEventCategoryGroup = "v8,devtools.timeline";
  TRACE_EVENT_BEGIN1(kTraceEventCategoryGroup, "v8.compile", "fileName",
                     file_name.Utf8());
  probe::V8Compile probe(ExecutionContext::From(script_state), file_name,
                         script_start_position.line_.ZeroBasedInt(),
                         script_start_position.column_.ZeroBasedInt());

  // NOTE: For compatibility with WebCore, ScriptSourceCode's line starts at
  // 1, whereas v8 starts at 0.
  v8::ScriptOrigin origin(
      V8String(isolate, file_name),
      v8::Integer::New(isolate, script_start_position.line_.ZeroBasedInt()),
      v8::Integer::New(isolate, script_start_position.column_.ZeroBasedInt()),
      v8::Boolean::New(isolate, access_control_status == kSharableCrossOrigin),
      v8::Local<v8::Integer>(), V8String(isolate, source.SourceMapUrl()),
      v8::Boolean::New(isolate, access_control_status == kOpaqueResource),
      v8::False(isolate),  // is_wasm
      v8::False(isolate),  // is_module
      referrer_info.ToV8HostDefinedOptions(isolate));

  CompileFn compile_fn =
      streamer ? SelectCompileFunction(compile_options, cache_handler, streamer)
               : SelectCompileFunction(compile_options, cache_handler,
                                       no_cache_reason);

  if (!*TRACE_EVENT_API_GET_CATEGORY_GROUP_ENABLED(kTraceEventCategoryGroup))
    return std::move(compile_fn).Run(isolate, code, origin, nullptr);

  InspectorCompileScriptEvent::V8CacheResult cache_result;
  v8::MaybeLocal<v8::Script> script =
      std::move(compile_fn).Run(isolate, code, origin, &cache_result);
  TRACE_EVENT_END1(
      kTraceEventCategoryGroup, "v8.compile", "data",
      InspectorCompileScriptEvent::Data(file_name, script_start_position,
                                        cache_result, streamer));
  return script;
}

v8::MaybeLocal<v8::Module> V8ScriptRunner::CompileModule(
    v8::Isolate* isolate,
    const String& source,
    const String& file_name,
    AccessControlStatus access_control_status,
    const TextPosition& start_position,
    const ReferrerScriptInfo& referrer_info) {
  TRACE_EVENT1("v8,devtools.timeline", "v8.compileModule", "fileName",
               file_name.Utf8());

  v8::ScriptOrigin origin(
      V8String(isolate, file_name),
      v8::Integer::New(isolate, start_position.line_.ZeroBasedInt()),
      v8::Integer::New(isolate, start_position.column_.ZeroBasedInt()),
      v8::Boolean::New(isolate, access_control_status == kSharableCrossOrigin),
      v8::Local<v8::Integer>(),    // script id
      v8::String::Empty(isolate),  // source_map_url
      v8::Boolean::New(isolate, access_control_status == kOpaqueResource),
      v8::False(isolate),  // is_wasm
      v8::True(isolate),   // is_module
      referrer_info.ToV8HostDefinedOptions(isolate));

  v8::ScriptCompiler::Source script_source(V8String(isolate, source), origin);
  return v8::ScriptCompiler::CompileModule(isolate, &script_source);
}

v8::MaybeLocal<v8::Value> V8ScriptRunner::RunCompiledScript(
    v8::Isolate* isolate,
    v8::Local<v8::Script> script,
    ExecutionContext* context) {
  DCHECK(!script.IsEmpty());
  ScopedFrameBlamer frame_blamer(
      context->IsDocument() ? ToDocument(context)->GetFrame() : nullptr);

  v8::Local<v8::Value> script_name =
      script->GetUnboundScript()->GetScriptName();
  TRACE_EVENT1("v8", "v8.run", "fileName",
               TRACE_STR_COPY(*v8::String::Utf8Value(isolate, script_name)));
  RuntimeCallStatsScopedTracer rcs_scoped_tracer(isolate);
  RUNTIME_CALL_TIMER_SCOPE(isolate, RuntimeCallStats::CounterId::kV8);

  if (v8::MicrotasksScope::GetCurrentDepth(isolate) >= kMaxRecursionDepth)
    return ThrowStackOverflowExceptionIfNeeded(isolate);

  CHECK(!context->IsIteratingOverObservers());

  // Run the script and keep track of the current recursion depth.
  v8::MaybeLocal<v8::Value> result;
  {
    if (ScriptForbiddenScope::IsScriptForbidden()) {
      ThrowScriptForbiddenException(isolate);
      return v8::MaybeLocal<v8::Value>();
    }
    v8::MicrotasksScope microtasks_scope(isolate,
                                         v8::MicrotasksScope::kRunMicrotasks);
    // ToCoreString here should be zero copy due to externalized string
    // unpacked.
    String script_url = ToCoreString(script_name->ToString());
    probe::ExecuteScript probe(context, script_url);
    result = script->Run(isolate->GetCurrentContext());
  }

  CHECK(!isolate->IsDead());
  return result;
}

void V8ScriptRunner::ProduceCache(
    v8::Isolate* isolate,
    v8::Local<v8::Script> script,
    const ScriptSourceCode& source,
    ProduceCacheOptions produce_cache_options,
    v8::ScriptCompiler::CompileOptions compile_options) {
  TRACE_EVENT0("v8", "v8.run");
  RuntimeCallStatsScopedTracer rcs_scoped_tracer(isolate);
  RUNTIME_CALL_TIMER_SCOPE(isolate, RuntimeCallStats::CounterId::kV8);

  switch (produce_cache_options) {
    case ProduceCacheOptions::kSetTimeStamp:
      V8ScriptRunner::SetCacheTimeStamp(source.CacheHandler());
      break;
    case ProduceCacheOptions::kProduceCodeCache: {
      constexpr const char* kTraceEventCategoryGroup = "v8,devtools.timeline";
      TRACE_EVENT_BEGIN1(kTraceEventCategoryGroup, "v8.compile", "fileName",
                         source.Url().GetString().Utf8());

      std::unique_ptr<v8::ScriptCompiler::CachedData> cached_data(
          v8::ScriptCompiler::CreateCodeCache(
              script->GetUnboundScript(), V8String(isolate, source.Source())));
      if (cached_data) {
        const char* data = reinterpret_cast<const char*>(cached_data->data);
        int length = cached_data->length;
        if (length > 1024) {
          // Omit histogram samples for small cache data to avoid outliers.
          int cache_size_ratio =
              static_cast<int>(100.0 * length / source.Source().length());
          DEFINE_THREAD_SAFE_STATIC_LOCAL(
              CustomCountHistogram, code_cache_size_histogram,
              ("V8.CodeCacheSizeRatio", 0, 10000, 50));
          code_cache_size_histogram.Count(cache_size_ratio);
        }
        SingleCachedMetadataHandler* cache_handler = source.CacheHandler();
        cache_handler->ClearCachedMetadata(
            CachedMetadataHandler::kCacheLocally);
        cache_handler->SetCachedMetadata(
            V8ScriptRunner::TagForCodeCache(cache_handler), data, length,
            CachedMetadataHandler::kSendToPlatform);
      }

      TRACE_EVENT_END1(
          kTraceEventCategoryGroup, "v8.compile", "data",
          InspectorCompileScriptEvent::Data(
              source.Url().GetString(), source.StartPosition(),
              InspectorCompileScriptEvent::V8CacheResult(
                  InspectorCompileScriptEvent::V8CacheResult::ProduceResult(
                      compile_options, cached_data ? cached_data->length : 0),
                  Optional<InspectorCompileScriptEvent::V8CacheResult::
                               ConsumeResult>()),
              source.Streamer()));
      break;
    }
    case ProduceCacheOptions::kNoProduceCache:
      break;
  }
}

v8::MaybeLocal<v8::Value> V8ScriptRunner::CompileAndRunInternalScript(
    v8::Isolate* isolate,
    ScriptState* script_state,
    const ScriptSourceCode& source_code) {
  DCHECK_EQ(isolate, script_state->GetIsolate());

  v8::ScriptCompiler::CompileOptions compile_options;
  V8ScriptRunner::ProduceCacheOptions produce_cache_options;
  v8::ScriptCompiler::NoCacheReason no_cache_reason;
  std::tie(compile_options, produce_cache_options, no_cache_reason) =
      V8ScriptRunner::GetCompileOptions(kV8CacheOptionsDefault, source_code);
  // Currently internal scripts don't have cache handlers. So we should not
  // produce cache for them.
  DCHECK_EQ(produce_cache_options, ProduceCacheOptions::kNoProduceCache);
  v8::Local<v8::Script> script;
  // Use default ScriptReferrerInfo here:
  // - nonce: empty for internal script, and
  // - parser_state: always "not parser inserted" for internal scripts.
  if (!V8ScriptRunner::CompileScript(script_state, source_code,
                                     kSharableCrossOrigin, compile_options,
                                     no_cache_reason, ReferrerScriptInfo())
           .ToLocal(&script))
    return v8::MaybeLocal<v8::Value>();

  TRACE_EVENT0("v8", "v8.run");
  RuntimeCallStatsScopedTracer rcs_scoped_tracer(isolate);
  RUNTIME_CALL_TIMER_SCOPE(isolate, RuntimeCallStats::CounterId::kV8);
  v8::MicrotasksScope microtasks_scope(
      isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::MaybeLocal<v8::Value> result = script->Run(isolate->GetCurrentContext());
  CHECK(!isolate->IsDead());
  return result;
}

v8::MaybeLocal<v8::Value> V8ScriptRunner::RunCompiledInternalScript(
    v8::Isolate* isolate,
    v8::Local<v8::Script> script) {
  TRACE_EVENT0("v8", "v8.run");
  RuntimeCallStatsScopedTracer rcs_scoped_tracer(isolate);
  RUNTIME_CALL_TIMER_SCOPE(isolate, RuntimeCallStats::CounterId::kV8);

  v8::MicrotasksScope microtasks_scope(
      isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::MaybeLocal<v8::Value> result = script->Run(isolate->GetCurrentContext());
  CHECK(!isolate->IsDead());
  return result;
}

v8::MaybeLocal<v8::Value> V8ScriptRunner::CallAsConstructor(
    v8::Isolate* isolate,
    v8::Local<v8::Object> constructor,
    ExecutionContext* context,
    int argc,
    v8::Local<v8::Value> argv[]) {
  TRACE_EVENT0("v8", "v8.callAsConstructor");
  RUNTIME_CALL_TIMER_SCOPE(isolate, RuntimeCallStats::CounterId::kV8);

  int depth = v8::MicrotasksScope::GetCurrentDepth(isolate);
  if (depth >= kMaxRecursionDepth)
    return v8::MaybeLocal<v8::Value>(
        ThrowStackOverflowExceptionIfNeeded(isolate));

  CHECK(!context->IsIteratingOverObservers());

  if (ScriptForbiddenScope::IsScriptForbidden()) {
    ThrowScriptForbiddenException(isolate);
    return v8::MaybeLocal<v8::Value>();
  }

  // TODO(dominicc): When inspector supports tracing object
  // invocation, change this to use v8::Object instead of
  // v8::Function. All callers use functions because
  // CustomElementRegistry#define's IDL signature is Function.
  CHECK(constructor->IsFunction());
  v8::Local<v8::Function> function = constructor.As<v8::Function>();

  v8::MicrotasksScope microtasks_scope(isolate,
                                       v8::MicrotasksScope::kRunMicrotasks);
  probe::CallFunction probe(context, function, depth);
  v8::MaybeLocal<v8::Value> result =
      constructor->CallAsConstructor(isolate->GetCurrentContext(), argc, argv);
  CHECK(!isolate->IsDead());
  return result;
}

v8::MaybeLocal<v8::Value> V8ScriptRunner::CallFunction(
    v8::Local<v8::Function> function,
    ExecutionContext* context,
    v8::Local<v8::Value> receiver,
    int argc,
    v8::Local<v8::Value> args[],
    v8::Isolate* isolate) {
  LocalFrame* frame =
      context->IsDocument() ? ToDocument(context)->GetFrame() : nullptr;
  ScopedFrameBlamer frame_blamer(frame);
  TRACE_EVENT0("v8", "v8.callFunction");
  RuntimeCallStatsScopedTracer rcs_scoped_tracer(isolate);
  RUNTIME_CALL_TIMER_SCOPE(isolate, RuntimeCallStats::CounterId::kV8);

  int depth = v8::MicrotasksScope::GetCurrentDepth(isolate);
  if (depth >= kMaxRecursionDepth)
    return v8::MaybeLocal<v8::Value>(
        ThrowStackOverflowExceptionIfNeeded(isolate));

  CHECK(!context->IsIteratingOverObservers());

  if (ScriptForbiddenScope::IsScriptForbidden()) {
    ThrowScriptForbiddenException(isolate);
    return v8::MaybeLocal<v8::Value>();
  }

  DCHECK(!frame || BindingSecurity::ShouldAllowAccessToFrame(
                       ToLocalDOMWindow(function->CreationContext()), frame,
                       BindingSecurity::ErrorReportOption::kDoNotReport));
  CHECK(!ThreadState::Current()->IsWrapperTracingForbidden());
  v8::MicrotasksScope microtasks_scope(isolate,
                                       v8::MicrotasksScope::kRunMicrotasks);
  probe::CallFunction probe(context, function, depth);
  v8::MaybeLocal<v8::Value> result =
      function->Call(isolate->GetCurrentContext(), receiver, argc, args);
  CHECK(!isolate->IsDead());

  return result;
}

v8::MaybeLocal<v8::Value> V8ScriptRunner::CallInternalFunction(
    v8::Isolate* isolate,
    v8::Local<v8::Function> function,
    v8::Local<v8::Value> receiver,
    int argc,
    v8::Local<v8::Value> args[]) {
  TRACE_EVENT0("v8", "v8.callFunction");
  RuntimeCallStatsScopedTracer rcs_scoped_tracer(isolate);
  RUNTIME_CALL_TIMER_SCOPE(isolate, RuntimeCallStats::CounterId::kV8);

  CHECK(!ThreadState::Current()->IsWrapperTracingForbidden());
  v8::MicrotasksScope microtasks_scope(
      isolate, v8::MicrotasksScope::kDoNotRunMicrotasks);
  v8::MaybeLocal<v8::Value> result =
      function->Call(isolate->GetCurrentContext(), receiver, argc, args);
  CHECK(!isolate->IsDead());
  return result;
}

v8::MaybeLocal<v8::Value> V8ScriptRunner::EvaluateModule(
    v8::Isolate* isolate,
    v8::Local<v8::Module> module,
    v8::Local<v8::Context> context) {
  TRACE_EVENT0("v8,devtools.timeline", "v8.evaluateModule");
  RUNTIME_CALL_TIMER_SCOPE(isolate, RuntimeCallStats::CounterId::kV8);
  v8::MicrotasksScope microtasks_scope(isolate,
                                       v8::MicrotasksScope::kRunMicrotasks);
  return module->Evaluate(context);
}

uint32_t V8ScriptRunner::TagForCodeCache(
    SingleCachedMetadataHandler* cache_handler) {
  return CacheTag(kCacheTagCode, cache_handler->Encoding());
}

uint32_t V8ScriptRunner::TagForTimeStamp(
    SingleCachedMetadataHandler* cache_handler) {
  return CacheTag(kCacheTagTimeStamp, cache_handler->Encoding());
}

// Store a timestamp to the cache as hint.
void V8ScriptRunner::SetCacheTimeStamp(
    SingleCachedMetadataHandler* cache_handler) {
  double now = WTF::CurrentTime();
  cache_handler->ClearCachedMetadata(CachedMetadataHandler::kCacheLocally);
  cache_handler->SetCachedMetadata(TagForTimeStamp(cache_handler),
                                   reinterpret_cast<char*>(&now), sizeof(now),
                                   CachedMetadataHandler::kSendToPlatform);
}

void V8ScriptRunner::ReportException(v8::Isolate* isolate,
                                     v8::Local<v8::Value> exception) {
  DCHECK(!exception.IsEmpty());

  // https://html.spec.whatwg.org/multipage/webappapis.html#report-the-error
  v8::Local<v8::Message> message =
      v8::Exception::CreateMessage(isolate, exception);
  if (IsMainThread())
    V8Initializer::MessageHandlerInMainThread(message, exception);
  else
    V8Initializer::MessageHandlerInWorker(message, exception);
}

v8::MaybeLocal<v8::Value> V8ScriptRunner::CallExtraHelper(
    ScriptState* script_state,
    const char* name,
    size_t num_args,
    v8::Local<v8::Value>* args) {
  v8::Isolate* isolate = script_state->GetIsolate();
  v8::Local<v8::Value> function_value;
  v8::Local<v8::Context> context = script_state->GetContext();
  if (!context->GetExtrasBindingObject()
           ->Get(context, V8AtomicString(isolate, name))
           .ToLocal(&function_value))
    return v8::MaybeLocal<v8::Value>();
  v8::Local<v8::Function> function = function_value.As<v8::Function>();
  return V8ScriptRunner::CallInternalFunction(
      isolate, function, v8::Undefined(isolate), num_args, args);
}

// static
scoped_refptr<CachedMetadata> V8ScriptRunner::GenerateFullCodeCache(
    ScriptState* script_state,
    const String& script_string,
    const String& file_name,
    const WTF::TextEncoding& encoding,
    OpaqueMode opaque_mode) {
  constexpr const char* kTraceEventCategoryGroup = "v8,devtools.timeline";
  TRACE_EVENT_BEGIN1(kTraceEventCategoryGroup, "v8.compile", "fileName",
                     file_name.Utf8());

  ScriptState::Scope scope(script_state);
  v8::Isolate* isolate = script_state->GetIsolate();
  ReferrerScriptInfo referrer_info;
  v8::ScriptOrigin origin(
      V8String(isolate, file_name),
      v8::Integer::New(isolate, 0),  // line_offset
      v8::Integer::New(isolate, 0),  // column_offset
      v8::Boolean::New(
          isolate,
          opaque_mode == OpaqueMode::kNotOpaque),  // is_shared_cross_origin
      v8::Local<v8::Integer>(),                    // script_id
      V8String(isolate, String("")),               // source_map_url
      v8::Boolean::New(isolate,
                       opaque_mode == OpaqueMode::kOpaque),  // is_opaque
      v8::False(isolate),                                    // is_wasm
      v8::False(isolate),                                    // is_module
      referrer_info.ToV8HostDefinedOptions(isolate));
  v8::Local<v8::String> code(V8String(isolate, script_string));
  v8::ScriptCompiler::Source source(code, origin);
  scoped_refptr<CachedMetadata> cached_metadata;
  std::unique_ptr<v8::ScriptCompiler::CachedData> cached_data;

  v8::Local<v8::UnboundScript> unbound_script;
  if (v8::ScriptCompiler::CompileUnboundScript(
          isolate, &source, v8::ScriptCompiler::kEagerCompile)
          .ToLocal(&unbound_script)) {
    cached_data.reset(
        v8::ScriptCompiler::CreateCodeCache(unbound_script, code));
    if (cached_data && cached_data->length) {
      cached_metadata = CachedMetadata::Create(
          CacheTag(kCacheTagCode, encoding.GetName()),
          reinterpret_cast<const char*>(cached_data->data),
          cached_data->length);
    }
  }

  TRACE_EVENT_END1(
      kTraceEventCategoryGroup, "v8.compile", "data",
      InspectorCompileScriptEvent::Data(
          file_name, TextPosition(),
          InspectorCompileScriptEvent::V8CacheResult(
              InspectorCompileScriptEvent::V8CacheResult::ProduceResult(
                  v8::ScriptCompiler::kEagerCompile,
                  cached_data ? cached_data->length : 0),
              Optional<
                  InspectorCompileScriptEvent::V8CacheResult::ConsumeResult>()),
          false));

  return cached_metadata;
}

STATIC_ASSERT_ENUM(WebSettings::kV8CacheOptionsDefault, kV8CacheOptionsDefault);
STATIC_ASSERT_ENUM(WebSettings::kV8CacheOptionsNone, kV8CacheOptionsNone);
STATIC_ASSERT_ENUM(WebSettings::kV8CacheOptionsCode, kV8CacheOptionsCode);

}  // namespace blink
