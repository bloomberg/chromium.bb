// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptModule_h
#define ScriptModule_h

#include "core/CoreExport.h"
#include "platform/bindings/ScriptState.h"
#include "platform/bindings/SharedPersistent.h"
#include "platform/loader/fetch/AccessControlStatus.h"
#include "platform/loader/fetch/ResourceLoaderOptions.h"
#include "platform/wtf/Allocator.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/TextPosition.h"
#include "platform/wtf/text/WTFString.h"
#include "public/platform/WebURLRequest.h"
#include "v8/include/v8.h"

namespace blink {

class ExceptionState;

// Correspond to TC39 ModuleRecord.[[Status]]
// TODO(kouhei): Add URL after https://github.com/tc39/ecma262/pull/916 is
// merged.
using ScriptModuleState = v8::Module::Status;
const char* ScriptModuleStateToString(ScriptModuleState);

// CaptureEvalErrorFlag is used to implement "rethrow errors" parameter in
// run-a-module-script. When "rethrow errors" is to be set, use kCapture for
// ScriptModule::Evaluate()/Modulator::EvaluateModule(), and rethrow the
// returned exception (if any) in the caller of these methods. When "rethrow
// errors" is not to be set, use kReport, and Evaluate()/EvaluateModule()
// "report the error".
enum class CaptureEvalErrorFlag : bool { kReport, kCapture };

// ScriptModule wraps a handle to a v8::Module for use in core.
//
// Using ScriptModules needs a ScriptState and its scope to operate in. You
// should always provide the same ScriptState and not try to reuse ScriptModules
// across different contexts.
// Currently all ScriptModule users can easily access its context Modulator, so
// we use it to fill ScriptState in.
class CORE_EXPORT ScriptModule final {
  DISALLOW_NEW_EXCEPT_PLACEMENT_NEW();

 public:
  static ScriptModule Compile(v8::Isolate*,
                              const String& source,
                              const String& file_name,
                              AccessControlStatus,
                              WebURLRequest::FetchCredentialsMode,
                              const String& nonce,
                              ParserDisposition,
                              const TextPosition&,
                              ExceptionState&);

  // TODO(kouhei): Remove copy ctor
  ScriptModule();
  ~ScriptModule();

  // Returns exception, if any.
  ScriptValue Instantiate(ScriptState*);

  // Returns exception if CaptureEvalErrorFlag::kCapture is specified.
  // Otherwise, "report the error" to console.
  ScriptValue Evaluate(ScriptState*, CaptureEvalErrorFlag) const;
  static void ReportException(ScriptState*, v8::Local<v8::Value> exception);

  Vector<String> ModuleRequests(ScriptState*);
  Vector<TextPosition> ModuleRequestPositions(ScriptState*);
  ScriptModuleState Status(ScriptState*);

  // Returns record's [[ErrorCompletion]] field's [[Value]].
  // Should only be used via ModulatorImpl::GetError()
  v8::Local<v8::Value> ErrorCompletion(ScriptState*);

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

  ScriptModule(v8::Isolate*, v8::Local<v8::Module>);

  v8::Local<v8::Module> NewLocal(v8::Isolate* isolate) {
    return module_->NewLocal(isolate);
  }

  static v8::MaybeLocal<v8::Module> ResolveModuleCallback(
      v8::Local<v8::Context>,
      v8::Local<v8::String> specifier,
      v8::Local<v8::Module> referrer);

  RefPtr<SharedPersistent<v8::Module>> module_;
  unsigned identity_hash_ = 0;

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
    return HashTraits<RefPtr<blink::SharedPersistent<v8::Module>>>::
        IsDeletedValue(value.module_);
  }

  static void ConstructDeletedValue(blink::ScriptModule& slot,
                                    bool zero_value) {
    HashTraits<RefPtr<blink::SharedPersistent<v8::Module>>>::
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

#endif  // ScriptModule_h
