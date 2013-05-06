/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef InternalSettings_h
#define InternalSettings_h

#include "InternalSettingsGenerated.h"
#include "core/editing/EditingBehaviorTypes.h"
#include "core/platform/graphics/IntSize.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

typedef int ExceptionCode;

class Frame;
class Document;
class Page;
class Settings;

class InternalSettings : public InternalSettingsGenerated {
public:
    class Backup {
    public:
        explicit Backup(Settings*);
        void restoreTo(Settings*);

        bool m_originalCSSExclusionsEnabled;
        bool m_originalCSSVariablesEnabled;
        bool m_originalAuthorShadowDOMForAnyElementEnabled;
        bool m_originalExperimentalShadowDOMEnabled;
        bool m_originalStyleScoped;
        EditingBehaviorType m_originalEditingBehavior;
        bool m_originalTextAutosizingEnabled;
        IntSize m_originalTextAutosizingWindowSizeOverride;
        float m_originalTextAutosizingFontScaleFactor;
        IntSize m_originalResolutionOverride;
        String m_originalMediaTypeOverride;
        bool m_originalDialogElementEnabled;
        bool m_originalLazyLayoutEnabled;
        bool m_originalMockScrollbarsEnabled;
        bool m_originalUsesOverlayScrollbars;
        bool m_langAttributeAwareFormControlUIEnabled;
        bool m_imagesEnabled;
        bool m_shouldDisplaySubtitles;
        bool m_shouldDisplayCaptions;
        bool m_shouldDisplayTextDescriptions;
        String m_defaultVideoPosterURL;
    };

    static PassRefPtr<InternalSettings> create(Page* page)
    {
        return adoptRef(new InternalSettings(page));
    }
    static InternalSettings* from(Page*);
    void hostDestroyed() { m_page = 0; }

    virtual ~InternalSettings();
    void resetToConsistentState();

    void setStandardFontFamily(const String& family, const String& script, ExceptionCode&);
    void setSerifFontFamily(const String& family, const String& script, ExceptionCode&);
    void setSansSerifFontFamily(const String& family, const String& script, ExceptionCode&);
    void setFixedFontFamily(const String& family, const String& script, ExceptionCode&);
    void setCursiveFontFamily(const String& family, const String& script, ExceptionCode&);
    void setFantasyFontFamily(const String& family, const String& script, ExceptionCode&);
    void setPictographFontFamily(const String& family, const String& script, ExceptionCode&);

    void setDefaultVideoPosterURL(const String& url, ExceptionCode&);
    void setEditingBehavior(const String&, ExceptionCode&);
    void setImagesEnabled(bool, ExceptionCode&);
    void setMediaTypeOverride(const String& mediaType, ExceptionCode&);
    void setMockScrollbarsEnabled(bool, ExceptionCode&);
    void setResolutionOverride(int dotsPerCSSInchHorizontally, int dotsPerCSSInchVertically, ExceptionCode&);
    void setTextAutosizingEnabled(bool, ExceptionCode&);
    void setTextAutosizingFontScaleFactor(float fontScaleFactor, ExceptionCode&);
    void setTextAutosizingWindowSizeOverride(int width, int height, ExceptionCode&);
    void setTouchEventEmulationEnabled(bool, ExceptionCode&);
    void setUsesOverlayScrollbars(bool, ExceptionCode&);

    void setShouldDisplayTrackKind(const String& kind, bool, ExceptionCode&);
    bool shouldDisplayTrackKind(const String& kind, ExceptionCode&);

    // cssVariablesEnabled is not backed by RuntimeEnabledFeatures, but should be.
    bool cssVariablesEnabled(ExceptionCode&);
    void setCSSVariablesEnabled(bool, ExceptionCode&);

    // FIXME: The following are RuntimeEnabledFeatures and likely
    // cannot be changed after process start.  These setters should
    // be removed or moved onto internals.runtimeFlags:
    void setAuthorShadowDOMForAnyElementEnabled(bool);
    void setCSSExclusionsEnabled(bool);
    void setDialogElementEnabled(bool);
    void setExperimentalShadowDOMEnabled(bool);
    void setLangAttributeAwareFormControlUIEnabled(bool);
    void setLazyLayoutEnabled(bool);
    void setStyleScopedEnabled(bool);

private:
    explicit InternalSettings(Page*);

    Settings* settings() const;
    Page* page() const { return m_page; }
    static const char* supplementName();

    Page* m_page;
    Backup m_backup;
};

} // namespace WebCore

#endif
