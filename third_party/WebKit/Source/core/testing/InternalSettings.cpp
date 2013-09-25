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

#include "RuntimeEnabledFeatures.h"
#include "bindings/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/page/Page.h"
#include "core/page/Settings.h"
#include "core/platform/ColorChooser.h"
#include "core/platform/Supplementable.h"
#include "core/platform/text/LocaleToScriptMapping.h"

#define InternalSettingsGuardForSettingsReturn(returnValue) \
    if (!settings()) { \
        es.throwUninformativeAndGenericDOMException(InvalidAccessError); \
        return returnValue; \
    }

#define InternalSettingsGuardForSettings()  \
    if (!settings()) { \
        es.throwUninformativeAndGenericDOMException(InvalidAccessError); \
        return; \
    }

#define InternalSettingsGuardForPage() \
    if (!page()) { \
        es.throwUninformativeAndGenericDOMException(InvalidAccessError); \
        return; \
    }

namespace WebCore {

InternalSettings::Backup::Backup(Settings* settings)
    : m_originalCSSExclusionsEnabled(RuntimeEnabledFeatures::cssExclusionsEnabled())
    , m_originalAuthorShadowDOMForAnyElementEnabled(RuntimeEnabledFeatures::authorShadowDOMForAnyElementEnabled())
    , m_originalExperimentalWebSocketEnabled(settings->experimentalWebSocketEnabled())
    , m_originalStyleScoped(RuntimeEnabledFeatures::styleScopedEnabled())
    , m_originalOverlayScrollbarsEnabled(RuntimeEnabledFeatures::overlayScrollbarsEnabled())
    , m_originalEditingBehavior(settings->editingBehaviorType())
    , m_originalTextAutosizingEnabled(settings->textAutosizingEnabled())
    , m_originalTextAutosizingWindowSizeOverride(settings->textAutosizingWindowSizeOverride())
    , m_originalTextAutosizingFontScaleFactor(settings->textAutosizingFontScaleFactor())
    , m_originalMediaTypeOverride(settings->mediaTypeOverride())
    , m_originalMockScrollbarsEnabled(settings->mockScrollbarsEnabled())
    , m_langAttributeAwareFormControlUIEnabled(RuntimeEnabledFeatures::langAttributeAwareFormControlUIEnabled())
    , m_imagesEnabled(settings->areImagesEnabled())
    , m_shouldDisplaySubtitles(settings->shouldDisplaySubtitles())
    , m_shouldDisplayCaptions(settings->shouldDisplayCaptions())
    , m_shouldDisplayTextDescriptions(settings->shouldDisplayTextDescriptions())
    , m_defaultVideoPosterURL(settings->defaultVideoPosterURL())
    , m_originalCompositorDrivenAcceleratedScrollEnabled(settings->isCompositorDrivenAcceleratedScrollingEnabled())
{
}

void InternalSettings::Backup::restoreTo(Settings* settings)
{
    RuntimeEnabledFeatures::setCSSExclusionsEnabled(m_originalCSSExclusionsEnabled);
    RuntimeEnabledFeatures::setAuthorShadowDOMForAnyElementEnabled(m_originalAuthorShadowDOMForAnyElementEnabled);
    settings->setExperimentalWebSocketEnabled(m_originalExperimentalWebSocketEnabled);
    RuntimeEnabledFeatures::setStyleScopedEnabled(m_originalStyleScoped);
    RuntimeEnabledFeatures::setOverlayScrollbarsEnabled(m_originalOverlayScrollbarsEnabled);
    settings->setEditingBehaviorType(m_originalEditingBehavior);
    settings->setTextAutosizingEnabled(m_originalTextAutosizingEnabled);
    settings->setTextAutosizingWindowSizeOverride(m_originalTextAutosizingWindowSizeOverride);
    settings->setTextAutosizingFontScaleFactor(m_originalTextAutosizingFontScaleFactor);
    settings->setMediaTypeOverride(m_originalMediaTypeOverride);
    settings->setMockScrollbarsEnabled(m_originalMockScrollbarsEnabled);
    RuntimeEnabledFeatures::setLangAttributeAwareFormControlUIEnabled(m_langAttributeAwareFormControlUIEnabled);
    settings->setImagesEnabled(m_imagesEnabled);
    settings->setShouldDisplaySubtitles(m_shouldDisplaySubtitles);
    settings->setShouldDisplayCaptions(m_shouldDisplayCaptions);
    settings->setShouldDisplayTextDescriptions(m_shouldDisplayTextDescriptions);
    settings->setDefaultVideoPosterURL(m_defaultVideoPosterURL);
    settings->setCompositorDrivenAcceleratedScrollingEnabled(m_originalCompositorDrivenAcceleratedScrollEnabled);
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
    , m_backup(&page->settings())
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
    return &page()->settings();
}

void InternalSettings::setMockScrollbarsEnabled(bool enabled, ExceptionState& es)
{
    InternalSettingsGuardForSettings();
    settings()->setMockScrollbarsEnabled(enabled);
}

void InternalSettings::setAuthorShadowDOMForAnyElementEnabled(bool isEnabled)
{
    RuntimeEnabledFeatures::setAuthorShadowDOMForAnyElementEnabled(isEnabled);
}

void InternalSettings::setExperimentalWebSocketEnabled(bool isEnabled)
{
    settings()->setExperimentalWebSocketEnabled(isEnabled);
}

void InternalSettings::setStyleScopedEnabled(bool enabled)
{
    RuntimeEnabledFeatures::setStyleScopedEnabled(enabled);
}

void InternalSettings::setOverlayScrollbarsEnabled(bool enabled)
{
    RuntimeEnabledFeatures::setOverlayScrollbarsEnabled(enabled);
}

void InternalSettings::setTouchEventEmulationEnabled(bool enabled, ExceptionState& es)
{
    InternalSettingsGuardForSettings();
    settings()->setTouchEventEmulationEnabled(enabled);
}

void InternalSettings::setViewportEnabled(bool enabled, ExceptionState& es)
{
    InternalSettingsGuardForSettings();
    settings()->setViewportEnabled(enabled);
}

// FIXME: This is a temporary flag and should be removed once accelerated
// overflow scroll is ready (crbug.com/254111).
void InternalSettings::setCompositorDrivenAcceleratedScrollingEnabled(bool enabled, ExceptionState& es)
{
    InternalSettingsGuardForSettings();
    settings()->setCompositorDrivenAcceleratedScrollingEnabled(enabled);
}

typedef void (Settings::*SetFontFamilyFunction)(const AtomicString&, UScriptCode);
static void setFontFamily(Settings* settings, const String& family, const String& script, SetFontFamilyFunction setter)
{
    UScriptCode code = scriptNameToCode(script);
    if (code != USCRIPT_INVALID_CODE)
        (settings->*setter)(family, code);
}

void InternalSettings::setStandardFontFamily(const String& family, const String& script, ExceptionState& es)
{
    InternalSettingsGuardForSettings();
    setFontFamily(settings(), family, script, &Settings::setStandardFontFamily);
}

void InternalSettings::setSerifFontFamily(const String& family, const String& script, ExceptionState& es)
{
    InternalSettingsGuardForSettings();
    setFontFamily(settings(), family, script, &Settings::setSerifFontFamily);
}

void InternalSettings::setSansSerifFontFamily(const String& family, const String& script, ExceptionState& es)
{
    InternalSettingsGuardForSettings();
    setFontFamily(settings(), family, script, &Settings::setSansSerifFontFamily);
}

void InternalSettings::setFixedFontFamily(const String& family, const String& script, ExceptionState& es)
{
    InternalSettingsGuardForSettings();
    setFontFamily(settings(), family, script, &Settings::setFixedFontFamily);
}

void InternalSettings::setCursiveFontFamily(const String& family, const String& script, ExceptionState& es)
{
    InternalSettingsGuardForSettings();
    setFontFamily(settings(), family, script, &Settings::setCursiveFontFamily);
}

void InternalSettings::setFantasyFontFamily(const String& family, const String& script, ExceptionState& es)
{
    InternalSettingsGuardForSettings();
    setFontFamily(settings(), family, script, &Settings::setFantasyFontFamily);
}

void InternalSettings::setPictographFontFamily(const String& family, const String& script, ExceptionState& es)
{
    InternalSettingsGuardForSettings();
    setFontFamily(settings(), family, script, &Settings::setPictographFontFamily);
}

void InternalSettings::setTextAutosizingEnabled(bool enabled, ExceptionState& es)
{
    InternalSettingsGuardForSettings();
    settings()->setTextAutosizingEnabled(enabled);
}

void InternalSettings::setTextAutosizingWindowSizeOverride(int width, int height, ExceptionState& es)
{
    InternalSettingsGuardForSettings();
    settings()->setTextAutosizingWindowSizeOverride(IntSize(width, height));
}

void InternalSettings::setMediaTypeOverride(const String& mediaType, ExceptionState& es)
{
    InternalSettingsGuardForSettings();
    settings()->setMediaTypeOverride(mediaType);
}

void InternalSettings::setTextAutosizingFontScaleFactor(float fontScaleFactor, ExceptionState& es)
{
    InternalSettingsGuardForSettings();
    settings()->setTextAutosizingFontScaleFactor(fontScaleFactor);
}

void InternalSettings::setCSSExclusionsEnabled(bool enabled)
{
    RuntimeEnabledFeatures::setCSSExclusionsEnabled(enabled);
}

void InternalSettings::setEditingBehavior(const String& editingBehavior, ExceptionState& es)
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
        es.throwUninformativeAndGenericDOMException(SyntaxError);
}

void InternalSettings::setLangAttributeAwareFormControlUIEnabled(bool enabled)
{
    RuntimeEnabledFeatures::setLangAttributeAwareFormControlUIEnabled(enabled);
}

void InternalSettings::setImagesEnabled(bool enabled, ExceptionState& es)
{
    InternalSettingsGuardForSettings();
    settings()->setImagesEnabled(enabled);
}

void InternalSettings::setDefaultVideoPosterURL(const String& url, ExceptionState& es)
{
    InternalSettingsGuardForSettings();
    settings()->setDefaultVideoPosterURL(url);
}

}
