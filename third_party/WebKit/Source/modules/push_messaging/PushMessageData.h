// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PushMessageData_h
#define PushMessageData_h

#include "bindings/core/v8/ScriptValue.h"
#include "modules/ModulesExport.h"
#include "platform/bindings/ScriptWrappable.h"
#include "platform/heap/Handle.h"
#include "platform/wtf/Vector.h"
#include "platform/wtf/text/WTFString.h"

namespace blink {

class ArrayBufferOrArrayBufferViewOrUSVString;
class Blob;
class DOMArrayBuffer;
class ExceptionState;
class ScriptState;

class MODULES_EXPORT PushMessageData final
    : public GarbageCollectedFinalized<PushMessageData>,
      public ScriptWrappable {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static PushMessageData* Create(const String& data);
  static PushMessageData* Create(
      const ArrayBufferOrArrayBufferViewOrUSVString& data);

  virtual ~PushMessageData();

  DOMArrayBuffer* arrayBuffer() const;
  Blob* blob() const;
  ScriptValue json(ScriptState*, ExceptionState&) const;
  String text() const;

  void Trace(blink::Visitor*);

 private:
  PushMessageData(const char* data, unsigned bytes_size);

  Vector<char> data_;
};

}  // namespace blink

#endif  // PushMessageData_h
