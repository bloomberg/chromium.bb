// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MessageCallback_h
#define MessageCallback_h

#include "platform/heap/Handle.h"

namespace blink {

class NFCMessage;
class ScriptState;

class MessageCallback : public GarbageCollectedFinalized<MessageCallback> {
 public:
  virtual ~MessageCallback() {}
  virtual void Trace(blink::Visitor* visitor) {}
  virtual void handleMessage(const NFCMessage&) = 0;

  void SetScriptState(ScriptState* script_state) {
    script_state_ = script_state;
  }
  ScriptState* GetScriptState() const { return script_state_.get(); }

 private:
  scoped_refptr<ScriptState> script_state_;
};

}  // namespace blink

#endif  // MessageCallback_h
