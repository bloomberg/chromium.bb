// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef InspectorEmulationAgent_h
#define InspectorEmulationAgent_h

#include "core/InspectorFrontend.h"
#include "core/inspector/InspectorBaseAgent.h"

namespace blink {

class WebLocalFrameImpl;
class WebViewImpl;

using ErrorString = String;

class InspectorEmulationAgent final : public InspectorBaseAgent<InspectorEmulationAgent, InspectorFrontend::Emulation>, public InspectorBackendDispatcher::EmulationCommandHandler {
    WTF_MAKE_NONCOPYABLE(InspectorEmulationAgent);
public:
    class Client {
    public:
        virtual ~Client() {}

        virtual void setCPUThrottlingRate(double rate) {}
    };

    static PassOwnPtrWillBeRawPtr<InspectorEmulationAgent> create(WebLocalFrameImpl*, Client*);
    ~InspectorEmulationAgent() override;

    void viewportChanged();

    // InspectorBackendDispatcher::EmulationCommandHandler implementation.
    void resetPageScaleFactor(ErrorString*) override;
    void setPageScaleFactor(ErrorString*, double pageScaleFactor) override;
    void setScriptExecutionDisabled(ErrorString*, bool) override;
    void setTouchEmulationEnabled(ErrorString*, bool enabled, const String* configuration) override;
    void setEmulatedMedia(ErrorString*, const String&) override;
    void setCPUThrottlingRate(ErrorString*, double rate) override;

    // InspectorBaseAgent overrides.
    void disable(ErrorString*) override;
    void restore() override;
    void discardAgent() override;
    void didCommitLoadForLocalFrame(LocalFrame*) override;

    DECLARE_VIRTUAL_TRACE();

private:
    InspectorEmulationAgent(WebLocalFrameImpl*, Client*);
    WebViewImpl* webViewImpl();

    RawPtrWillBeMember<WebLocalFrameImpl> m_webLocalFrameImpl;
    Client* m_client;
};

} // namespace blink

#endif // !defined(InspectorEmulationAgent_h)
