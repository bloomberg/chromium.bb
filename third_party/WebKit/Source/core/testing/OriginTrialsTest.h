// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef OriginTrialsTest_h
#define OriginTrialsTest_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"

namespace blink {

class ExceptionState;
class ScriptState;

// OriginTrialsTest is a very simple interface used for testing
// origin-trial-enabled features which are attached directly to interfaces at
// run-time.
class OriginTrialsTest : public GarbageCollectedFinalized<OriginTrialsTest>,
                         public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static OriginTrialsTest* Create() { return new OriginTrialsTest(); }
  virtual ~OriginTrialsTest() = default;

  bool normalAttribute() { return true; }
  static bool staticAttribute() { return true; }
  bool normalMethod() { return true; }
  static bool staticMethod() { return true; }
  static const unsigned short kConstant = 1;

  bool throwingAttribute(ScriptState*, ExceptionState&);

  bool unconditionalAttribute() { return true; }
  static bool staticUnconditionalAttribute() { return true; }
  bool unconditionalMethod() { return true; }
  static bool staticUnconditionalMethod() { return true; }
  static const unsigned short kUnconditionalConstant = 99;

  bool secureUnconditionalAttribute() { return true; }
  static bool secureStaticUnconditionalAttribute() { return true; }
  bool secureUnconditionalMethod() { return true; }
  static bool secureStaticUnconditionalMethod() { return true; }

  bool secureAttribute() { return true; }
  static bool secureStaticAttribute() { return true; }
  bool secureMethod() { return true; }
  static bool secureStaticMethod() { return true; }

  void Trace(blink::Visitor*);

 private:
  OriginTrialsTest() = default;
};

}  // namespace blink

#endif  // OriginTrialsTest_h
