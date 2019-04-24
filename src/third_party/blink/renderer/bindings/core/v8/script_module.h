// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_MODULE_H_
#define THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_MODULE_H_

#include "third_party/blink/renderer/bindings/core/v8/script_source_location_type.h"
#include "third_party/blink/renderer/bindings/core/v8/v8_code_cache.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/platform/bindings/shared_persistent.h"
#include "third_party/blink/renderer/platform/bindings/trace_wrapper_v8_reference.h"
#include "third_party/blink/renderer/platform/loader/fetch/cached_metadata_handler.h"
#include "third_party/blink/renderer/platform/wtf/allocator.h"
#include "third_party/blink/renderer/platform/wtf/text/text_position.h"
#include "third_party/blink/renderer/platform/wtf/text/wtf_string.h"
#include "third_party/blink/renderer/platform/wtf/vector.h"
#include "v8/include/v8.h"

namespace blink {

class ExceptionState;
class KURL;
class ScriptFetchOptions;
class ScriptState;
class ScriptValue;

// ScriptModuleProduceCacheData is a parameter object for
// ScriptModule::ProduceCache().
class CORE_EXPORT ScriptModuleProduceCacheData final
    : public GarbageCollectedFinalized<ScriptModuleProduceCacheData> {
 public:
  ScriptModuleProduceCacheData(v8::Isolate*,
                               SingleCachedMetadataHandler*,
                               V8CodeCache::ProduceCacheOptions,
                               v8::Local<v8::Module>);

  void Trace(blink::Visitor*);

  SingleCachedMetadataHandler* CacheHandler() const { return cache_handler_; }
  V8CodeCache::ProduceCacheOptions GetProduceCacheOptions() const {
    return produce_cache_options_;
  }
  v8::Local<v8::UnboundModuleScript> UnboundScript(v8::Isolate* isolate) const {
    return unbound_script_.NewLocal(isolate);
  }

 private:
  Member<SingleCachedMetadataHandler> cache_handler_;
  V8CodeCache::ProduceCacheOptions produce_cache_options_;

  // TODO(keishi): Visitor only defines a trace method for v8::Value so this
  // needs to be cast.
  GC_PLUGIN_IGNORE("757708")
  TraceWrapperV8Reference<v8::UnboundModuleScript> unbound_script_;
};

// ScriptModule wraps a handle to a v8::Module for use in core.
//
// Using ScriptModules needs a ScriptState and its scope to operate in. You
// should always provide the same ScriptState and not try to reuse ScriptModules
// across different contexts.
// Currently all ScriptModule users can easily access its context Modulator, so
// we use it to fill ScriptState in.
class CORE_EXPORT ScriptModule final {
  DISALLOW_NEW();

 public:
  static ScriptModule Compile(
      v8::Isolate*,
      const String& source,
      const KURL& source_url,
      const KURL& base_url,
      const ScriptFetchOptions&,
      const TextPosition&,
      ExceptionState&,
      V8CacheOptions = kV8CacheOptionsDefault,
      SingleCachedMetadataHandler* = nullptr,
      ScriptSourceLocationType source_location_type =
          ScriptSourceLocationType::kInternal,
      ScriptModuleProduceCacheData** out_produce_cache_data = nullptr);

  // TODO(kouhei): Remove copy ctor
  ScriptModule();
  ~ScriptModule();

  ScriptModule(v8::Isolate*, v8::Local<v8::Module>, const KURL&);

  // Returns exception, if any.
  ScriptValue Instantiate(ScriptState*);

  // Returns exception, if any.
  ScriptValue Evaluate(ScriptState*) const;
  static void ReportException(ScriptState*, v8::Local<v8::Value> exception);

  Vector<String> ModuleRequests(ScriptState*);
  Vector<TextPosition> ModuleRequestPositions(ScriptState*);

  inline bool operator==(const blink::ScriptModule& other) const;
  bool operator!=(const blink::ScriptModule& other) const {
    return !(*this == other);
  }

  bool IsNull() const { return !module_ || module_->IsEmpty(); }

  v8::Local<v8::Value> V8Namespace(v8::Isolate*);

 private:
  // ModuleScript instances store their record as
  // TraceWrapperV8Reference<v8::Module>, and reconstructs ScriptModule from it.
  friend class ModuleScript;

  v8::Local<v8::Module> NewLocal(v8::Isolate* isolate) {
    return module_->NewLocal(isolate);
  }

  static v8::MaybeLocal<v8::Module> ResolveModuleCallback(
      v8::Local<v8::Context>,
      v8::Local<v8::String> specifier,
      v8::Local<v8::Module> referrer);

  scoped_refptr<SharedPersistent<v8::Module>> module_;
  unsigned identity_hash_ = 0;
  String source_url_;

  friend struct ScriptModuleHash;
  friend struct WTF::HashTraits<blink::ScriptModule>;
};

struct ScriptModuleHash {
  STATIC_ONLY(ScriptModuleHash);

 public:
  static unsigned GetHash(const blink::ScriptModule& key) {
    return key.identity_hash_;
  }

  static bool Equal(const blink::ScriptModule& a,
                    const blink::ScriptModule& b) {
    return a == b;
  }

  static constexpr bool safe_to_compare_to_empty_or_deleted = true;
};

}  // namespace blink

namespace WTF {

template <>
struct DefaultHash<blink::ScriptModule> {
  using Hash = blink::ScriptModuleHash;
};

template <>
struct HashTraits<blink::ScriptModule>
    : public SimpleClassHashTraits<blink::ScriptModule> {
  static bool IsDeletedValue(const blink::ScriptModule& value) {
    return HashTraits<scoped_refptr<blink::SharedPersistent<v8::Module>>>::
        IsDeletedValue(value.module_);
  }

  static void ConstructDeletedValue(blink::ScriptModule& slot,
                                    bool zero_value) {
    HashTraits<scoped_refptr<blink::SharedPersistent<v8::Module>>>::
        ConstructDeletedValue(slot.module_, zero_value);
  }
};

}  // namespace WTF

namespace blink {

inline bool ScriptModule::operator==(const ScriptModule& other) const {
  if (HashTraits<ScriptModule>::IsDeletedValue(*this) &&
      HashTraits<ScriptModule>::IsDeletedValue(other))
    return true;

  if (HashTraits<ScriptModule>::IsDeletedValue(*this) ||
      HashTraits<ScriptModule>::IsDeletedValue(other))
    return false;

  blink::SharedPersistent<v8::Module>* left = module_.get();
  blink::SharedPersistent<v8::Module>* right = other.module_.get();
  if (left == right)
    return true;
  if (!left || !right)
    return false;
  return *left == *right;
}

}  // namespace blink

#endif  // THIRD_PARTY_BLINK_RENDERER_BINDINGS_CORE_V8_SCRIPT_MODULE_H_
