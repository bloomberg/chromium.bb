// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorEmulationAgent_h
#define InspectorEmulationAgent_h

#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/protocol/Emulation.h"
#include "platform/WebTaskRunner.h"

namespace blink {

class WebLocalFrameImpl;
class WebViewImpl;

namespace protocol {
namespace DOM {
class RGBA;
}  // namespace DOM
}  // namespace protocol

class InspectorEmulationAgent final
    : public InspectorBaseAgent<protocol::Emulation::Metainfo> {
  WTF_MAKE_NONCOPYABLE(InspectorEmulationAgent);

 public:
  class Client {
   public:
    virtual ~Client() {}

    virtual void setCPUThrottlingRate(double rate) {}
  };

  static InspectorEmulationAgent* create(WebLocalFrameImpl*, Client*);
  ~InspectorEmulationAgent() override;

  // protocol::Dispatcher::EmulationCommandHandler implementation.
  Response forceViewport(double x, double y, double scale) override;
  Response resetViewport() override;
  Response resetPageScaleFactor() override;
  Response setPageScaleFactor(double) override;
  Response setScriptExecutionDisabled(bool value) override;
  Response setTouchEmulationEnabled(bool enabled,
                                    Maybe<String> configuration) override;
  Response setEmulatedMedia(const String&) override;
  Response setCPUThrottlingRate(double) override;
  Response setVirtualTimePolicy(const String& policy,
                                Maybe<int> virtualTimeBudgetMs) override;
  Response setDefaultBackgroundColorOverride(
      Maybe<protocol::DOM::RGBA>) override;

  // InspectorBaseAgent overrides.
  Response disable() override;
  void restore() override;

  DECLARE_VIRTUAL_TRACE();

 private:
  InspectorEmulationAgent(WebLocalFrameImpl*, Client*);
  WebViewImpl* webViewImpl();
  void virtualTimeBudgetExpired();

  Member<WebLocalFrameImpl> m_webLocalFrameImpl;
  Client* m_client;
  TaskHandle m_virtualTimeBudgetExpiredTaskHandle;
};

}  // namespace blink

#endif  // !defined(InspectorEmulationAgent_h)
