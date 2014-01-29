/*
 * Copyright (C) 2012 Google, Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY GOOGLE INC. ``AS IS'' AND ANY
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

#ifndef UseCounter_h
#define UseCounter_h

#include "CSSPropertyNames.h"
#include "wtf/BitVector.h"
#include "wtf/Noncopyable.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/text/WTFString.h"

namespace WebCore {

class CSSStyleSheet;
class DOMWindow;
class Document;
class ExecutionContext;
class StyleSheetContents;

// UseCounter is used for counting the number of times features of
// Blink are used on real web pages and help us know commonly
// features are used and thus when it's safe to remove or change them.
//
// The Chromium Content layer controls what is done with this data.
// For instance, in Google Chrome, these counts are submitted
// anonymously through the Histogram recording system in Chrome
// for users who opt-in to "Usage Statistics" submission
// during their install of Google Chrome:
// http://www.google.com/chrome/intl/en/privacy.html

class UseCounter {
    WTF_MAKE_NONCOPYABLE(UseCounter);
public:
    UseCounter();
    ~UseCounter();

    enum Feature {
        PageDestruction,
        LegacyNotifications,
        MultipartMainResource,
        PrefixedIndexedDB,
        WorkerStart,
        SharedWorkerStart,
        LegacyWebAudio,
        WebAudioStart,
        PrefixedContentSecurityPolicy,
        UnprefixedIndexedDB,
        OpenWebDatabase,
        REMOVEDLegacyHTMLNotifications,
        LegacyTextNotifications,
        UnprefixedRequestAnimationFrame,
        PrefixedRequestAnimationFrame,
        ContentSecurityPolicy,
        ContentSecurityPolicyReportOnly,
        PrefixedContentSecurityPolicyReportOnly,
        PrefixedTransitionEndEvent,
        UnprefixedTransitionEndEvent,
        PrefixedAndUnprefixedTransitionEndEvent,
        AutoFocusAttribute,
        REMOVEDAutoSaveAttribute,
        DataListElement,
        FormAttribute,
        IncrementalAttribute,
        InputTypeColor,
        InputTypeDate,
        InputTypeDateTime,
        InputTypeDateTimeFallback,
        InputTypeDateTimeLocal,
        InputTypeEmail,
        InputTypeMonth,
        InputTypeNumber,
        InputTypeRange,
        InputTypeSearch,
        InputTypeTel,
        InputTypeTime,
        InputTypeURL,
        InputTypeWeek,
        InputTypeWeekFallback,
        ListAttribute,
        MaxAttribute,
        MinAttribute,
        PatternAttribute,
        PlaceholderAttribute,
        PrecisionAttribute,
        PrefixedDirectoryAttribute,
        PrefixedSpeechAttribute,
        RequiredAttribute,
        ResultsAttribute,
        StepAttribute,
        PageVisits,
        HTMLMarqueeElement,
        REMOVEDCSSOverflowMarquee,
        Reflection,
        REMOVEDCursorVisibility,
        PrefixedStorageInfo,
        XFrameOptions,
        XFrameOptionsSameOrigin,
        XFrameOptionsSameOriginWithBadAncestorChain,
        DeprecatedFlexboxWebContent,
        DeprecatedFlexboxChrome,
        DeprecatedFlexboxChromeExtension,
        REMOVEDSVGTRefElement,
        UnprefixedPerformanceTimeline,
        PrefixedPerformanceTimeline,
        UnprefixedUserTiming,
        REMOVEDPrefixedUserTiming,
        WindowEvent,
        ContentSecurityPolicyWithBaseElement,
        PrefixedMediaAddKey,
        PrefixedMediaGenerateKeyRequest,
        REMOVEDWebAudioLooping,
        DocumentClear,
        REMOVEDPrefixedTransitionMediaFeature,
        SVGFontElement,
        XMLDocument,
        XSLProcessingInstruction,
        XSLTProcessor,
        SVGSwitchElement,
        REMOVEDPrefixedDocumentRegister,
        HTMLShadowElementOlderShadowRoot,
        DocumentAll,
        FormElement,
        DemotedFormElement,
        CaptureAttributeAsEnum,
        ShadowDOMPrefixedPseudo,
        ShadowDOMPrefixedCreateShadowRoot,
        ShadowDOMPrefixedShadowRoot,
        SVGAnimationElement,
        KeyboardEventKeyLocation,
        CaptureEvents,
        ReleaseEvents,
        CSSDisplayRunIn,
        CSSDisplayCompact,
        LineClamp,
        SubFrameBeforeUnloadRegistered,
        SubFrameBeforeUnloadFired,
        CSSPseudoElementPrefixedDistributed,
        TextReplaceWholeText,
        PrefixedShadowRootConstructor,
        ConsoleMarkTimeline,
        CSSPseudoElementUserAgentCustomPseudo,
        DocumentTypeEntities, // Removed from DOM4.
        DocumentTypeInternalSubset, // Removed from DOM4.
        DocumentTypeNotations, // Removed from DOM4.
        ElementGetAttributeNode, // Removed from DOM4.
        ElementSetAttributeNode, // Removed from DOM4.
        ElementRemoveAttributeNode, // Removed from DOM4.
        ElementGetAttributeNodeNS, // Removed from DOM4.
        DocumentCreateAttribute, // Removed from DOM4.
        DocumentCreateAttributeNS, // Removed from DOM4.
        DocumentCreateCDATASection, // Removed from DOM4.
        DocumentInputEncoding, // Removed from DOM4.
        DocumentXMLEncoding, // Removed from DOM4.
        DocumentXMLStandalone, // Removed from DOM4.
        DocumentXMLVersion, // Removed from DOM4.
        NodeIsSameNode, // Removed from DOM4.
        NodeIsSupported, // Removed from DOM4.
        NodeNamespaceURI, // Removed from DOM4.
        REMOVEDNodePrefix,
        NodeLocalName, // Removed from DOM4.
        NavigatorProductSub,
        NavigatorVendor,
        NavigatorVendorSub,
        FileError,
        DocumentCharset, // Documented as IE extensions, from KHTML days.
        PrefixedAnimationEndEvent,
        UnprefixedAnimationEndEvent,
        PrefixedAndUnprefixedAnimationEndEvent,
        PrefixedAnimationStartEvent,
        UnprefixedAnimationStartEvent,
        PrefixedAndUnprefixedAnimationStartEvent,
        PrefixedAnimationIterationEvent,
        UnprefixedAnimationIterationEvent,
        PrefixedAndUnprefixedAnimationIterationEvent,
        EventReturnValue, // Legacy IE extension.
        SVGSVGElement,
        InsertAdjacentText,
        InsertAdjacentElement,
        HasAttributes, // Removed from DOM4.
        DOMSubtreeModifiedEvent,
        DOMNodeInsertedEvent,
        DOMNodeRemovedEvent,
        DOMNodeRemovedFromDocumentEvent,
        DOMNodeInsertedIntoDocumentEvent,
        DOMCharacterDataModifiedEvent,
        DocumentAllTags,
        DocumentAllLegacyCall,
        HTMLAppletElementLegacyCall,
        HTMLEmbedElementLegacyCall,
        HTMLObjectElementLegacyCall,
        BeforeLoadEvent,
        GetMatchedCSSRules,
        SVGFontInCSS,
        ScrollTopBodyNotQuirksMode,
        ScrollLeftBodyNotQuirksMode,
        REMOVEDAttributeIsId,
        REMOVEDAttributeOwnerElement,
        REMOVEDAttributeSetPrefix,
        AttributeSpecified, // Removed in DOM4.
        BeforeLoadEventInIsolatedWorld,
        PrefixedAudioDecodedByteCount,
        PrefixedVideoDecodedByteCount,
        PrefixedVideoSupportsFullscreen,
        PrefixedVideoDisplayingFullscreen,
        PrefixedVideoEnterFullscreen,
        PrefixedVideoExitFullscreen,
        PrefixedVideoEnterFullScreen,
        PrefixedVideoExitFullScreen,
        PrefixedVideoDecodedFrameCount,
        PrefixedVideoDroppedFrameCount,
        REMOVEDSourceElementCandidate,
        REMOVEDSourceElementNonMatchingMedia,
        PrefixedElementRequestFullscreen,
        PrefixedElementRequestFullScreen,
        BarPropLocationbar,
        BarPropMenubar,
        BarPropPersonalbar,
        BarPropScrollbars,
        BarPropStatusbar,
        BarPropToolbar,
        InputTypeEmailMultiple,
        InputTypeEmailMaxLength,
        InputTypeEmailMultipleMaxLength,
        TextTrackCueConstructor,
        CSSStyleDeclarationPropertyName, // Removed in CSSOM.
        CSSStyleDeclarationFloatPropertyName, // Pending removal in CSSOM.
        InputTypeText,
        InputTypeTextMaxLength,
        InputTypePassword,
        InputTypePasswordMaxLength,
        SVGInstanceRoot,
        ShowModalDialog,
        PrefixedPageVisibility,
        HTMLFrameElementLocation,
        CSSStyleSheetInsertRuleOptionalArg, // Inconsistent with the specification and other browsers.
        CSSWebkitRegionAtRule, // @region rule changed to ::region()
        DocumentBeforeUnloadRegistered,
        DocumentBeforeUnloadFired,
        DocumentUnloadRegistered,
        DocumentUnloadFired,
        SVGLocatableNearestViewportElement,
        SVGLocatableFarthestViewportElement,
        IsIndexElement,
        HTMLHeadElementProfile,
        OverflowChangedEvent,
        SVGPointMatrixTransform,
        HTMLHtmlElementManifest,
        DOMFocusInOutEvent,
        FileGetLastModifiedDate,
        HTMLElementInnerText,
        HTMLElementOuterText,
        ReplaceDocumentViaJavaScriptURL,
        ElementSetAttributeNodeNS, // Removed from DOM4.
        ElementPrefixedMatchesSelector,
        DOMImplementationCreateCSSStyleSheet,
        CSSStyleSheetRules,
        CSSStyleSheetAddRule,
        CSSStyleSheetRemoveRule,
        InitMessageEvent,
        PrefixedInitMessageEvent,
        ElementSetPrefix, // Element.prefix is readonly in DOM4.
        CSSStyleDeclarationGetPropertyCSSValue,
        SVGElementGetPresentationAttribute,
        REMOVEDAttrUsedAsNodeParameter,
        REMOVEDAttrUsedAsNodeReceiver,
        PrefixedMediaCancelKeyRequest,
        DOMImplementationHasFeature,
        DOMImplementationHasFeatureReturnFalse,
        CanPlayTypeKeySystem,
        PrefixedDevicePixelRatioMediaFeature,
        PrefixedMaxDevicePixelRatioMediaFeature,
        PrefixedMinDevicePixelRatioMediaFeature,
        PrefixedTransform2dMediaFeature,
        PrefixedTransform3dMediaFeature,
        PrefixedAnimationMediaFeature,
        PrefixedViewModeMediaFeature,
        PrefixedStorageQuota,
        ContentSecurityPolicyReportOnlyInMeta,
        PrefixedMediaSourceOpen,
        ResetReferrerPolicy,
        CaseInsensitiveAttrSelectorMatch, // Case-insensitivity dropped from specification.
        CaptureAttributeAsBoolean,
        FormNameAccessForImageElement,
        FormNameAccessForPastNamesMap,
        FormAssociationByParser,
        HTMLSourceElementMedia,
        // Add new features immediately above this line. Don't change assigned
        // numbers of any item, and don't reuse removed slots.
        NumberOfFeatures, // This enum value must be last.
    };

    // "count" sets the bit for this feature to 1. Repeated calls are ignored.
    static void count(const Document&, Feature);
    static void count(const ExecutionContext*, Feature);
    void count(CSSParserContext, CSSPropertyID);
    void count(Feature);

    // "countDeprecation" sets the bit for this feature to 1, and sends a deprecation
    // warning to the console. Repeated calls are ignored.
    //
    // Be considerate to developers' consoles: features should only send deprecation warnings
    // when we're actively interested in removing them from the platform.
    static void countDeprecation(const DOMWindow*, Feature);
    static void countDeprecation(ExecutionContext*, Feature);
    static void countDeprecation(const Document&, Feature);
    String deprecationMessage(Feature);

    void didCommitLoad();

    static UseCounter* getFrom(const Document*);
    static UseCounter* getFrom(const CSSStyleSheet*);
    static UseCounter* getFrom(const StyleSheetContents*);

    static int mapCSSPropertyIdToCSSSampleIdForHistogram(int id);

private:
    bool recordMeasurement(Feature feature)
    {
        ASSERT(feature != PageDestruction); // PageDestruction is reserved as a scaling factor.
        ASSERT(feature < NumberOfFeatures);
        if (!m_countBits) {
            m_countBits = adoptPtr(new BitVector(NumberOfFeatures));
            m_countBits->clearAll();
        }

        if (m_countBits->quickGet(feature))
            return false;

        m_countBits->quickSet(feature);
        return true;
    }

    void updateMeasurements();

    OwnPtr<BitVector> m_countBits;
    BitVector m_CSSFeatureBits;
};

} // namespace WebCore

#endif // UseCounter_h
