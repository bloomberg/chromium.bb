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
class V8TestCallback;
class V8TestEnumCallback;
class V8TestInterfaceCallback;
class V8TestReceiverObjectCallback;
class V8TestSequenceCallback;

class CallbackFunctionTest final
    : public GarbageCollected<CallbackFunctionTest>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  void Trace(blink::Visitor*);

  static CallbackFunctionTest* Create() { return new CallbackFunctionTest(); }

  String testCallback(V8TestCallback*,
                      const String&,
                      const String&,
                      ExceptionState&);
  String testNullableCallback(V8TestCallback*,
                              const String&,
                              const String&,
                              ExceptionState&);
  void testInterfaceCallback(V8TestInterfaceCallback*,
                             HTMLDivElement*,
                             ExceptionState&);
  void testReceiverObjectCallback(V8TestReceiverObjectCallback*,
                                  ExceptionState&);
  Vector<String> testSequenceCallback(V8TestSequenceCallback*,
                                      const Vector<int>& numbers,
                                      ExceptionState&);
  void testEnumCallback(V8TestEnumCallback*,
                        const String& enum_value,
                        ExceptionState&);
};

}  // namespace blink

#endif  // CallbackFunctionTest_h
