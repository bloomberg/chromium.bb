// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorIOAgent_h
#define InspectorIOAgent_h

#include "core/CoreExport.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/protocol/IO.h"
#include "platform/wtf/Noncopyable.h"
#include "platform/wtf/text/WTFString.h"

namespace v8 {
class Isolate;
}

namespace v8_inspector {
class V8InspectorSession;
}

namespace blink {

class CORE_EXPORT InspectorIOAgent final
    : public InspectorBaseAgent<protocol::IO::Metainfo> {
  WTF_MAKE_NONCOPYABLE(InspectorIOAgent);

 public:
  InspectorIOAgent(v8::Isolate*, v8_inspector::V8InspectorSession*);

 private:
  ~InspectorIOAgent() override;

  void Restore() override {}

  // Called from the front-end.
  protocol::Response resolveBlob(const String& object_id,
                                 String* uuid) override;

  v8::Isolate* isolate_;
  v8_inspector::V8InspectorSession* v8_session_;
};

}  // namespace blink

#endif  // !defined(InspectorIOAgent_h)
