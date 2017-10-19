// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef HTMLIFrameElementPayments_h
#define HTMLIFrameElementPayments_h

#include "core/html/HTMLIFrameElement.h"
#include "platform/Supplementable.h"
#include "platform/heap/Handle.h"

namespace blink {

class HTMLIFrameElement;
class QualifiedName;

class HTMLIFrameElementPayments final
    : public GarbageCollected<HTMLIFrameElementPayments>,
      public Supplement<HTMLIFrameElement> {
  USING_GARBAGE_COLLECTED_MIXIN(HTMLIFrameElementPayments);

 public:
  static bool FastHasAttribute(const QualifiedName&, const HTMLIFrameElement&);
  static void SetBooleanAttribute(const QualifiedName&,
                                  HTMLIFrameElement&,
                                  bool);
  static HTMLIFrameElementPayments& From(HTMLIFrameElement&);
  static bool AllowPaymentRequest(HTMLIFrameElement&);

  virtual void Trace(blink::Visitor*);

 private:
  HTMLIFrameElementPayments();

  static const char* SupplementName();
};

}  // namespace blink

#endif  // HTMLIFrameElementPayments_h
