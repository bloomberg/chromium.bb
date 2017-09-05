// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorEmulationAgent_h
#define InspectorEmulationAgent_h

#include "core/CoreExport.h"
#include "core/inspector/InspectorBaseAgent.h"
#include "core/inspector/protocol/Emulation.h"
#include "platform/scheduler/renderer/web_view_scheduler.h"
#include "platform/wtf/Time.h"

namespace blink {

class WebLocalFrameImpl;
class WebViewImpl;

namespace protocol {
namespace DOM {
class RGBA;
}  // namespace DOM
}  // namespace protocol

class CORE_EXPORT InspectorEmulationAgent final
    : public InspectorBaseAgent<protocol::Emulation::Metainfo>,
      public WebViewScheduler::VirtualTimeObserver {
  WTF_MAKE_NONCOPYABLE(InspectorEmulationAgent);

 public:
  class Client {
   public:
    virtual ~Client() {}

    virtual void SetCPUThrottlingRate(double rate) {}
  };

  static InspectorEmulationAgent* Create(WebLocalFrameImpl*, Client*);
  ~InspectorEmulationAgent() override;

  // protocol::Dispatcher::EmulationCommandHandler implementation.
  protocol::Response resetPageScaleFactor() override;
  protocol::Response setPageScaleFactor(double) override;
  protocol::Response setScriptExecutionDisabled(bool value) override;
  protocol::Response setTouchEmulationEnabled(
      bool enabled,
      protocol::Maybe<int> max_touch_points) override;
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

  // scheduler::WebViewScheduler::VirtualTimeObserver implementation.
  void OnVirtualTimePaused(WTF::TimeDelta virtual_time_offset) override;

  DECLARE_VIRTUAL_TRACE();

 private:
  InspectorEmulationAgent(WebLocalFrameImpl*, Client*);
  WebViewImpl* GetWebViewImpl();
  void VirtualTimeBudgetExpired();

  Member<WebLocalFrameImpl> web_local_frame_;
  Client* client_;
  bool virtual_time_observer_registered_;
};

}  // namespace blink

#endif  // !defined(InspectorEmulationAgent_h)
