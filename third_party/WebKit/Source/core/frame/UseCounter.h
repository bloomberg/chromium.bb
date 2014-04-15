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
        // Do not change assigned numbers of existing items: add new features
        // to the end of the list.
        PageDestruction = 0,
        LegacyNotifications = 1,
        PrefixedIndexedDB = 3,
        WorkerStart = 4,
        SharedWorkerStart = 5,
        LegacyWebAudio = 6,
        WebAudioStart = 7,
        UnprefixedIndexedDB = 9,
        OpenWebDatabase = 10,
        LegacyTextNotifications = 12,
        UnprefixedRequestAnimationFrame = 13,
        PrefixedRequestAnimationFrame = 14,
        ContentSecurityPolicy = 15,
        ContentSecurityPolicyReportOnly = 16,
        PrefixedTransitionEndEvent = 18,
        UnprefixedTransitionEndEvent = 19,
        PrefixedAndUnprefixedTransitionEndEvent = 20,
        AutoFocusAttribute = 21,
        DataListElement = 23,
        FormAttribute = 24,
        IncrementalAttribute = 25,
        InputTypeColor = 26,
        InputTypeDate = 27,
        InputTypeDateTimeFallback = 29,
        InputTypeDateTimeLocal = 30,
        InputTypeEmail = 31,
        InputTypeMonth = 32,
        InputTypeNumber = 33,
        InputTypeRange = 34,
        InputTypeSearch = 35,
        InputTypeTel = 36,
        InputTypeTime = 37,
        InputTypeURL = 38,
        InputTypeWeek = 39,
        InputTypeWeekFallback = 40,
        ListAttribute = 41,
        MaxAttribute = 42,
        MinAttribute = 43,
        PatternAttribute = 44,
        PlaceholderAttribute = 45,
        PrecisionAttribute = 46,
        PrefixedDirectoryAttribute = 47,
        PrefixedSpeechAttribute = 48,
        RequiredAttribute = 49,
        ResultsAttribute = 50,
        StepAttribute = 51,
        PageVisits = 52,
        HTMLMarqueeElement = 53,
        Reflection = 55,
        PrefixedStorageInfo = 57,
        XFrameOptions = 58,
        XFrameOptionsSameOrigin = 59,
        XFrameOptionsSameOriginWithBadAncestorChain = 60,
        DeprecatedFlexboxWebContent = 61,
        DeprecatedFlexboxChrome = 62,
        DeprecatedFlexboxChromeExtension = 63,
        UnprefixedPerformanceTimeline = 65,
        PrefixedPerformanceTimeline = 66,
        UnprefixedUserTiming = 67,
        WindowEvent = 69,
        ContentSecurityPolicyWithBaseElement = 70,
        PrefixedMediaAddKey = 71,
        PrefixedMediaGenerateKeyRequest = 72,
        DocumentClear = 74,
        SVGFontElement = 76,
        XMLDocument = 77,
        XSLProcessingInstruction = 78,
        XSLTProcessor = 79,
        SVGSwitchElement = 80,
        DocumentAll = 83,
        FormElement = 84,
        DemotedFormElement = 85,
        CaptureAttributeAsEnum = 86,
        SVGAnimationElement = 90,
        KeyboardEventKeyLocation = 91,
        LineClamp = 96,
        SubFrameBeforeUnloadRegistered = 97,
        SubFrameBeforeUnloadFired = 98,
        TextReplaceWholeText = 100,
        PrefixedShadowRootConstructor = 101,
        ConsoleMarkTimeline = 102,
        CSSPseudoElementUserAgentCustomPseudo = 103,
        DocumentTypeInternalSubset = 105, // Removed from DOM4.
        ElementGetAttributeNode = 107, // Removed from DOM4.
        ElementSetAttributeNode = 108, // Removed from DOM4.
        ElementRemoveAttributeNode = 109, // Removed from DOM4.
        ElementGetAttributeNodeNS = 110, // Removed from DOM4.
        DocumentCreateAttribute = 111, // Removed from DOM4.
        DocumentCreateCDATASection = 113, // Removed from DOM4.
        DocumentInputEncoding = 114, // Removed from DOM4.
        DocumentXMLEncoding = 115, // Removed from DOM4.
        DocumentXMLStandalone = 116, // Removed from DOM4.
        DocumentXMLVersion = 117, // Removed from DOM4.
        NodeIsSameNode = 118, // Removed from DOM4.
        NodeNamespaceURI = 120, // Removed from DOM4.
        NodeLocalName = 122, // Removed from DOM4.
        NavigatorProductSub = 123,
        NavigatorVendor = 124,
        NavigatorVendorSub = 125,
        FileError = 126,
        DocumentCharset = 127, // Documented as IE extensions = 0, from KHTML days.
        PrefixedAnimationEndEvent = 128,
        UnprefixedAnimationEndEvent = 129,
        PrefixedAndUnprefixedAnimationEndEvent = 130,
        PrefixedAnimationStartEvent = 131,
        UnprefixedAnimationStartEvent = 132,
        PrefixedAndUnprefixedAnimationStartEvent = 133,
        PrefixedAnimationIterationEvent = 134,
        UnprefixedAnimationIterationEvent = 135,
        PrefixedAndUnprefixedAnimationIterationEvent = 136,
        EventReturnValue = 137, // Legacy IE extension.
        SVGSVGElement = 138,
        SVGAnimateColorElement = 139,
        InsertAdjacentText = 140,
        InsertAdjacentElement = 141,
        HasAttributes = 142, // Removed from DOM4.
        DOMSubtreeModifiedEvent = 143,
        DOMNodeInsertedEvent = 144,
        DOMNodeRemovedEvent = 145,
        DOMNodeRemovedFromDocumentEvent = 146,
        DOMNodeInsertedIntoDocumentEvent = 147,
        DOMCharacterDataModifiedEvent = 148,
        DocumentAllLegacyCall = 150,
        HTMLAppletElementLegacyCall = 151,
        HTMLEmbedElementLegacyCall = 152,
        HTMLObjectElementLegacyCall = 153,
        GetMatchedCSSRules = 155,
        SVGFontInCSS = 156,
        AttributeSpecified = 162, // Removed in DOM4.
        PrefixedAudioDecodedByteCount = 164,
        PrefixedVideoDecodedByteCount = 165,
        PrefixedVideoSupportsFullscreen = 166,
        PrefixedVideoDisplayingFullscreen = 167,
        PrefixedVideoEnterFullscreen = 168,
        PrefixedVideoExitFullscreen = 169,
        PrefixedVideoEnterFullScreen = 170,
        PrefixedVideoExitFullScreen = 171,
        PrefixedVideoDecodedFrameCount = 172,
        PrefixedVideoDroppedFrameCount = 173,
        PrefixedElementRequestFullscreen = 176,
        PrefixedElementRequestFullScreen = 177,
        BarPropLocationbar = 178,
        BarPropMenubar = 179,
        BarPropPersonalbar = 180,
        BarPropScrollbars = 181,
        BarPropStatusbar = 182,
        BarPropToolbar = 183,
        InputTypeEmailMultiple = 184,
        InputTypeEmailMaxLength = 185,
        InputTypeEmailMultipleMaxLength = 186,
        InputTypeText = 190,
        InputTypeTextMaxLength = 191,
        InputTypePassword = 192,
        InputTypePasswordMaxLength = 193,
        SVGInstanceRoot = 194,
        ShowModalDialog = 195,
        PrefixedPageVisibility = 196,
        CSSStyleSheetInsertRuleOptionalArg = 198, // Inconsistent with the specification and other browsers.
        DocumentBeforeUnloadRegistered = 200,
        DocumentBeforeUnloadFired = 201,
        DocumentUnloadRegistered = 202,
        DocumentUnloadFired = 203,
        SVGLocatableNearestViewportElement = 204,
        SVGLocatableFarthestViewportElement = 205,
        HTMLHeadElementProfile = 207,
        OverflowChangedEvent = 208,
        SVGPointMatrixTransform = 209,
        HTMLHtmlElementManifest = 210,
        DOMFocusInOutEvent = 211,
        FileGetLastModifiedDate = 212,
        HTMLElementInnerText = 213,
        HTMLElementOuterText = 214,
        ReplaceDocumentViaJavaScriptURL = 215,
        ElementPrefixedMatchesSelector = 217,
        DOMImplementationCreateCSSStyleSheet = 218,
        CSSStyleSheetRules = 219,
        CSSStyleSheetAddRule = 220,
        CSSStyleSheetRemoveRule = 221,
        // The above items are available in M33 branch.

        InitMessageEvent = 222,
        PrefixedInitMessageEvent = 223,
        ElementSetPrefix = 224, // Element.prefix is readonly in DOM4.
        CSSStyleDeclarationGetPropertyCSSValue = 225,
        PrefixedMediaCancelKeyRequest = 229,
        DOMImplementationHasFeature = 230,
        DOMImplementationHasFeatureReturnFalse = 231,
        CanPlayTypeKeySystem = 232,
        PrefixedDevicePixelRatioMediaFeature = 233,
        PrefixedMaxDevicePixelRatioMediaFeature = 234,
        PrefixedMinDevicePixelRatioMediaFeature = 235,
        PrefixedTransform2dMediaFeature = 236,
        PrefixedTransform3dMediaFeature = 237,
        PrefixedAnimationMediaFeature = 238,
        PrefixedViewModeMediaFeature = 239,
        PrefixedStorageQuota = 240,
        ContentSecurityPolicyReportOnlyInMeta = 241,
        PrefixedMediaSourceOpen = 242,
        ResetReferrerPolicy = 243,
        CaseInsensitiveAttrSelectorMatch = 244, // Case-insensitivity dropped from specification.
        CaptureAttributeAsBoolean = 245,
        FormNameAccessForImageElement = 246,
        FormNameAccessForPastNamesMap = 247,
        FormAssociationByParser = 248,
        HTMLSourceElementMedia = 249,
        SVGSVGElementInDocument = 250,
        SVGDocumentRootElement = 251,
        DocumentCreateEventOptionalArgument = 252,
        MediaErrorEncrypted = 253,
        EventSourceURL = 254,
        WebSocketURL = 255,
        WorkerSubjectToCSP = 257,
        WorkerAllowedByChildBlockedByScript = 258,
        HTMLMediaElementControllerNotNull = 259,
        DeprecatedWebKitGradient = 260,
        DeprecatedWebKitLinearGradient = 261,
        DeprecatedWebKitRepeatingLinearGradient = 262,
        DeprecatedWebKitRadialGradient = 263,
        DeprecatedWebKitRepeatingRadialGradient = 264,
        PrefixedImageSmoothingEnabled = 267,
        UnprefixedImageSmoothingEnabled = 268,
        PromiseConstructor = 270,
        PromiseCast = 271,
        PromiseReject = 272,
        PromiseResolve = 273,
        // The above items are available in M34 branch.

        TextAutosizing = 274,
        TextAutosizingLayout = 275,
        HTMLAnchorElementPingAttribute = 276,
        JavascriptExhaustedMemory = 277,
        InsertAdjacentHTML = 278,
        SVGClassName = 279,
        HTMLAppletElement = 280,
        HTMLMediaElementSeekToFragmentStart = 281,
        HTMLMediaElementPauseAtFragmentEnd = 282,
        PrefixedWindowURL = 283,
        PrefixedWorkerURL = 284,
        WindowOrientation = 285,
        DOMStringListContains = 286,
        DocumentCaptureEvents = 287,
        DocumentReleaseEvents = 288,
        WindowCaptureEvents = 289,
        WindowReleaseEvents = 290,
        PrefixedGamepad = 291,
        ElementAnimateKeyframeListEffectObjectTiming = 292,
        ElementAnimateKeyframeListEffectDoubleTiming = 293,
        ElementAnimateKeyframeListEffectNoTiming = 294,
        DocumentXPathCreateExpression = 295,
        DocumentXPathCreateNSResolver = 296,
        DocumentXPathEvaluate = 297,
        AttrGetValue = 298,
        AttrSetValue = 299,
        AnimationConstructorKeyframeListEffectObjectTiming = 300,
        AnimationConstructorKeyframeListEffectDoubleTiming = 301,
        AnimationConstructorKeyframeListEffectNoTiming = 302,
        AttrSetValueWithElement = 303,
        PrefixedCancelAnimationFrame = 304,
        PrefixedCancelRequestAnimationFrame = 305,
        NamedNodeMapGetNamedItem = 306,
        NamedNodeMapSetNamedItem = 307,
        NamedNodeMapRemoveNamedItem = 308,
        NamedNodeMapItem = 309,
        NamedNodeMapGetNamedItemNS = 310,
        NamedNodeMapSetNamedItemNS = 311,
        NamedNodeMapRemoveNamedItemNS = 312,
        OpenWebDatabaseInWorker = 313,
        OpenWebDatabaseSyncInWorker = 314,
        PrefixedAllowFullscreenAttribute = 315,
        XHRProgressEventPosition = 316,
        XHRProgressEventTotalSize = 317,
        PrefixedDocumentIsFullscreen = 318,
        PrefixedDocumentFullScreenKeyboardInputAllowed = 319,
        PrefixedDocumentCurrentFullScreenElement = 320,
        PrefixedDocumentCancelFullScreen = 321,
        PrefixedDocumentFullscreenEnabled = 322,
        PrefixedDocumentFullscreenElement = 323,
        PrefixedDocumentExitFullscreen = 324,
        // The above items are available in M35 branch.

        SVGForeignObjectElement = 325,
        PrefixedElementRequestPointerLock = 326,
        SelectionSetPosition = 327,
        AnimationPlayerFinishEvent = 328,
        SVGSVGElementInXMLDocument = 329,
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

    static void muteForInspector();
    static void unmuteForInspector();

private:
    static int m_muteCount;

    bool recordMeasurement(Feature feature)
    {
        if (UseCounter::m_muteCount)
            return false;
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
