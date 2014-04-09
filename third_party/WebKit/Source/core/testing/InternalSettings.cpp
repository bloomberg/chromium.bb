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
#include "core/frame/Settings.h"
#include "core/inspector/InspectorController.h"
#include "core/page/Page.h"
#include "platform/ColorChooser.h"
#include "platform/Supplementable.h"
#include "platform/text/LocaleToScriptMapping.h"

#define InternalSettingsGuardForSettingsReturn(returnValue) \
    if (!settings()) { \
        exceptionState.throwDOMException(InvalidAccessError, "The settings object cannot be obtained."); \
        return returnValue; \
    }

#define InternalSettingsGuardForSettings()  \
    if (!settings()) { \
        exceptionState.throwDOMException(InvalidAccessError, "The settings object cannot be obtained."); \
        return; \
    }

#define InternalSettingsGuardForPage() \
    if (!page()) { \
        exceptionState.throwDOMException(InvalidAccessError, "The page object cannot be obtained."); \
        return; \
    }

namespace WebCore {

InternalSettings::Backup::Backup(Settings* settings)
    : m_originalCSSExclusionsEnabled(RuntimeEnabledFeatures::cssExclusionsEnabled())
    , m_originalAuthorShadowDOMForAnyElementEnabled(RuntimeEnabledFeatures::authorShadowDOMForAnyElementEnabled())
    , m_originalStyleScoped(RuntimeEnabledFeatures::styleScopedEnabled())
    , m_originalCSP(RuntimeEnabledFeatures::experimentalContentSecurityPolicyFeaturesEnabled())
    , m_originalOverlayScrollbarsEnabled(RuntimeEnabledFeatures::overlayScrollbarsEnabled())
    , m_originalEditingBehavior(settings->editingBehaviorType())
    , m_originalTextAutosizingEnabled(settings->textAutosizingEnabled())
    , m_originalTextAutosizingWindowSizeOverride(settings->textAutosizingWindowSizeOverride())
    , m_originalAccessibilityFontScaleFactor(settings->accessibilityFontScaleFactor())
    , m_originalMediaTypeOverride(settings->mediaTypeOverride())
    , m_originalMockScrollbarsEnabled(settings->mockScrollbarsEnabled())
    , m_langAttributeAwareFormControlUIEnabled(RuntimeEnabledFeatures::langAttributeAwareFormControlUIEnabled())
    , m_imagesEnabled(settings->imagesEnabled())
    , m_shouldDisplaySubtitles(settings->shouldDisplaySubtitles())
    , m_shouldDisplayCaptions(settings->shouldDisplayCaptions())
    , m_shouldDisplayTextDescriptions(settings->shouldDisplayTextDescriptions())
    , m_defaultVideoPosterURL(settings->defaultVideoPosterURL())
    , m_originalCompositorDrivenAcceleratedScrollEnabled(settings->compositorDrivenAcceleratedScrollingEnabled())
    , m_originalLayerSquashingEnabled(settings->layerSquashingEnabled())
    , m_originalPasswordGenerationDecorationEnabled(settings->passwordGenerationDecorationEnabled())
{
}

void InternalSettings::Backup::restoreTo(Settings* settings)
{
    RuntimeEnabledFeatures::setCSSExclusionsEnabled(m_originalCSSExclusionsEnabled);
    RuntimeEnabledFeatures::setAuthorShadowDOMForAnyElementEnabled(m_originalAuthorShadowDOMForAnyElementEnabled);
    RuntimeEnabledFeatures::setStyleScopedEnabled(m_originalStyleScoped);
    RuntimeEnabledFeatures::setExperimentalContentSecurityPolicyFeaturesEnabled(m_originalCSP);
    RuntimeEnabledFeatures::setOverlayScrollbarsEnabled(m_originalOverlayScrollbarsEnabled);
    settings->setEditingBehaviorType(m_originalEditingBehavior);
    settings->setTextAutosizingEnabled(m_originalTextAutosizingEnabled);
    settings->setTextAutosizingWindowSizeOverride(m_originalTextAutosizingWindowSizeOverride);
    settings->setAccessibilityFontScaleFactor(m_originalAccessibilityFontScaleFactor);
    settings->setMediaTypeOverride(m_originalMediaTypeOverride);
    settings->setMockScrollbarsEnabled(m_originalMockScrollbarsEnabled);
    RuntimeEnabledFeatures::setLangAttributeAwareFormControlUIEnabled(m_langAttributeAwareFormControlUIEnabled);
    settings->setImagesEnabled(m_imagesEnabled);
    settings->setShouldDisplaySubtitles(m_shouldDisplaySubtitles);
    settings->setShouldDisplayCaptions(m_shouldDisplayCaptions);
    settings->setShouldDisplayTextDescriptions(m_shouldDisplayTextDescriptions);
    settings->setDefaultVideoPosterURL(m_defaultVideoPosterURL);
    settings->setCompositorDrivenAcceleratedScrollingEnabled(m_originalCompositorDrivenAcceleratedScrollEnabled);
    settings->setLayerSquashingEnabled(m_originalLayerSquashingEnabled);
    settings->setPasswordGenerationDecorationEnabled(m_originalPasswordGenerationDecorationEnabled);
    settings->genericFontFamilySettings().reset();
}

// We can't use RefCountedSupplement because that would try to make InternalSettings RefCounted
// and InternalSettings is already RefCounted via its base class, InternalSettingsGenerated.
// Instead, we manually make InternalSettings supplement Page.
class InternalSettingsWrapper : public Supplement<Page> {
public:
    explicit InternalSettingsWrapper(Page& page)
        : m_internalSettings(InternalSettings::create(page)) { }
    virtual ~InternalSettingsWrapper() { m_internalSettings->hostDestroyed(); }
#if !ASSERT_DISABLED
    virtual bool isRefCountedWrapper() const OVERRIDE { return true; }
#endif
    InternalSettings* internalSettings() const { return m_internalSettings.get(); }

    virtual void trace(Visitor*) OVERRIDE
    {
        // FIXME: Oilpan: Move Page to the managed heap before using this trace method.
    }

private:
    RefPtrWillBePersistent<InternalSettings> m_internalSettings;
};

const char* InternalSettings::supplementName()
{
    return "InternalSettings";
}

InternalSettings* InternalSettings::from(Page& page)
{
    if (!Supplement<Page>::from(page, supplementName()))
        Supplement<Page>::provideTo(page, supplementName(), adoptPtr(new InternalSettingsWrapper(page)));
    return static_cast<InternalSettingsWrapper*>(Supplement<Page>::from(page, supplementName()))->internalSettings();
}

InternalSettings::~InternalSettings()
{
}

InternalSettings::InternalSettings(Page& page)
    : InternalSettingsGenerated(&page)
    , m_page(&page)
    , m_backup(&page.settings())
{
}

void InternalSettings::resetToConsistentState()
{
    page()->setPageScaleFactor(1, IntPoint(0, 0));

    m_backup.restoreTo(settings());
    m_backup = Backup(settings());
    m_backup.m_originalTextAutosizingEnabled = settings()->textAutosizingEnabled();

    InternalSettingsGenerated::resetToConsistentState();
}

Settings* InternalSettings::settings() const
{
    if (!page())
        return 0;
    return &page()->settings();
}

void InternalSettings::setMockScrollbarsEnabled(bool enabled, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    settings()->setMockScrollbarsEnabled(enabled);
}

void InternalSettings::setAuthorShadowDOMForAnyElementEnabled(bool isEnabled)
{
    RuntimeEnabledFeatures::setAuthorShadowDOMForAnyElementEnabled(isEnabled);
}

void InternalSettings::setStyleScopedEnabled(bool enabled)
{
    RuntimeEnabledFeatures::setStyleScopedEnabled(enabled);
}

void InternalSettings::setExperimentalContentSecurityPolicyFeaturesEnabled(bool enabled)
{
    RuntimeEnabledFeatures::setExperimentalContentSecurityPolicyFeaturesEnabled(enabled);
}

void InternalSettings::setOverlayScrollbarsEnabled(bool enabled)
{
    RuntimeEnabledFeatures::setOverlayScrollbarsEnabled(enabled);
}

void InternalSettings::setTouchEventEmulationEnabled(bool enabled, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    settings()->setTouchEventEmulationEnabled(enabled);
}

void InternalSettings::setViewportEnabled(bool enabled, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    settings()->setViewportEnabled(enabled);
}

// FIXME: This is a temporary flag and should be removed once accelerated
// overflow scroll is ready (crbug.com/254111).
void InternalSettings::setCompositorDrivenAcceleratedScrollingEnabled(bool enabled, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    settings()->setCompositorDrivenAcceleratedScrollingEnabled(enabled);
}

// FIXME: This is a temporary flag and should be removed once squashing is
// ready (crbug.com/261605).
void InternalSettings::setLayerSquashingEnabled(bool enabled, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    settings()->setLayerSquashingEnabled(enabled);
}

void InternalSettings::setStandardFontFamily(const AtomicString& family, const String& script, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    UScriptCode code = scriptNameToCode(script);
    if (code == USCRIPT_INVALID_CODE)
        return;
    settings()->genericFontFamilySettings().setStandard(family, code);
    settings()->notifyGenericFontFamilyChange();
}

void InternalSettings::setSerifFontFamily(const AtomicString& family, const String& script, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    UScriptCode code = scriptNameToCode(script);
    if (code == USCRIPT_INVALID_CODE)
        return;
    settings()->genericFontFamilySettings().setSerif(family, code);
    settings()->notifyGenericFontFamilyChange();
}

void InternalSettings::setSansSerifFontFamily(const AtomicString& family, const String& script, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    UScriptCode code = scriptNameToCode(script);
    if (code == USCRIPT_INVALID_CODE)
        return;
    settings()->genericFontFamilySettings().setSansSerif(family, code);
    settings()->notifyGenericFontFamilyChange();
}

void InternalSettings::setFixedFontFamily(const AtomicString& family, const String& script, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    UScriptCode code = scriptNameToCode(script);
    if (code == USCRIPT_INVALID_CODE)
        return;
    settings()->genericFontFamilySettings().setFixed(family, code);
    settings()->notifyGenericFontFamilyChange();
}

void InternalSettings::setCursiveFontFamily(const AtomicString& family, const String& script, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    UScriptCode code = scriptNameToCode(script);
    if (code == USCRIPT_INVALID_CODE)
        return;
    settings()->genericFontFamilySettings().setCursive(family, code);
    settings()->notifyGenericFontFamilyChange();
}

void InternalSettings::setFantasyFontFamily(const AtomicString& family, const String& script, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    UScriptCode code = scriptNameToCode(script);
    if (code == USCRIPT_INVALID_CODE)
        return;
    settings()->genericFontFamilySettings().setFantasy(family, code);
    settings()->notifyGenericFontFamilyChange();
}

void InternalSettings::setPictographFontFamily(const AtomicString& family, const String& script, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    UScriptCode code = scriptNameToCode(script);
    if (code == USCRIPT_INVALID_CODE)
        return;
    settings()->genericFontFamilySettings().setPictograph(family, code);
    settings()->notifyGenericFontFamilyChange();
}

void InternalSettings::setTextAutosizingEnabled(bool enabled, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    settings()->setTextAutosizingEnabled(enabled);
    m_page->inspectorController().setTextAutosizingEnabled(enabled);
}

void InternalSettings::setTextAutosizingWindowSizeOverride(int width, int height, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    settings()->setTextAutosizingWindowSizeOverride(IntSize(width, height));
}

void InternalSettings::setMediaTypeOverride(const String& mediaType, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    settings()->setMediaTypeOverride(mediaType);
}

void InternalSettings::setAccessibilityFontScaleFactor(float fontScaleFactor, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    settings()->setAccessibilityFontScaleFactor(fontScaleFactor);
}

void InternalSettings::setCSSExclusionsEnabled(bool enabled)
{
    RuntimeEnabledFeatures::setCSSExclusionsEnabled(enabled);
}

void InternalSettings::setEditingBehavior(const String& editingBehavior, ExceptionState& exceptionState)
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
        exceptionState.throwDOMException(SyntaxError, "The editing behavior type provided ('" + editingBehavior + "') is invalid.");
}

void InternalSettings::setLangAttributeAwareFormControlUIEnabled(bool enabled)
{
    RuntimeEnabledFeatures::setLangAttributeAwareFormControlUIEnabled(enabled);
}

void InternalSettings::setImagesEnabled(bool enabled, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    settings()->setImagesEnabled(enabled);
}

void InternalSettings::setDefaultVideoPosterURL(const String& url, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    settings()->setDefaultVideoPosterURL(url);
}

void InternalSettings::setPasswordGenerationDecorationEnabled(bool enabled, ExceptionState& exceptionState)
{
    InternalSettingsGuardForSettings();
    settings()->setPasswordGenerationDecorationEnabled(enabled);
}

}
