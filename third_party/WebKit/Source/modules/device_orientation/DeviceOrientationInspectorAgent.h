// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DeviceOrientationInspectorAgent_h
#define DeviceOrientationInspectorAgent_h

#include "core/inspector/InspectorBaseAgent.h"
#include "wtf/text/WTFString.h"

namespace blink {

class DeviceOrientationController;
class Page;

typedef String ErrorString;

class DeviceOrientationInspectorAgent final : public InspectorBaseAgent<DeviceOrientationInspectorAgent>, public InspectorBackendDispatcher::DeviceOrientationCommandHandler {
    WTF_MAKE_NONCOPYABLE(DeviceOrientationInspectorAgent);
public:
    static void provideTo(Page&);

    virtual ~DeviceOrientationInspectorAgent();

    // Protocol methods.
    virtual void setDeviceOrientationOverride(ErrorString*, double, double, double) override;
    virtual void clearDeviceOrientationOverride(ErrorString*) override;

    // Inspector Controller API.
    virtual void clearFrontend() override;
    virtual void restore() override;
    virtual void didCommitLoadForMainFrame() override;

private:
    explicit DeviceOrientationInspectorAgent(Page&);
    DeviceOrientationController& controller();
    Page& m_page;
};

} // namespace blink


#endif // !defined(DeviceOrientationInspectorAgent_h)
