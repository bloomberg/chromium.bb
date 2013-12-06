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
#include "core/fetch/ResourceFetcher.h"
#include "core/inspector/InspectorInstrumentation.h"
#include "core/page/Chrome.h"
#include "core/frame/Frame.h"
#include "core/page/FrameTree.h"
#include "core/frame/FrameView.h"
#include "core/page/Page.h"
#include "core/rendering/TextAutosizer.h"
#include "platform/scroll/ScrollbarTheme.h"

using namespace std;

namespace WebCore {

static void setImageLoadingSettings(Page* page)
{
    for (Frame* frame = page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        frame->document()->fetcher()->setImagesEnabled(page->settings().areImagesEnabled());
        frame->document()->fetcher()->setAutoLoadImages(page->settings().loadsImagesAutomatically());
    }
}

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

Settings::Settings(Page* page)
    : m_page(0)
    , m_mediaTypeOverride("screen")
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
    , m_setImageLoadingSettingsTimer(this, &Settings::imageLoadingSettingsTimerFired)
{
    m_page = page; // Page is not yet fully initialized wen constructing Settings, so keeping m_page null over initializeDefaultFontFamilies() call.
}

PassOwnPtr<Settings> Settings::create(Page* page)
{
    return adoptPtr(new Settings(page));
}

SETTINGS_SETTER_BODIES

void Settings::setTextAutosizingEnabled(bool textAutosizingEnabled)
{
    if (m_textAutosizingEnabled == textAutosizingEnabled)
        return;

    m_textAutosizingEnabled = textAutosizingEnabled;
    m_page->setNeedsRecalcStyleInAllFrames();
}

bool Settings::textAutosizingEnabled() const
{
    return InspectorInstrumentation::overrideTextAutosizing(m_page, m_textAutosizingEnabled);
}

void Settings::setTextAutosizingWindowSizeOverride(const IntSize& textAutosizingWindowSizeOverride)
{
    if (m_textAutosizingWindowSizeOverride == textAutosizingWindowSizeOverride)
        return;

    m_textAutosizingWindowSizeOverride = textAutosizingWindowSizeOverride;
    m_page->setNeedsRecalcStyleInAllFrames();
}

void Settings::setUseWideViewport(bool useWideViewport)
{
    if (m_useWideViewport == useWideViewport)
        return;

    m_useWideViewport = useWideViewport;
    if (m_page->mainFrame())
        m_page->chrome().dispatchViewportPropertiesDidChange(m_page->mainFrame()->document()->viewportDescription());
}

void Settings::setLoadWithOverviewMode(bool loadWithOverviewMode)
{
    if (m_loadWithOverviewMode == loadWithOverviewMode)
        return;

    m_loadWithOverviewMode = loadWithOverviewMode;
    if (m_page->mainFrame())
        m_page->chrome().dispatchViewportPropertiesDidChange(m_page->mainFrame()->document()->viewportDescription());
}

void Settings::recalculateTextAutosizingMultipliers()
{
    // FIXME: I wonder if this needs to traverse frames like in WebViewImpl::resize, or whether there is only one document per Settings instance?
    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        TextAutosizer* textAutosizer = frame->document()->textAutosizer();
        if (textAutosizer)
            textAutosizer->recalculateMultipliers();
    }

    m_page->setNeedsRecalcStyleInAllFrames();
}

void Settings::setAccessibilityFontScaleFactor(float fontScaleFactor)
{
    m_accessibilityFontScaleFactor = fontScaleFactor;
    recalculateTextAutosizingMultipliers();
}

void Settings::setDeviceScaleAdjustment(float deviceScaleAdjustment)
{
    m_deviceScaleAdjustment = deviceScaleAdjustment;
    recalculateTextAutosizingMultipliers();
}

float Settings::deviceScaleAdjustment() const
{
    return InspectorInstrumentation::overrideFontScaleFactor(m_page, m_deviceScaleAdjustment);
}

void Settings::setMediaTypeOverride(const String& mediaTypeOverride)
{
    if (m_mediaTypeOverride == mediaTypeOverride)
        return;

    m_mediaTypeOverride = mediaTypeOverride;

    Frame* mainFrame = m_page->mainFrame();
    ASSERT(mainFrame);
    FrameView* view = mainFrame->view();
    ASSERT(view);

    view->setMediaType(mediaTypeOverride);
    m_page->setNeedsRecalcStyleInAllFrames();
}

void Settings::setLoadsImagesAutomatically(bool loadsImagesAutomatically)
{
    m_loadsImagesAutomatically = loadsImagesAutomatically;

    // Changing this setting to true might immediately start new loads for images that had previously had loading disabled.
    // If this happens while a WebView is being dealloc'ed, and we don't know the WebView is being dealloc'ed, these new loads
    // can cause crashes downstream when the WebView memory has actually been free'd.
    // One example where this can happen is in Mac apps that subclass WebView then do work in their overridden dealloc methods.
    // Starting these loads synchronously is not important.  By putting it on a 0-delay, properly closing the Page cancels them
    // before they have a chance to really start.
    // See http://webkit.org/b/60572 for more discussion.
    m_setImageLoadingSettingsTimer.startOneShot(0);
}

void Settings::imageLoadingSettingsTimerFired(Timer<Settings>*)
{
    setImageLoadingSettings(m_page);
}

void Settings::setScriptEnabled(bool isScriptEnabled)
{
    m_isScriptEnabled = isScriptEnabled;
    InspectorInstrumentation::scriptsEnabled(m_page, m_isScriptEnabled);
}

void Settings::setJavaEnabled(bool isJavaEnabled)
{
    m_isJavaEnabled = isJavaEnabled;
}

void Settings::setImagesEnabled(bool areImagesEnabled)
{
    m_areImagesEnabled = areImagesEnabled;

    // See comment in setLoadsImagesAutomatically.
    m_setImageLoadingSettingsTimer.startOneShot(0);
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
    m_page->dnsPrefetchingStateChanged();
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
    m_page->multisamplingChanged();
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
    if (m_page->mainFrame())
        m_page->mainFrame()->document()->updateViewportDescription();
}

void Settings::setViewportMetaEnabled(bool enabled)
{
    m_viewportMetaEnabled = enabled;
}

} // namespace WebCore
