// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BlobEvent_h
#define BlobEvent_h

#include "core/dom/DOMHighResTimeStamp.h"
#include "core/fileapi/Blob.h"
#include "modules/EventModules.h"
#include "modules/ModulesExport.h"
#include "platform/wtf/text/AtomicString.h"

namespace blink {

class Blob;
class BlobEventInit;

class MODULES_EXPORT BlobEvent final : public Event {
  DEFINE_WRAPPERTYPEINFO();

 public:
  ~BlobEvent() override {}

  static BlobEvent* Create(const AtomicString& type,
                           const BlobEventInit& initializer);
  static BlobEvent* Create(const AtomicString& type, Blob*, double);

  Blob* data() const { return blob_.Get(); }
  DOMHighResTimeStamp timecode() const { return timecode_; }

  // Event
  const AtomicString& InterfaceName() const final;

  virtual void Trace(blink::Visitor*);

 private:
  BlobEvent(const AtomicString& type, const BlobEventInit& initializer);
  BlobEvent(const AtomicString& type, Blob*, double);

  Member<Blob> blob_;
  DOMHighResTimeStamp timecode_;
};

}  // namespace blink

#endif  // BlobEvent_h
