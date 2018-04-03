// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ContextFeatureSettings_h
#define ContextFeatureSettings_h

#include "core/CoreExport.h"
#include "core/execution_context/ExecutionContext.h"
#include "platform/Supplementable.h"

namespace blink {

class ExecutionContext;

// ContextFeatureSettings attaches to an ExecutionContext some basic flags
// pertaining to the enabled/disabled state of any platform API features which
// are gated behind a ContextEnabled extended attribute in IDL.
class CORE_EXPORT ContextFeatureSettings final
    : public GarbageCollectedFinalized<ContextFeatureSettings>,
      public Supplement<ExecutionContext> {
  USING_GARBAGE_COLLECTED_MIXIN(ContextFeatureSettings)
 public:
  static const char kSupplementName[];

  enum class CreationMode { kCreateIfNotExists, kDontCreateIfNotExists };

  explicit ContextFeatureSettings(ExecutionContext&);

  // Returns the ContextFeatureSettings for an ExecutionContext. If one does not
  // already exist for the given context, one is created.
  static ContextFeatureSettings* From(ExecutionContext*, CreationMode);

  // ContextEnabled=MojoJS feature
  void enableMojoJS(bool enable) { enable_mojo_js_ = enable; }
  bool isMojoJSEnabled() const { return enable_mojo_js_; }

  virtual void Trace(blink::Visitor*);

 private:
  bool enable_mojo_js_ = false;
};

}  // namespace blink

#endif  // ContextFeatureSettings_h
