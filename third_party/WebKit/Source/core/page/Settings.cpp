/*
 * Copyright (C) 2006, 2007, 2008, 2009, 2011, 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "core/page/Settings.h"

#include <limits>
#include "core/dom/Document.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/page/Chrome.h"
#include "core/frame/Frame.h"
#include "core/page/FrameTree.h"
#include "core/frame/FrameView.h"
#include "core/page/Page.h"
#include "platform/scroll/ScrollbarTheme.h"

using namespace std;

namespace WebCore {


// NOTEs
//  1) EditingMacBehavior comprises builds on Mac;
//  2) EditingWindowsBehavior comprises builds on Windows;
//  3) EditingUnixBehavior comprises all unix-based systems, but Darwin/MacOS/Android (and then abusing the terminology);
//  4) EditingAndroidBehavior comprises Android builds.
// 99) MacEditingBehavior is used a fallback.
static EditingBehaviorType editingBehaviorTypeForPlatform()
{
    return
#if OS(MACOSX)
    EditingMacBehavior
#elif OS(WIN)
    EditingWindowsBehavior
#elif OS(ANDROID)
    EditingAndroidBehavior
#else // Rest of the UNIX-like systems
    EditingUnixBehavior
#endif
    ;
}

static const bool defaultUnifiedTextCheckerEnabled = false;
#if OS(MACOSX)
static const bool defaultSmartInsertDeleteEnabled = true;
#else
static const bool defaultSmartInsertDeleteEnabled = false;
#endif
#if OS(WIN)
static const bool defaultSelectTrailingWhitespaceEnabled = true;
#else
static const bool defaultSelectTrailingWhitespaceEnabled = false;
#endif

Settings::Settings()
    : m_mediaTypeOverride("screen")
    , m_accessibilityFontScaleFactor(1)
    , m_deviceScaleAdjustment(1.0f)
#if HACK_FORCE_TEXT_AUTOSIZING_ON_DESKTOP
    , m_textAutosizingWindowSizeOverride(320, 480)
    , m_textAutosizingEnabled(true)
#else
    , m_textAutosizingEnabled(false)
#endif
    , m_useWideViewport(true)
    , m_loadWithOverviewMode(true)
    SETTINGS_INITIALIZER_LIST
    , m_isJavaEnabled(false)
    , m_loadsImagesAutomatically(false)
    , m_areImagesEnabled(true)
    , m_arePluginsEnabled(false)
    , m_isScriptEnabled(false)
    , m_dnsPrefetchingEnabled(false)
    , m_touchEventEmulationEnabled(false)
    , m_openGLMultisamplingEnabled(false)
    , m_viewportEnabled(false)
    , m_viewportMetaEnabled(false)
    , m_compositorDrivenAcceleratedScrollingEnabled(false)
    , m_layerSquashingEnabled(false)
{
}

PassOwnPtr<Settings> Settings::create()
{
    return adoptPtr(new Settings);
}

SETTINGS_SETTER_BODIES

void Settings::setDelegate(SettingsDelegate* delegate)
{
    m_delegate = delegate;
}

void Settings::invalidate(SettingsDelegate::ChangeType changeType)
{
    if (m_delegate)
        m_delegate->settingsChanged(changeType);
}

// This is a total hack and should be removed.
Page* Settings::pageOfShame() const
{
    if (!m_delegate)
        return 0;
    return m_delegate->page();
}

void Settings::setTextAutosizingEnabled(bool textAutosizingEnabled)
{
    if (m_textAutosizingEnabled == textAutosizingEnabled)
        return;

    m_textAutosizingEnabled = textAutosizingEnabled;
    invalidate(SettingsDelegate::StyleChange);
}

bool Settings::textAutosizingEnabled() const
{
    return InspectorInstrumentation::overrideTextAutosizing(pageOfShame(), m_textAutosizingEnabled);
}

void Settings::setTextAutosizingWindowSizeOverride(const IntSize& textAutosizingWindowSizeOverride)
{
    if (m_textAutosizingWindowSizeOverride == textAutosizingWindowSizeOverride)
        return;

    m_textAutosizingWindowSizeOverride = textAutosizingWindowSizeOverride;
    invalidate(SettingsDelegate::StyleChange);
}

void Settings::setUseWideViewport(bool useWideViewport)
{
    if (m_useWideViewport == useWideViewport)
        return;

    m_useWideViewport = useWideViewport;
    invalidate(SettingsDelegate::ViewportDescriptionChange);
}

void Settings::setLoadWithOverviewMode(bool loadWithOverviewMode)
{
    if (m_loadWithOverviewMode == loadWithOverviewMode)
        return;

    m_loadWithOverviewMode = loadWithOverviewMode;
    invalidate(SettingsDelegate::ViewportDescriptionChange);
}

void Settings::setAccessibilityFontScaleFactor(float fontScaleFactor)
{
    m_accessibilityFontScaleFactor = fontScaleFactor;
    invalidate(SettingsDelegate::TextAutosizingChange);
}

void Settings::setDeviceScaleAdjustment(float deviceScaleAdjustment)
{
    m_deviceScaleAdjustment = deviceScaleAdjustment;
    invalidate(SettingsDelegate::TextAutosizingChange);
}

float Settings::deviceScaleAdjustment() const
{
    return InspectorInstrumentation::overrideFontScaleFactor(pageOfShame(), m_deviceScaleAdjustment);
}

void Settings::setMediaTypeOverride(const String& mediaTypeOverride)
{
    if (m_mediaTypeOverride == mediaTypeOverride)
        return;

    m_mediaTypeOverride = mediaTypeOverride;
    invalidate(SettingsDelegate::MediaTypeChange);
}

void Settings::setLoadsImagesAutomatically(bool loadsImagesAutomatically)
{
    m_loadsImagesAutomatically = loadsImagesAutomatically;
    invalidate(SettingsDelegate::ImageLoadingChange);
}

void Settings::setScriptEnabled(bool isScriptEnabled)
{
    m_isScriptEnabled = isScriptEnabled;
    InspectorInstrumentation::scriptsEnabled(pageOfShame(), m_isScriptEnabled);
}

void Settings::setJavaEnabled(bool isJavaEnabled)
{
    m_isJavaEnabled = isJavaEnabled;
}

void Settings::setImagesEnabled(bool areImagesEnabled)
{
    m_areImagesEnabled = areImagesEnabled;
    invalidate(SettingsDelegate::ImageLoadingChange);
}

void Settings::setPluginsEnabled(bool arePluginsEnabled)
{
    m_arePluginsEnabled = arePluginsEnabled;
}

void Settings::setDNSPrefetchingEnabled(bool dnsPrefetchingEnabled)
{
    if (m_dnsPrefetchingEnabled == dnsPrefetchingEnabled)
        return;

    m_dnsPrefetchingEnabled = dnsPrefetchingEnabled;
    invalidate(SettingsDelegate::DNSPrefetchingChange);
}

void Settings::setMockScrollbarsEnabled(bool flag)
{
    ScrollbarTheme::setMockScrollbarsEnabled(flag);
}

bool Settings::mockScrollbarsEnabled()
{
    return ScrollbarTheme::mockScrollbarsEnabled();
}

void Settings::setOpenGLMultisamplingEnabled(bool flag)
{
    if (m_openGLMultisamplingEnabled == flag)
        return;

    m_openGLMultisamplingEnabled = flag;
    invalidate(SettingsDelegate::MultisamplingChange);
}

bool Settings::openGLMultisamplingEnabled()
{
    return m_openGLMultisamplingEnabled;
}

void Settings::setViewportEnabled(bool enabled)
{
    if (m_viewportEnabled == enabled)
        return;

    m_viewportEnabled = enabled;
    invalidate(SettingsDelegate::ViewportDescriptionChange);
}

void Settings::setViewportMetaEnabled(bool enabled)
{
    m_viewportMetaEnabled = enabled;
}

} // namespace WebCore
