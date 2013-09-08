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
#include "core/page/Frame.h"
#include "core/page/FrameTree.h"
#include "core/page/FrameView.h"
#include "core/page/Page.h"
#include "core/rendering/TextAutosizer.h"

using namespace std;

namespace WebCore {

static void setImageLoadingSettings(Page* page)
{
    for (Frame* frame = page->mainFrame(); frame; frame = frame->tree()->traverseNext()) {
        frame->document()->fetcher()->setImagesEnabled(page->settings().areImagesEnabled());
        frame->document()->fetcher()->setAutoLoadImages(page->settings().loadsImagesAutomatically());
    }
}

// Sets the entry in the font map for the given script. If family is the empty string, removes the entry instead.
static inline void setGenericFontFamilyMap(ScriptFontFamilyMap& fontMap, const AtomicString& family, UScriptCode script, Page* page)
{
    ScriptFontFamilyMap::iterator it = fontMap.find(static_cast<int>(script));
    if (family.isEmpty()) {
        if (it == fontMap.end())
            return;
        fontMap.remove(it);
    } else if (it != fontMap.end() && it->value == family)
        return;
    else
        fontMap.set(static_cast<int>(script), family);

    if (page)
        page->setNeedsRecalcStyleInAllFrames();
}

static inline const AtomicString& getGenericFontFamilyForScript(const ScriptFontFamilyMap& fontMap, UScriptCode script)
{
    ScriptFontFamilyMap::const_iterator it = fontMap.find(static_cast<int>(script));
    if (it != fontMap.end())
        return it->value;
    if (script != USCRIPT_COMMON)
        return getGenericFontFamilyForScript(fontMap, USCRIPT_COMMON);
    return emptyAtom;
}

bool Settings::gMockScrollbarsEnabled = false;
bool Settings::gUsesOverlayScrollbars = false;

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
#elif OS(UNIX)
    EditingUnixBehavior
#else
    // Fallback
    EditingMacBehavior
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
    , m_textAutosizingFontScaleFactor(1)
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
    , m_isCSSCustomFilterEnabled(false)
    , m_cssStickyPositionEnabled(true)
    , m_dnsPrefetchingEnabled(false)
    , m_touchEventEmulationEnabled(false)
    , m_viewportEnabled(false)
    , m_setImageLoadingSettingsTimer(this, &Settings::imageLoadingSettingsTimerFired)
    , m_compositorDrivenAcceleratedScrollingEnabled(false)
{
    m_page = page; // Page is not yet fully initialized wen constructing Settings, so keeping m_page null over initializeDefaultFontFamilies() call.
}

PassOwnPtr<Settings> Settings::create(Page* page)
{
    return adoptPtr(new Settings(page));
}

SETTINGS_SETTER_BODIES

const AtomicString& Settings::standardFontFamily(UScriptCode script) const
{
    return getGenericFontFamilyForScript(m_standardFontFamilyMap, script);
}

void Settings::setStandardFontFamily(const AtomicString& family, UScriptCode script)
{
    setGenericFontFamilyMap(m_standardFontFamilyMap, family, script, m_page);
}

const AtomicString& Settings::fixedFontFamily(UScriptCode script) const
{
    return getGenericFontFamilyForScript(m_fixedFontFamilyMap, script);
}

void Settings::setFixedFontFamily(const AtomicString& family, UScriptCode script)
{
    setGenericFontFamilyMap(m_fixedFontFamilyMap, family, script, m_page);
}

const AtomicString& Settings::serifFontFamily(UScriptCode script) const
{
    return getGenericFontFamilyForScript(m_serifFontFamilyMap, script);
}

void Settings::setSerifFontFamily(const AtomicString& family, UScriptCode script)
{
     setGenericFontFamilyMap(m_serifFontFamilyMap, family, script, m_page);
}

const AtomicString& Settings::sansSerifFontFamily(UScriptCode script) const
{
    return getGenericFontFamilyForScript(m_sansSerifFontFamilyMap, script);
}

void Settings::setSansSerifFontFamily(const AtomicString& family, UScriptCode script)
{
    setGenericFontFamilyMap(m_sansSerifFontFamilyMap, family, script, m_page);
}

const AtomicString& Settings::cursiveFontFamily(UScriptCode script) const
{
    return getGenericFontFamilyForScript(m_cursiveFontFamilyMap, script);
}

void Settings::setCursiveFontFamily(const AtomicString& family, UScriptCode script)
{
    setGenericFontFamilyMap(m_cursiveFontFamilyMap, family, script, m_page);
}

const AtomicString& Settings::fantasyFontFamily(UScriptCode script) const
{
    return getGenericFontFamilyForScript(m_fantasyFontFamilyMap, script);
}

void Settings::setFantasyFontFamily(const AtomicString& family, UScriptCode script)
{
    setGenericFontFamilyMap(m_fantasyFontFamilyMap, family, script, m_page);
}

const AtomicString& Settings::pictographFontFamily(UScriptCode script) const
{
    return getGenericFontFamilyForScript(m_pictographFontFamilyMap, script);
}

void Settings::setPictographFontFamily(const AtomicString& family, UScriptCode script)
{
    setGenericFontFamilyMap(m_pictographFontFamilyMap, family, script, m_page);
}

void Settings::setTextAutosizingEnabled(bool textAutosizingEnabled)
{
    if (m_textAutosizingEnabled == textAutosizingEnabled)
        return;

    m_textAutosizingEnabled = textAutosizingEnabled;
    m_page->setNeedsRecalcStyleInAllFrames();
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
        m_page->chrome().dispatchViewportPropertiesDidChange(m_page->mainFrame()->document()->viewportArguments());
}

void Settings::setLoadWithOverviewMode(bool loadWithOverviewMode)
{
    if (m_loadWithOverviewMode == loadWithOverviewMode)
        return;

    m_loadWithOverviewMode = loadWithOverviewMode;
    if (m_page->mainFrame())
        m_page->chrome().dispatchViewportPropertiesDidChange(m_page->mainFrame()->document()->viewportArguments());
}

void Settings::setTextAutosizingFontScaleFactor(float fontScaleFactor)
{
    m_textAutosizingFontScaleFactor = fontScaleFactor;

    // FIXME: I wonder if this needs to traverse frames like in WebViewImpl::resize, or whether there is only one document per Settings instance?
    for (Frame* frame = m_page->mainFrame(); frame; frame = frame->tree()->traverseNext())
        frame->document()->textAutosizer()->recalculateMultipliers();

    m_page->setNeedsRecalcStyleInAllFrames();
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

void Settings::setUserStyleSheetLocation(const KURL& userStyleSheetLocation)
{
    if (m_userStyleSheetLocation == userStyleSheetLocation)
        return;

    m_userStyleSheetLocation = userStyleSheetLocation;

    m_page->userStyleSheetLocationChanged();
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
    gMockScrollbarsEnabled = flag;
}

bool Settings::mockScrollbarsEnabled()
{
    return gMockScrollbarsEnabled;
}

void Settings::setUsesOverlayScrollbars(bool flag)
{
    gUsesOverlayScrollbars = flag;
}

bool Settings::usesOverlayScrollbars()
{
    return gUsesOverlayScrollbars;
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
        m_page->mainFrame()->document()->updateViewportArguments();
}

} // namespace WebCore
