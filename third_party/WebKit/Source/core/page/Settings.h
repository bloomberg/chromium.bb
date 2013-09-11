/*
 * Copyright (C) 2003, 2006, 2007, 2008, 2009, 2011, 2012 Apple Inc. All rights reserved.
 *           (C) 2006 Graham Dennis (graham.dennis@gmail.com)
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

#ifndef Settings_h
#define Settings_h

#include "SettingsMacros.h"
#include "core/editing/EditingBehaviorTypes.h"
#include "core/platform/Timer.h"
#include "core/platform/graphics/IntSize.h"
#include "weborigin/KURL.h"
#include "wtf/HashMap.h"
#include "wtf/text/AtomicString.h"
#include "wtf/text/AtomicStringHash.h"

namespace WebCore {

class Page;

enum EditableLinkBehavior {
    EditableLinkDefaultBehavior,
    EditableLinkAlwaysLive,
    EditableLinkOnlyLiveWithShiftKey,
    EditableLinkLiveWhenNotFocused,
    EditableLinkNeverLive
};

// UScriptCode uses -1 and 0 for UScriptInvalidCode and UScriptCommon.
// We need to use -2 and -3 for empty value and deleted value.
struct UScriptCodeHashTraits : WTF::GenericHashTraits<int> {
    static const bool emptyValueIsZero = false;
    static int emptyValue() { return -2; }
    static void constructDeletedValue(int& slot) { slot = -3; }
    static bool isDeletedValue(int value) { return value == -3; }
};

typedef HashMap<int, AtomicString, DefaultHash<int>::Hash, UScriptCodeHashTraits> ScriptFontFamilyMap;

class Settings {
    WTF_MAKE_NONCOPYABLE(Settings); WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<Settings> create(Page*);

    void setStandardFontFamily(const AtomicString&, UScriptCode = USCRIPT_COMMON);
    const AtomicString& standardFontFamily(UScriptCode = USCRIPT_COMMON) const;

    void setFixedFontFamily(const AtomicString&, UScriptCode = USCRIPT_COMMON);
    const AtomicString& fixedFontFamily(UScriptCode = USCRIPT_COMMON) const;

    void setSerifFontFamily(const AtomicString&, UScriptCode = USCRIPT_COMMON);
    const AtomicString& serifFontFamily(UScriptCode = USCRIPT_COMMON) const;

    void setSansSerifFontFamily(const AtomicString&, UScriptCode = USCRIPT_COMMON);
    const AtomicString& sansSerifFontFamily(UScriptCode = USCRIPT_COMMON) const;

    void setCursiveFontFamily(const AtomicString&, UScriptCode = USCRIPT_COMMON);
    const AtomicString& cursiveFontFamily(UScriptCode = USCRIPT_COMMON) const;

    void setFantasyFontFamily(const AtomicString&, UScriptCode = USCRIPT_COMMON);
    const AtomicString& fantasyFontFamily(UScriptCode = USCRIPT_COMMON) const;

    void setPictographFontFamily(const AtomicString&, UScriptCode = USCRIPT_COMMON);
    const AtomicString& pictographFontFamily(UScriptCode = USCRIPT_COMMON) const;

    void setTextAutosizingEnabled(bool);
    bool textAutosizingEnabled() const { return m_textAutosizingEnabled; }

    void setTextAutosizingFontScaleFactor(float);
    float textAutosizingFontScaleFactor() const { return m_textAutosizingFontScaleFactor; }

    // Only set by Layout Tests, and only used if textAutosizingEnabled() returns true.
    void setTextAutosizingWindowSizeOverride(const IntSize&);
    const IntSize& textAutosizingWindowSizeOverride() const { return m_textAutosizingWindowSizeOverride; }

    void setUseWideViewport(bool);
    bool useWideViewport() const { return m_useWideViewport; }

    void setLoadWithOverviewMode(bool);
    bool loadWithOverviewMode() const { return m_loadWithOverviewMode; }

    // Only set by Layout Tests.
    void setMediaTypeOverride(const String&);
    const String& mediaTypeOverride() const { return m_mediaTypeOverride; }

    // Unlike areImagesEnabled, this only suppresses the network load of
    // the image URL.  A cached image will still be rendered if requested.
    void setLoadsImagesAutomatically(bool);
    bool loadsImagesAutomatically() const { return m_loadsImagesAutomatically; }

    // Clients that execute script should call ScriptController::canExecuteScripts()
    // instead of this function. ScriptController::canExecuteScripts() checks the
    // HTML sandbox, plug-in sandboxing, and other important details.
    bool isScriptEnabled() const { return m_isScriptEnabled; }
    void setScriptEnabled(bool);

    SETTINGS_GETTERS_AND_SETTERS

    void setJavaEnabled(bool);
    bool isJavaEnabled() const { return m_isJavaEnabled; }

    void setImagesEnabled(bool);
    bool areImagesEnabled() const { return m_areImagesEnabled; }

    void setPluginsEnabled(bool);
    bool arePluginsEnabled() const { return m_arePluginsEnabled; }

    void setDNSPrefetchingEnabled(bool);
    bool dnsPrefetchingEnabled() const { return m_dnsPrefetchingEnabled; }

    void setUserStyleSheetLocation(const KURL&);
    const KURL& userStyleSheetLocation() const { return m_userStyleSheetLocation; }

    void setCSSCustomFilterEnabled(bool enabled) { m_isCSSCustomFilterEnabled = enabled; }
    bool isCSSCustomFilterEnabled() const { return m_isCSSCustomFilterEnabled; }

    void setCSSStickyPositionEnabled(bool enabled) { m_cssStickyPositionEnabled = enabled; }
    bool cssStickyPositionEnabled() const { return m_cssStickyPositionEnabled; }

    static void setMockScrollbarsEnabled(bool flag);
    static bool mockScrollbarsEnabled();

    void setTouchEventEmulationEnabled(bool enabled) { m_touchEventEmulationEnabled = enabled; }
    bool isTouchEventEmulationEnabled() const { return m_touchEventEmulationEnabled; }

    void setOpenGLMultisamplingEnabled(bool flag);
    bool openGLMultisamplingEnabled();

    void setViewportEnabled(bool);
    bool viewportEnabled() const { return m_viewportEnabled; }

    // FIXME: This is a temporary flag and should be removed once accelerated
    // overflow scroll is ready (crbug.com/254111).
    void setCompositorDrivenAcceleratedScrollingEnabled(bool enabled) { m_compositorDrivenAcceleratedScrollingEnabled = enabled; }
    bool isCompositorDrivenAcceleratedScrollingEnabled() const { return m_compositorDrivenAcceleratedScrollingEnabled; }

private:
    explicit Settings(Page*);

    Page* m_page;

    String m_mediaTypeOverride;
    KURL m_userStyleSheetLocation;
    ScriptFontFamilyMap m_standardFontFamilyMap;
    ScriptFontFamilyMap m_serifFontFamilyMap;
    ScriptFontFamilyMap m_fixedFontFamilyMap;
    ScriptFontFamilyMap m_sansSerifFontFamilyMap;
    ScriptFontFamilyMap m_cursiveFontFamilyMap;
    ScriptFontFamilyMap m_fantasyFontFamilyMap;
    ScriptFontFamilyMap m_pictographFontFamilyMap;
    float m_textAutosizingFontScaleFactor;
    IntSize m_textAutosizingWindowSizeOverride;
    bool m_textAutosizingEnabled : 1;
    bool m_useWideViewport : 1;
    bool m_loadWithOverviewMode : 1;

    SETTINGS_MEMBER_VARIABLES

    bool m_isJavaEnabled : 1;
    bool m_loadsImagesAutomatically : 1;
    bool m_areImagesEnabled : 1;
    bool m_arePluginsEnabled : 1;
    bool m_isScriptEnabled : 1;
    bool m_isCSSCustomFilterEnabled : 1;
    bool m_cssStickyPositionEnabled : 1;
    bool m_dnsPrefetchingEnabled : 1;

    bool m_touchEventEmulationEnabled : 1;
    bool m_openGLMultisamplingEnabled : 1;
    bool m_viewportEnabled : 1;

    // FIXME: This is a temporary flag and should be removed once accelerated
    // overflow scroll is ready (crbug.com/254111).
    bool m_compositorDrivenAcceleratedScrollingEnabled : 1;

    Timer<Settings> m_setImageLoadingSettingsTimer;
    void imageLoadingSettingsTimerFired(Timer<Settings>*);

    static bool gMockScrollbarsEnabled;
};

} // namespace WebCore

#endif // Settings_h
