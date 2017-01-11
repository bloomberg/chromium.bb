// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NavigatorBeacon_h
#define NavigatorBeacon_h

#include "core/frame/Navigator.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class ScriptState;
class ExceptionState;
class KURL;
class ArrayBufferViewOrBlobOrStringOrFormData;

class NavigatorBeacon final : public GarbageCollectedFinalized<NavigatorBeacon>,
                              public Supplement<Navigator> {
  USING_GARBAGE_COLLECTED_MIXIN(NavigatorBeacon);

 public:
  static NavigatorBeacon& from(Navigator&);
  virtual ~NavigatorBeacon();

  static bool sendBeacon(ScriptState*,
                         Navigator&,
                         const String&,
                         const ArrayBufferViewOrBlobOrStringOrFormData&,
                         ExceptionState&);

  DECLARE_VIRTUAL_TRACE();

 private:
  explicit NavigatorBeacon(Navigator&);

  static const char* supplementName();

  bool sendBeaconImpl(ScriptState*,
                      const String&,
                      const ArrayBufferViewOrBlobOrStringOrFormData&,
                      ExceptionState&);
  bool canSendBeacon(ExecutionContext*, const KURL&, ExceptionState&);
  int maxAllowance() const;
  void addTransmittedBytes(int sentBytes);

  int m_transmittedBytes;
};

}  // namespace blink

#endif  // NavigatorBeacon_h
