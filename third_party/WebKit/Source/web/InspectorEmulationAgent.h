// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorEmulationAgent_h
#define InspectorEmulationAgent_h

#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/protocol/Emulation.h"

namespace blink {

class WebLocalFrameBase;
class WebViewBase;

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

    virtual void SetCPUThrottlingRate(double rate) {}
  };

  static InspectorEmulationAgent* Create(WebLocalFrameBase*, Client*);
  ~InspectorEmulationAgent() override;

  // protocol::Dispatcher::EmulationCommandHandler implementation.
  protocol::Response forceViewport(double x, double y, double scale) override;
  protocol::Response resetViewport() override;
  protocol::Response resetPageScaleFactor() override;
  protocol::Response setPageScaleFactor(double) override;
  protocol::Response setScriptExecutionDisabled(bool value) override;
  protocol::Response setTouchEmulationEnabled(
      bool enabled,
      protocol::Maybe<String> configuration) override;
  protocol::Response setEmulatedMedia(const String&) override;
  protocol::Response setCPUThrottlingRate(double) override;
  protocol::Response setVirtualTimePolicy(
      const String& policy,
      protocol::Maybe<int> virtual_time_budget_ms) override;
  protocol::Response setDefaultBackgroundColorOverride(
      protocol::Maybe<protocol::DOM::RGBA>) override;

  // InspectorBaseAgent overrides.
  protocol::Response disable() override;
  void Restore() override;

  DECLARE_VIRTUAL_TRACE();

 private:
  InspectorEmulationAgent(WebLocalFrameBase*, Client*);
  WebViewBase* GetWebViewBase();
  void VirtualTimeBudgetExpired();

  Member<WebLocalFrameBase> web_local_frame_;
  Client* client_;
};

}  // namespace blink

#endif  // !defined(InspectorEmulationAgent_h)
