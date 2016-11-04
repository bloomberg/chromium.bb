// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorLogAgent_h
#define InspectorLogAgent_h

#include "core/CoreExport.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/protocol/Log.h"

namespace blink {

class ConsoleMessage;
class ConsoleMessageStorage;

class CORE_EXPORT InspectorLogAgent
    : public InspectorBaseAgent<protocol::Log::Metainfo> {
  WTF_MAKE_NONCOPYABLE(InspectorLogAgent);

 public:
  explicit InspectorLogAgent(ConsoleMessageStorage*);
  ~InspectorLogAgent() override;
  DECLARE_VIRTUAL_TRACE();

  void restore() override;

  // Called from InspectorInstrumentation.
  void consoleMessageAdded(ConsoleMessage*);

  // Protocol methods.
  Response enable() override;
  Response disable() override;
  Response clear() override;

 private:
  bool m_enabled;
  Member<ConsoleMessageStorage> m_storage;
};

}  // namespace blink

#endif  // !defined(InspectorLogAgent_h)
