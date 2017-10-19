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
  static NavigatorBeacon& From(Navigator&);
  virtual ~NavigatorBeacon();

  static bool sendBeacon(ScriptState*,
                         Navigator&,
                         const String&,
                         const ArrayBufferViewOrBlobOrStringOrFormData&,
                         ExceptionState&);

  virtual void Trace(blink::Visitor*);

 private:
  explicit NavigatorBeacon(Navigator&);

  static const char* SupplementName();

  bool SendBeaconImpl(ScriptState*,
                      const String&,
                      const ArrayBufferViewOrBlobOrStringOrFormData&,
                      ExceptionState&);
  bool CanSendBeacon(ExecutionContext*, const KURL&, ExceptionState&);
  int MaxAllowance() const;
  void AddTransmittedBytes(size_t sent_bytes);

  size_t transmitted_bytes_;
};

}  // namespace blink

#endif  // NavigatorBeacon_h
