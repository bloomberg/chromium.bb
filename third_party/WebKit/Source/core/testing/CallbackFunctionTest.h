// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CallbackFunctionTest_h
#define CallbackFunctionTest_h

#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class ExceptionState;
class HTMLDivElement;
class TestCallback;
class TestInterfaceCallback;
class TestReceiverObjectCallback;
class TestSequenceCallback;

class CallbackFunctionTest final
    : public GarbageCollected<CallbackFunctionTest>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  DECLARE_TRACE();

  static CallbackFunctionTest* Create() { return new CallbackFunctionTest(); }

  String testCallback(TestCallback*,
                      const String&,
                      const String&,
                      ExceptionState&);
  String testNullableCallback(TestCallback*,
                              const String&,
                              const String&,
                              ExceptionState&);
  void testInterfaceCallback(TestInterfaceCallback*,
                             HTMLDivElement*,
                             ExceptionState&);
  void testReceiverObjectCallback(TestReceiverObjectCallback*, ExceptionState&);
  Vector<String> testSequenceCallback(TestSequenceCallback*,
                                      const Vector<int>& numbers,
                                      ExceptionState&);
};

}  // namespace blink

#endif  // CallbackFunctionTest_h
