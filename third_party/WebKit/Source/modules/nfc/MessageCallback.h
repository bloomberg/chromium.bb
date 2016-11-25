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
  DEFINE_INLINE_VIRTUAL_TRACE() {}
  virtual void handleMessage(const NFCMessage&) = 0;

  void setScriptState(ScriptState* scriptState) { m_scriptState = scriptState; }
  ScriptState* getScriptState() const { return m_scriptState.get(); }

 private:
  RefPtr<ScriptState> m_scriptState;
};

}  // namespace blink

#endif  // MessageCallback_h
