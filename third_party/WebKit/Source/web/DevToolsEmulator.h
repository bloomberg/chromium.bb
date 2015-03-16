// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DevToolsEmulator_h
#define DevToolsEmulator_h

#include "wtf/Forward.h"
#include "wtf/OwnPtr.h"

namespace blink {

class IntPoint;
class WebDevToolsAgentImpl;
class WebInputEvent;
class WebViewImpl;

struct WebDeviceEmulationParams;

class DevToolsEmulator final {
public:
    explicit DevToolsEmulator(WebViewImpl*);
    ~DevToolsEmulator();

    // FIXME(dgozman): remove this after reversing emulation flow.
    void setDevToolsAgent(WebDevToolsAgentImpl*);

    // Settings overrides.
    void setTextAutosizingEnabled(bool);
    void setDeviceScaleAdjustment(float);
    void setPreferCompositingToLCDTextEnabled(bool);
    void setUseMobileViewportStyle(bool);

    // FIXME(dgozman): remove this after reversing emulation flow.
    void setDeviceMetricsOverride(int width, int height, float deviceScaleFactor, bool mobile, bool fitWindow, float scale, float offsetX, float offsetY);
    void clearDeviceMetricsOverride();

    // Emulation.
    void enableDeviceEmulation(const WebDeviceEmulationParams&);
    void disableDeviceEmulation();
    bool deviceEmulationEnabled() { return m_deviceMetricsEnabled; }
    void setTouchEventEmulationEnabled(bool);
    bool handleInputEvent(const WebInputEvent&);

private:
    void enableMobileEmulation();
    void disableMobileEmulation();

    // FIXME(dgozman): remove this after reversing emulation flow.
    void enableDeviceEmulationInner(const WebDeviceEmulationParams&);
    void disableDeviceEmulationInner();

    WebViewImpl* m_webViewImpl;
    WebDevToolsAgentImpl* m_devToolsAgent;
    bool m_deviceMetricsEnabled;
    bool m_emulateMobileEnabled;
    bool m_originalViewportEnabled;
    bool m_isOverlayScrollbarsEnabled;
    float m_originalDefaultMinimumPageScaleFactor;
    float m_originalDefaultMaximumPageScaleFactor;
    bool m_embedderTextAutosizingEnabled;
    float m_embedderDeviceScaleAdjustment;
    bool m_embedderPreferCompositingToLCDTextEnabled;
    bool m_embedderUseMobileViewport;

    bool m_touchEventEmulationEnabled;
    bool m_originalTouchEnabled;
    bool m_originalDeviceSupportsMouse;
    bool m_originalDeviceSupportsTouch;
    int m_originalMaxTouchPoints;
    OwnPtr<IntPoint> m_lastPinchAnchorCss;
    OwnPtr<IntPoint> m_lastPinchAnchorDip;

    // FIXME(dgozman): remove this after reversing emulation flow.
    bool m_ignoreSetOverrides;
};

} // namespace blink

#endif
