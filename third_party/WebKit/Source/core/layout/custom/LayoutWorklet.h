// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef LayoutWorklet_h
#define LayoutWorklet_h

#include "core/CoreExport.h"
#include "core/workers/Worklet.h"
#include "platform/heap/Handle.h"

namespace blink {

// Manages a layout worklet:
// https://drafts.css-houdini.org/css-layout-api/#dom-css-layoutworklet
//
// Provides access to web developer defined layout classes within multiple
// global scopes.
class CORE_EXPORT LayoutWorklet : public Worklet,
                                  public Supplement<LocalDOMWindow> {
  USING_GARBAGE_COLLECTED_MIXIN(LayoutWorklet);
  WTF_MAKE_NONCOPYABLE(LayoutWorklet);

 public:
  // At the moment, layout worklet allows at most two global scopes at any time.
  static const size_t kNumGlobalScopes;
  static LayoutWorklet* From(LocalDOMWindow&);
  static LayoutWorklet* Create(LocalFrame*);

  ~LayoutWorklet() override;

  void Trace(blink::Visitor*) override;

 protected:
  explicit LayoutWorklet(LocalFrame*);

 private:
  // Implements Worklet.
  bool NeedsToCreateGlobalScope() final;
  WorkletGlobalScopeProxy* CreateGlobalScope() final;

  static const char* SupplementName();
};

}  // namespace blink

#endif  // LayoutWorklet_h
