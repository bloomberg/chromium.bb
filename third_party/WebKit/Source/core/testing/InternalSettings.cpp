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

#include "config.h"
#include "InternalSettings.h"

#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/page/CaptionUserPreferences.h"
#include "core/page/Frame.h"
#include "core/page/FrameView.h"
#include "core/page/Page.h"
#include "core/page/PageGroup.h"
#include "RuntimeEnabledFeatures.h"
#include "core/page/Settings.h"
#include "core/platform/Language.h"
#include "core/platform/Supplementable.h"
#include "core/platform/graphics/TextRun.h"
#include "core/platform/text/LocaleToScriptMapping.h"

#if ENABLE(INPUT_TYPE_COLOR)
#include "core/platform/ColorChooser.h"
#endif

#define InternalSettingsGuardForSettingsReturn(returnValue) \
    if (!settings()) { \
        ec = INVALID_ACCESS_ERR; \
        return returnValue; \
    }

#define InternalSettingsGuardForSettings()  \
    if (!settings()) { \
        ec = INVALID_ACCESS_ERR; \
        return; \
    }

#define InternalSettingsGuardForPage() \
    if (!page()) { \
        ec = INVALID_ACCESS_ERR; \
        return; \
    }

namespace WebCore {

InternalSettings::Backup::Backup(Settings* settings)
    : m_originalCSSExclusionsEnabled(RuntimeEnabledFeatures::cssExclusionsEnabled())
    , m_originalCSSVariablesEnabled(settings->cssVariablesEnabled())
    , m_originalAuthorShadowDOMForAnyElementEnabled(RuntimeEnabledFeatures::authorShadowDOMForAnyElementEnabled())
    , m_originalExperimentalShadowDOMEnabled(RuntimeEnabledFeatures::experimentalShadowDOMEnabled())
    , m_originalStyleScoped(RuntimeEnabledFeatures::styleScopedEnabled())
    , m_originalEditingBehavior(settings->editingBehaviorType())
    , m_originalTextAutosizingEnabled(settings->textAutosizingEnabled())
    , m_originalTextAutosizingWindowSizeOverride(settings->textAutosizingWindowSizeOverride())
    , m_originalTextAutosizingFontScaleFactor(settings->textAutosizingFontScaleFactor())
    , m_originalResolutionOverride(settings->resolutionOverride())
    , m_originalMediaTypeOverride(settings->mediaTypeOverride())
    , m_originalDialogElementEnabled(RuntimeEnabledFeatures::dialogElementEnabled())
    , m_originalLazyLayoutEnabled(RuntimeEnabledFeatures::lazyLayoutEnabled())
    , m_originalMockScrollbarsEnabled(settings->mockScrollbarsEnabled())
    , m_langAttributeAwareFormControlUIEnabled(RuntimeEnabledFeatures::langAttributeAwareFormControlUIEnabled())
    , m_imagesEnabled(settings->areImagesEnabled())
    , m_shouldDisplaySubtitles(settings->shouldDisplaySubtitles())
    , m_shouldDisplayCaptions(settings->shouldDisplayCaptions())
    , m_shouldDisplayTextDescriptions(settings->shouldDisplayTextDescriptions())
    , m_defaultVideoPosterURL(settings->defaultVideoPosterURL())
{
}

void InternalSettings::Backup::restoreTo(Settings* settings)
{
    RuntimeEnabledFeatures::setCSSExclusionsEnabled(m_originalCSSExclusionsEnabled);
    settings->setCSSVariablesEnabled(m_originalCSSVariablesEnabled);
    RuntimeEnabledFeatures::setAuthorShadowDOMForAnyElementEnabled(m_originalAuthorShadowDOMForAnyElementEnabled);
    RuntimeEnabledFeatures::setExperimentalShadowDOMEnabled(m_originalExperimentalShadowDOMEnabled);
    RuntimeEnabledFeatures::setStyleScopedEnabled(m_originalStyleScoped);
    settings->setEditingBehaviorType(m_originalEditingBehavior);
    settings->setTextAutosizingEnabled(m_originalTextAutosizingEnabled);
    settings->setTextAutosizingWindowSizeOverride(m_originalTextAutosizingWindowSizeOverride);
    settings->setTextAutosizingFontScaleFactor(m_originalTextAutosizingFontScaleFactor);
    settings->setResolutionOverride(m_originalResolutionOverride);
    settings->setMediaTypeOverride(m_originalMediaTypeOverride);
    RuntimeEnabledFeatures::setDialogElementEnabled(m_originalDialogElementEnabled);
    RuntimeEnabledFeatures::setLazyLayoutEnabled(m_originalLazyLayoutEnabled);
    settings->setMockScrollbarsEnabled(m_originalMockScrollbarsEnabled);
    RuntimeEnabledFeatures::setLangAttributeAwareFormControlUIEnabled(m_langAttributeAwareFormControlUIEnabled);
    settings->setImagesEnabled(m_imagesEnabled);
    settings->setShouldDisplaySubtitles(m_shouldDisplaySubtitles);
    settings->setShouldDisplayCaptions(m_shouldDisplayCaptions);
    settings->setShouldDisplayTextDescriptions(m_shouldDisplayTextDescriptions);
    settings->setDefaultVideoPosterURL(m_defaultVideoPosterURL);
}

// We can't use RefCountedSupplement because that would try to make InternalSettings RefCounted
// and InternalSettings is already RefCounted via its base class, InternalSettingsGenerated.
// Instead, we manually make InternalSettings supplement Page.
class InternalSettingsWrapper : public Supplement<Page> {
public:
    explicit InternalSettingsWrapper(Page* page)
        : m_internalSettings(InternalSettings::create(page)) { }
    virtual ~InternalSettingsWrapper() { m_internalSettings->hostDestroyed(); }
#if !ASSERT_DISABLED
    virtual bool isRefCountedWrapper() const OVERRIDE { return true; }
#endif
    InternalSettings* internalSettings() const { return m_internalSettings.get(); }

private:
    RefPtr<InternalSettings> m_internalSettings;
};

const char* InternalSettings::supplementName()
{
    return "InternalSettings";
}

InternalSettings* InternalSettings::from(Page* page)
{
    if (!Supplement<Page>::from(page, supplementName()))
        Supplement<Page>::provideTo(page, supplementName(), adoptPtr(new InternalSettingsWrapper(page)));
    return static_cast<InternalSettingsWrapper*>(Supplement<Page>::from(page, supplementName()))->internalSettings();
}

InternalSettings::~InternalSettings()
{
}

InternalSettings::InternalSettings(Page* page)
    : InternalSettingsGenerated(page)
    , m_page(page)
    , m_backup(page->settings())
{
}

void InternalSettings::resetToConsistentState()
{
    page()->setPageScaleFactor(1, IntPoint(0, 0));

    m_backup.restoreTo(settings());
    m_backup = Backup(settings());

    InternalSettingsGenerated::resetToConsistentState();
}

Settings* InternalSettings::settings() const
{
    if (!page())
        return 0;
    return page()->settings();
}

void InternalSettings::setMockScrollbarsEnabled(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setMockScrollbarsEnabled(enabled);
}

void InternalSettings::setAuthorShadowDOMForAnyElementEnabled(bool isEnabled)
{
    RuntimeEnabledFeatures::setAuthorShadowDOMForAnyElementEnabled(isEnabled);
}

void InternalSettings::setExperimentalShadowDOMEnabled(bool isEnabled)
{
    RuntimeEnabledFeatures::setExperimentalShadowDOMEnabled(isEnabled);
}

void InternalSettings::setStyleScopedEnabled(bool enabled)
{
    RuntimeEnabledFeatures::setStyleScopedEnabled(enabled);
}

void InternalSettings::setTouchEventEmulationEnabled(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setTouchEventEmulationEnabled(enabled);
}

typedef void (Settings::*SetFontFamilyFunction)(const AtomicString&, UScriptCode);
static void setFontFamily(Settings* settings, const String& family, const String& script, SetFontFamilyFunction setter)
{
    UScriptCode code = scriptNameToCode(script);
    if (code != USCRIPT_INVALID_CODE)
        (settings->*setter)(family, code);
}

void InternalSettings::setStandardFontFamily(const String& family, const String& script, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    setFontFamily(settings(), family, script, &Settings::setStandardFontFamily);
}

void InternalSettings::setSerifFontFamily(const String& family, const String& script, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    setFontFamily(settings(), family, script, &Settings::setSerifFontFamily);
}

void InternalSettings::setSansSerifFontFamily(const String& family, const String& script, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    setFontFamily(settings(), family, script, &Settings::setSansSerifFontFamily);
}

void InternalSettings::setFixedFontFamily(const String& family, const String& script, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    setFontFamily(settings(), family, script, &Settings::setFixedFontFamily);
}

void InternalSettings::setCursiveFontFamily(const String& family, const String& script, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    setFontFamily(settings(), family, script, &Settings::setCursiveFontFamily);
}

void InternalSettings::setFantasyFontFamily(const String& family, const String& script, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    setFontFamily(settings(), family, script, &Settings::setFantasyFontFamily);
}

void InternalSettings::setPictographFontFamily(const String& family, const String& script, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    setFontFamily(settings(), family, script, &Settings::setPictographFontFamily);
}

void InternalSettings::setTextAutosizingEnabled(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setTextAutosizingEnabled(enabled);
}

void InternalSettings::setTextAutosizingWindowSizeOverride(int width, int height, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setTextAutosizingWindowSizeOverride(IntSize(width, height));
}

void InternalSettings::setResolutionOverride(int dotsPerCSSInchHorizontally, int dotsPerCSSInchVertically, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    // An empty size resets the override.
    settings()->setResolutionOverride(IntSize(dotsPerCSSInchHorizontally, dotsPerCSSInchVertically));
}

void InternalSettings::setMediaTypeOverride(const String& mediaType, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setMediaTypeOverride(mediaType);
}

void InternalSettings::setTextAutosizingFontScaleFactor(float fontScaleFactor, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setTextAutosizingFontScaleFactor(fontScaleFactor);
}

void InternalSettings::setCSSExclusionsEnabled(bool enabled)
{
    RuntimeEnabledFeatures::setCSSExclusionsEnabled(enabled);
}

void InternalSettings::setCSSVariablesEnabled(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setCSSVariablesEnabled(enabled);
}

bool InternalSettings::cssVariablesEnabled(ExceptionCode& ec)
{
    InternalSettingsGuardForSettingsReturn(false);
    return settings()->cssVariablesEnabled();
}

void InternalSettings::setEditingBehavior(const String& editingBehavior, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    if (equalIgnoringCase(editingBehavior, "win"))
        settings()->setEditingBehaviorType(EditingWindowsBehavior);
    else if (equalIgnoringCase(editingBehavior, "mac"))
        settings()->setEditingBehaviorType(EditingMacBehavior);
    else if (equalIgnoringCase(editingBehavior, "unix"))
        settings()->setEditingBehaviorType(EditingUnixBehavior);
    else if (equalIgnoringCase(editingBehavior, "android"))
        settings()->setEditingBehaviorType(EditingAndroidBehavior);
    else
        ec = SYNTAX_ERR;
}

void InternalSettings::setDialogElementEnabled(bool enabled)
{
    RuntimeEnabledFeatures::setDialogElementEnabled(enabled);
}

void InternalSettings::setLazyLayoutEnabled(bool enabled)
{
    RuntimeEnabledFeatures::setLazyLayoutEnabled(enabled);
}

void InternalSettings::setShouldDisplayTrackKind(const String& kind, bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();

    if (!page())
        return;
    CaptionUserPreferences* captionPreferences = page()->group().captionPreferences();

    if (equalIgnoringCase(kind, "Subtitles"))
        captionPreferences->setUserPrefersSubtitles(enabled);
    else if (equalIgnoringCase(kind, "Captions"))
        captionPreferences->setUserPrefersCaptions(enabled);
    else if (equalIgnoringCase(kind, "TextDescriptions"))
        captionPreferences->setUserPrefersTextDescriptions(enabled);
    else
        ec = SYNTAX_ERR;
}

bool InternalSettings::shouldDisplayTrackKind(const String& kind, ExceptionCode& ec)
{
    InternalSettingsGuardForSettingsReturn(false);

    if (!page())
        return false;
    CaptionUserPreferences* captionPreferences = page()->group().captionPreferences();

    if (equalIgnoringCase(kind, "Subtitles"))
        return captionPreferences->userPrefersSubtitles();
    if (equalIgnoringCase(kind, "Captions"))
        return captionPreferences->userPrefersCaptions();
    if (equalIgnoringCase(kind, "TextDescriptions"))
        return captionPreferences->userPrefersTextDescriptions();

    ec = SYNTAX_ERR;
    return false;
}

void InternalSettings::setLangAttributeAwareFormControlUIEnabled(bool enabled)
{
    RuntimeEnabledFeatures::setLangAttributeAwareFormControlUIEnabled(enabled);
}

void InternalSettings::setImagesEnabled(bool enabled, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setImagesEnabled(enabled);
}

void InternalSettings::setDefaultVideoPosterURL(const String& url, ExceptionCode& ec)
{
    InternalSettingsGuardForSettings();
    settings()->setDefaultVideoPosterURL(url);
}

}
