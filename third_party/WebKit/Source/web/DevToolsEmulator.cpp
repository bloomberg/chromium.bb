// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "web/DevToolsEmulator.h"

#include "core/frame/Settings.h"
#include "core/page/Page.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "public/web/WebDevToolsAgentClient.h"
#include "public/web/WebDeviceEmulationParams.h"
#include "web/WebDevToolsAgentImpl.h"
#include "web/WebLocalFrameImpl.h"
#include "web/WebSettingsImpl.h"
#include "web/WebViewImpl.h"

namespace {

static float calculateDeviceScaleAdjustment(int width, int height, float deviceScaleFactor)
{
    // Chromium on Android uses a device scale adjustment for fonts used in text autosizing for
    // improved legibility. This function computes this adjusted value for text autosizing.
    // For a description of the Android device scale adjustment algorithm, see:
    // chrome/browser/chrome_content_browser_client.cc, GetDeviceScaleAdjustment(...)
    if (!width || !height || !deviceScaleFactor)
        return 1;

    static const float kMinFSM = 1.05f;
    static const int kWidthForMinFSM = 320;
    static const float kMaxFSM = 1.3f;
    static const int kWidthForMaxFSM = 800;

    float minWidth = std::min(width, height) / deviceScaleFactor;
    if (minWidth <= kWidthForMinFSM)
        return kMinFSM;
    if (minWidth >= kWidthForMaxFSM)
        return kMaxFSM;

    // The font scale multiplier varies linearly between kMinFSM and kMaxFSM.
    float ratio = static_cast<float>(minWidth - kWidthForMinFSM) / (kWidthForMaxFSM - kWidthForMinFSM);
    return ratio * (kMaxFSM - kMinFSM) + kMinFSM;
}

} // namespace

namespace blink {

DevToolsEmulator::DevToolsEmulator(WebViewImpl* webViewImpl)
    : m_webViewImpl(webViewImpl)
    , m_devToolsAgent(nullptr)
    , m_deviceMetricsEnabled(false)
    , m_emulateMobileEnabled(false)
    , m_originalViewportEnabled(false)
    , m_isOverlayScrollbarsEnabled(false)
    , m_originalDefaultMinimumPageScaleFactor(0)
    , m_originalDefaultMaximumPageScaleFactor(0)
    , m_embedderTextAutosizingEnabled(webViewImpl->page()->settings().textAutosizingEnabled())
    , m_embedderDeviceScaleAdjustment(webViewImpl->page()->settings().deviceScaleAdjustment())
    , m_embedderPreferCompositingToLCDTextEnabled(webViewImpl->page()->settings().preferCompositingToLCDTextEnabled())
    , m_embedderUseMobileViewport(webViewImpl->page()->settings().useMobileViewportStyle())
{
}

DevToolsEmulator::~DevToolsEmulator()
{
}

void DevToolsEmulator::setDevToolsAgent(WebDevToolsAgentImpl* agent)
{
    m_devToolsAgent = agent;
}

void DevToolsEmulator::setTextAutosizingEnabled(bool enabled)
{
    m_embedderTextAutosizingEnabled = enabled;
    bool emulateMobileEnabled = m_deviceMetricsEnabled && m_emulateMobileEnabled;
    if (!emulateMobileEnabled)
        m_webViewImpl->page()->settings().setTextAutosizingEnabled(enabled);
}

void DevToolsEmulator::setDeviceScaleAdjustment(float deviceScaleAdjustment)
{
    m_embedderDeviceScaleAdjustment = deviceScaleAdjustment;
    bool emulateMobileEnabled = m_deviceMetricsEnabled && m_emulateMobileEnabled;
    if (!emulateMobileEnabled)
        m_webViewImpl->page()->settings().setDeviceScaleAdjustment(deviceScaleAdjustment);
}

void DevToolsEmulator::setPreferCompositingToLCDTextEnabled(bool enabled)
{
    m_embedderPreferCompositingToLCDTextEnabled = enabled;
    bool emulateMobileEnabled = m_deviceMetricsEnabled && m_emulateMobileEnabled;
    if (!emulateMobileEnabled)
        m_webViewImpl->page()->settings().setPreferCompositingToLCDTextEnabled(enabled);
}

void DevToolsEmulator::setUseMobileViewportStyle(bool enabled)
{
    m_embedderUseMobileViewport = enabled;
    bool emulateMobileEnabled = m_deviceMetricsEnabled && m_emulateMobileEnabled;
    if (!emulateMobileEnabled)
        m_webViewImpl->page()->settings().setUseMobileViewportStyle(enabled);
}

void DevToolsEmulator::setDeviceMetricsOverride(int width, int height, float deviceScaleFactor, bool mobile, bool fitWindow, float scale, float offsetX, float offsetY)
{
    if (!m_deviceMetricsEnabled) {
        m_deviceMetricsEnabled = true;
        m_webViewImpl->setBackgroundColorOverride(Color::darkGray);
        m_webViewImpl->updateShowFPSCounterAndContinuousPainting();
        if (m_devToolsAgent)
            m_devToolsAgent->pageAgent()->setViewportNotificationsEnabled(true);
    }
    m_webViewImpl->page()->settings().setDeviceScaleAdjustment(calculateDeviceScaleAdjustment(width, height, deviceScaleFactor));

    if (mobile)
        enableMobileEmulation();
    else
        disableMobileEmulation();

    WebDeviceEmulationParams params;
    params.screenPosition = mobile ? WebDeviceEmulationParams::Mobile : WebDeviceEmulationParams::Desktop;
    params.deviceScaleFactor = deviceScaleFactor;
    params.viewSize = WebSize(width, height);
    params.fitToView = fitWindow;
    params.scale = scale;
    params.offset = WebFloatPoint(offsetX, offsetY);
    ASSERT(m_devToolsAgent);
    m_devToolsAgent->client()->enableDeviceEmulation(params);

    if (Document* document = m_webViewImpl->mainFrameImpl()->frame()->document()) {
        document->styleResolverChanged();
        document->mediaQueryAffectingValueChanged();
    }
}

void DevToolsEmulator::clearDeviceMetricsOverride()
{
    if (m_deviceMetricsEnabled) {
        m_deviceMetricsEnabled = false;
        m_webViewImpl->setBackgroundColorOverride(Color::transparent);
        m_webViewImpl->updateShowFPSCounterAndContinuousPainting();
        if (m_devToolsAgent)
            m_devToolsAgent->pageAgent()->setViewportNotificationsEnabled(false);
        m_webViewImpl->page()->settings().setDeviceScaleAdjustment(m_embedderDeviceScaleAdjustment);
        disableMobileEmulation();
        ASSERT(m_devToolsAgent);
        m_devToolsAgent->client()->disableDeviceEmulation();

        if (Document* document = m_webViewImpl->mainFrameImpl()->frame()->document()) {
            document->styleResolverChanged();
            document->mediaQueryAffectingValueChanged();
        }
    }
}

void DevToolsEmulator::enableMobileEmulation()
{
    if (m_emulateMobileEnabled)
        return;
    m_emulateMobileEnabled = true;
    m_isOverlayScrollbarsEnabled = RuntimeEnabledFeatures::overlayScrollbarsEnabled();
    RuntimeEnabledFeatures::setOverlayScrollbarsEnabled(true);
    m_originalViewportEnabled = RuntimeEnabledFeatures::cssViewportEnabled();
    RuntimeEnabledFeatures::setCSSViewportEnabled(true);
    m_webViewImpl->enableViewport();
    m_webViewImpl->settings()->setViewportMetaEnabled(true);
    m_webViewImpl->settings()->setShrinksViewportContentToFit(true);
    m_webViewImpl->page()->settings().setTextAutosizingEnabled(true);
    m_webViewImpl->page()->settings().setPreferCompositingToLCDTextEnabled(true);
    m_webViewImpl->page()->settings().setUseMobileViewportStyle(true);
    m_webViewImpl->setZoomFactorOverride(1);

    m_originalDefaultMinimumPageScaleFactor = m_webViewImpl->defaultMinimumPageScaleFactor();
    m_originalDefaultMaximumPageScaleFactor = m_webViewImpl->defaultMaximumPageScaleFactor();
    m_webViewImpl->setDefaultPageScaleLimits(0.25f, 5);
}

void DevToolsEmulator::disableMobileEmulation()
{
    if (!m_emulateMobileEnabled)
        return;
    RuntimeEnabledFeatures::setOverlayScrollbarsEnabled(m_isOverlayScrollbarsEnabled);
    RuntimeEnabledFeatures::setCSSViewportEnabled(m_originalViewportEnabled);
    m_webViewImpl->disableViewport();
    m_webViewImpl->settings()->setViewportMetaEnabled(false);
    m_webViewImpl->settings()->setShrinksViewportContentToFit(false);
    m_webViewImpl->page()->settings().setTextAutosizingEnabled(m_embedderTextAutosizingEnabled);
    m_webViewImpl->page()->settings().setPreferCompositingToLCDTextEnabled(m_embedderPreferCompositingToLCDTextEnabled);
    m_webViewImpl->page()->settings().setUseMobileViewportStyle(m_embedderUseMobileViewport);
    m_webViewImpl->setZoomFactorOverride(0);
    m_emulateMobileEnabled = false;
    m_webViewImpl->setDefaultPageScaleLimits(
        m_originalDefaultMinimumPageScaleFactor,
        m_originalDefaultMaximumPageScaleFactor);
}

} // namespace blink
