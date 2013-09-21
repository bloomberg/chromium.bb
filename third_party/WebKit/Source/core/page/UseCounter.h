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
class ScriptExecutionContext;
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
        UnusedSlot01, // Prior to 7/2013, we used this slot for LegacyHTMLNotifications.
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
        UnusedSlot02, // Prior to 4/2013, we used this slot for AutoSaveAttribute.
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
        UnusedSlot03, // Removed, was tracking overflow: -webkit-marquee.
        Reflection,
        CursorVisibility, // Removed, was -webkit-cursor-visibility.
        StorageInfo,
        XFrameOptions,
        XFrameOptionsSameOrigin,
        XFrameOptionsSameOriginWithBadAncestorChain,
        DeprecatedFlexboxWebContent,
        DeprecatedFlexboxChrome,
        DeprecatedFlexboxChromeExtension,
        UnusedSlot04,
        UnprefixedPerformanceTimeline,
        PrefixedPerformanceTimeline,
        UnprefixedUserTiming,
        PrefixedUserTiming,
        WindowEvent,
        ContentSecurityPolicyWithBaseElement,
        PrefixedMediaAddKey,
        PrefixedMediaGenerateKeyRequest,
        WebAudioLooping,
        DocumentClear,
        PrefixedTransitionMediaFeature,
        SVGFontElement,
        XMLDocument,
        XSLProcessingInstruction,
        XSLTProcessor,
        SVGSwitchElement,
        PrefixedDocumentRegister,
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
        NodePrefix, // Removed from DOM4.
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
        SVGAnimateColorElement,
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
        // Add new features immediately above this line. Don't change assigned
        // numbers of each items, and don't reuse unused slots.
        NumberOfFeatures, // This enum value must be last.
    };

    // "count" sets the bit for this feature to 1. Repeated calls are ignored.
    static void count(Document*, Feature);
    static void count(DOMWindow*, Feature);
    void count(CSSPropertyID);
    void count(Feature);

    // "countDeprecation" sets the bit for this feature to 1, and sends a deprecation
    // warning to the console. Repeated calls are ignored.
    //
    // Be considerate to developers' consoles: features should only send deprecation warnings
    // when we're actively interested in removing them from the platform.
    static void countDeprecation(DOMWindow*, Feature);
    static void countDeprecation(ScriptExecutionContext*, Feature);
    static void countDeprecation(Document*, Feature);
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
