// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ScriptModule_h
#define ScriptModule_h

#include "bindings/core/v8/ScriptState.h"
#include "bindings/core/v8/SharedPersistent.h"
#include "core/CoreExport.h"
#include "v8/include/v8.h"
#include "wtf/Allocator.h"
#include "wtf/Vector.h"
#include "wtf/text/WTFString.h"

namespace blink {

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
  static ScriptModule compile(v8::Isolate*,
                              const String& source,
                              const String& fileName);

  // TODO(kouhei): Remove copy ctor
  ScriptModule() {}
  ScriptModule(WTF::HashTableDeletedValueType)
      : m_module(WTF::HashTableDeletedValue) {}
  ~ScriptModule();

  // Returns exception, if any.
  ScriptValue instantiate(ScriptState*);
  void evaluate(ScriptState*);

  Vector<String> moduleRequests(ScriptState*);

  bool isHashTableDeletedValue() const {
    return m_module.isHashTableDeletedValue();
  }

  bool operator==(const blink::ScriptModule& other) const {
    if (isHashTableDeletedValue() && other.isHashTableDeletedValue())
      return true;

    if (isHashTableDeletedValue() || other.isHashTableDeletedValue())
      return false;

    blink::SharedPersistent<v8::Module>* left = m_module.get();
    blink::SharedPersistent<v8::Module>* right = other.m_module.get();
    if (left == right)
      return true;
    if (!left || !right)
      return false;
    return *left == *right;
  }

  bool operator!=(const blink::ScriptModule& other) const {
    return !(*this == other);
  }

  bool isNull() const { return !m_module || m_module->isEmpty(); }

 private:
  ScriptModule(v8::Isolate*, v8::Local<v8::Module>);

  static v8::MaybeLocal<v8::Module> resolveModuleCallback(
      v8::Local<v8::Context>,
      v8::Local<v8::String> specifier,
      v8::Local<v8::Module> referrer);

  RefPtr<SharedPersistent<v8::Module>> m_module;
  unsigned m_identityHash = 0;

  friend struct ScriptModuleHash;
};

struct ScriptModuleHash {
  STATIC_ONLY(ScriptModuleHash);

 public:
  static unsigned hash(const blink::ScriptModule& key) {
    return key.m_identityHash;
  }

  static bool equal(const blink::ScriptModule& a,
                    const blink::ScriptModule& b) {
    return a == b;
  }

  static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

}  // namespace blink

namespace WTF {

template <>
struct DefaultHash<blink::ScriptModule> {
  using Hash = blink::ScriptModuleHash;
};

template <>
struct HashTraits<blink::ScriptModule>
    : public SimpleClassHashTraits<blink::ScriptModule> {};

}  // namespace WTF

#endif  // ScriptModule_h
