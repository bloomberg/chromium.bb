/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#ifndef Document_h
#define Document_h

#include "bindings/v8/ScriptValue.h"
#include "core/animation/css/CSSPendingAnimations.h"
#include "core/dom/ContainerNode.h"
#include "core/dom/DocumentEncodingData.h"
#include "core/dom/DocumentInit.h"
#include "core/dom/DocumentLifecycle.h"
#include "core/dom/DocumentSupplementable.h"
#include "core/dom/DocumentTiming.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/IconURL.h"
#include "core/dom/MutationObserver.h"
#include "core/dom/QualifiedName.h"
#include "core/dom/TextLinkColors.h"
#include "core/dom/TreeScope.h"
#include "core/dom/UserActionElementSet.h"
#include "core/dom/ViewportDescription.h"
#include "core/dom/custom/CustomElement.h"
#include "core/html/CollectionType.h"
#include "core/page/FocusType.h"
#include "core/page/PageVisibilityState.h"
#include "core/rendering/HitTestRequest.h"
#include "platform/Length.h"
#include "platform/Timer.h"
#include "platform/heap/Handle.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/ReferrerPolicy.h"
#include "wtf/HashSet.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/WeakPtr.h"

namespace WebCore {

class AXObjectCache;
class AnimationClock;
class Attr;
class CDATASection;
class CSSFontSelector;
class CSSStyleDeclaration;
class CSSStyleSheet;
class CSSStyleSheetResource;
class CanvasRenderingContext2D;
class CharacterData;
class Chrome;
class Comment;
class ContentSecurityPolicyResponseHeaders;
class ContextFeatures;
class CustomElementRegistrationContext;
class DOMImplementation;
class DOMSelection;
class DOMWindow;
class Database;
class DatabaseThread;
class DocumentFragment;
class DocumentLifecycleNotifier;
class DocumentLifecycleObserver;
class DocumentLoader;
class DocumentMarkerController;
class DocumentParser;
class DocumentTimeline;
class DocumentType;
class Element;
class ElementDataCache;
class Event;
class EventListener;
class ExceptionState;
class FastTextAutosizer;
class FloatQuad;
class FloatRect;
class FontFaceSet;
class FormController;
class LocalFrame;
class FrameHost;
class FrameView;
class HTMLAllCollection;
class HTMLCanvasElement;
class HTMLCollection;
class HTMLDialogElement;
class HTMLDocument;
class HTMLElement;
class HTMLFrameOwnerElement;
class HTMLHeadElement;
class HTMLIFrameElement;
class HTMLImport;
class HTMLMapElement;
class HTMLNameCollection;
class HTMLScriptElement;
class HitTestRequest;
class HitTestResult;
class IntPoint;
class JSNode;
class LayoutPoint;
class LayoutRect;
class LiveNodeListBase;
class Locale;
class Location;
class MainThreadTaskRunner;
class MediaQueryList;
class MediaQueryMatcher;
class MouseEventWithHitTestResults;
class NodeFilter;
class NodeIterator;
class Page;
class PlatformMouseEvent;
class ProcessingInstruction;
class Range;
class RegisteredEventListener;
class RenderView;
class RequestAnimationFrameCallback;
class ResourceFetcher;
class SVGDocumentExtensions;
class SVGUseElement;
class ScriptElementData;
class ScriptResource;
class ScriptRunner;
class ScriptableDocumentParser;
class ScriptedAnimationController;
class SecurityOrigin;
class SegmentedString;
class SelectorQueryCache;
class SerializedScriptValue;
class Settings;
class StyleEngine;
class StyleResolver;
class StyleSheet;
class StyleSheetContents;
class StyleSheetList;
class Text;
class TextAutosizer;
class Touch;
class TouchList;
class TransformSource;
class TreeWalker;
class VisitedLinkState;
class WebGLRenderingContext;
class XMLHttpRequest;

struct AnnotatedRegionValue;

typedef int ExceptionCode;

enum RecalcStyleTime {
    RecalcStyleImmediately, // synchronous
    RecalcStyleDeferred // asynchronous
};

enum StyleResolverUpdateMode {
    // Discards the StyleResolver and rebuilds it.
    FullStyleUpdate,
    // Attempts to use StyleInvalidationAnalysis to avoid discarding the entire StyleResolver.
    AnalyzedStyleUpdate
};

enum NodeListInvalidationType {
    DoNotInvalidateOnAttributeChanges = 0,
    InvalidateOnClassAttrChange,
    InvalidateOnIdNameAttrChange,
    InvalidateOnNameAttrChange,
    InvalidateOnForAttrChange,
    InvalidateForFormControls,
    InvalidateOnHRefAttrChange,
    InvalidateOnAnyAttrChange,
};
const int numNodeListInvalidationTypes = InvalidateOnAnyAttrChange + 1;

typedef HashCountedSet<Node*> TouchEventTargetSet;

enum DocumentClass {
    DefaultDocumentClass = 0,
    HTMLDocumentClass = 1,
    XHTMLDocumentClass = 1 << 1,
    ImageDocumentClass = 1 << 2,
    PluginDocumentClass = 1 << 3,
    MediaDocumentClass = 1 << 4,
    SVGDocumentClass = 1 << 5,
    XMLDocumentClass = 1 << 6,
};

typedef unsigned char DocumentClassFlags;

class Document;

class DocumentVisibilityObserver {
public:
    DocumentVisibilityObserver(Document&);
    virtual ~DocumentVisibilityObserver();

    virtual void didChangeVisibilityState(PageVisibilityState) = 0;

    // Classes that inherit Node and DocumentVisibilityObserver must have a
    // virtual override of Node::didMoveToNewDocument that calls
    // DocumentVisibilityObserver::setDocument
    void setObservedDocument(Document&);

private:
    void registerObserver(Document&);
    void unregisterObserver();
    Document* m_document;
};

class Document : public ContainerNode, public TreeScope, public SecurityContext, public ExecutionContext, public ExecutionContextClient
    , public DocumentSupplementable, public LifecycleContext<Document> {
    WILL_BE_USING_GARBAGE_COLLECTED_MIXIN(Document);
public:
    static PassRefPtr<Document> create(const DocumentInit& initializer = DocumentInit())
    {
        return adoptRef(new Document(initializer));
    }
    virtual ~Document();

    MediaQueryMatcher& mediaQueryMatcher();

    void mediaQueryAffectingValueChanged();

    using ContainerNode::ref;
    using ContainerNode::deref;
    using SecurityContext::securityOrigin;
    using SecurityContext::contentSecurityPolicy;
    using ExecutionContextClient::addConsoleMessage;

    virtual bool canContainRangeEndPoint() const OVERRIDE { return true; }

    SelectorQueryCache& selectorQueryCache();

    // Focus Management.
    Element* activeElement() const;
    bool hasFocus() const;

    // DOM methods & attributes for Document

    DEFINE_ATTRIBUTE_EVENT_LISTENER(beforecopy);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(beforecut);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(beforepaste);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(copy);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(cut);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(paste);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(readystatechange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(search);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(securitypolicyviolation);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(selectionchange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(selectstart);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(touchcancel);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(touchend);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(touchmove);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(touchstart);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitfullscreenchange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitfullscreenerror);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitpointerlockchange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitpointerlockerror);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(wheel);

    bool shouldMergeWithLegacyDescription(ViewportDescription::Type);
    bool shouldOverrideLegacyDescription(ViewportDescription::Type);
    void setViewportDescription(const ViewportDescription&);
    const ViewportDescription& viewportDescription() const { return m_viewportDescription; }
    Length viewportDefaultMinWidth() const { return m_viewportDefaultMinWidth; }

#ifndef NDEBUG
    bool didDispatchViewportPropertiesChanged() const { return m_didDispatchViewportPropertiesChanged; }
#endif
    bool hasLegacyViewportTag() const { return m_legacyViewportDescription.isLegacyViewportType(); }

    void setReferrerPolicy(ReferrerPolicy);
    ReferrerPolicy referrerPolicy() const { return m_referrerPolicy; }

    String outgoingReferrer();
    String outgoingOrigin() const;

    void setDoctype(PassRefPtr<DocumentType>);
    DocumentType* doctype() const { return m_docType.get(); }

    DOMImplementation& implementation();

    Element* documentElement() const
    {
        return m_documentElement.get();
    }

    bool hasManifest() const;

    Location* location() const;

    PassRefPtr<Element> createElement(const AtomicString& name, ExceptionState&);
    PassRefPtr<DocumentFragment> createDocumentFragment();
    PassRefPtr<Text> createTextNode(const String& data);
    PassRefPtr<Comment> createComment(const String& data);
    PassRefPtr<CDATASection> createCDATASection(const String& data, ExceptionState&);
    PassRefPtr<ProcessingInstruction> createProcessingInstruction(const String& target, const String& data, ExceptionState&);
    PassRefPtr<Attr> createAttribute(const AtomicString& name, ExceptionState&);
    PassRefPtr<Node> importNode(Node* importedNode, ExceptionState& ec) { return importNode(importedNode, true, ec); }
    PassRefPtr<Node> importNode(Node* importedNode, bool deep, ExceptionState&);
    PassRefPtr<Element> createElementNS(const AtomicString& namespaceURI, const AtomicString& qualifiedName, ExceptionState&);
    PassRefPtr<Element> createElement(const QualifiedName&, bool createdByParser);

    bool regionBasedColumnsEnabled() const;

    /**
     * Retrieve all nodes that intersect a rect in the window's document, until it is fully enclosed by
     * the boundaries of a node.
     *
     * @param centerX x reference for the rectangle in CSS pixels
     * @param centerY y reference for the rectangle in CSS pixels
     * @param topPadding How much to expand the top of the rectangle
     * @param rightPadding How much to expand the right of the rectangle
     * @param bottomPadding How much to expand the bottom of the rectangle
     * @param leftPadding How much to expand the left of the rectangle
     */
    PassRefPtr<NodeList> nodesFromRect(int centerX, int centerY,
        unsigned topPadding, unsigned rightPadding, unsigned bottomPadding, unsigned leftPadding,
        HitTestRequest::HitTestRequestType hitType = HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::ConfusingAndOftenMisusedDisallowShadowContent) const;
    Element* elementFromPoint(int x, int y) const;
    PassRefPtrWillBeRawPtr<Range> caretRangeFromPoint(int x, int y);

    String readyState() const;

    String defaultCharset() const;

    AtomicString inputEncoding() const { return Document::encodingName(); }
    AtomicString charset() const { return Document::encodingName(); }
    AtomicString characterSet() const { return Document::encodingName(); }

    AtomicString encodingName() const;

    void setCharset(const String&);

    void setContent(const String&);

    String suggestedMIMEType() const;
    void setMimeType(const AtomicString&);
    AtomicString contentType() const; // DOM 4 document.contentType

    const AtomicString& contentLanguage() const { return m_contentLanguage; }
    void setContentLanguage(const AtomicString&);

    String xmlEncoding() const { return m_xmlEncoding; }
    String xmlVersion() const { return m_xmlVersion; }
    enum StandaloneStatus { StandaloneUnspecified, Standalone, NotStandalone };
    bool xmlStandalone() const { return m_xmlStandalone == Standalone; }
    StandaloneStatus xmlStandaloneStatus() const { return static_cast<StandaloneStatus>(m_xmlStandalone); }
    bool hasXMLDeclaration() const { return m_hasXMLDeclaration; }

    void setXMLEncoding(const String& encoding) { m_xmlEncoding = encoding; } // read-only property, only to be set from XMLDocumentParser
    void setXMLVersion(const String&, ExceptionState&);
    void setXMLStandalone(bool, ExceptionState&);
    void setHasXMLDeclaration(bool hasXMLDeclaration) { m_hasXMLDeclaration = hasXMLDeclaration ? 1 : 0; }

    virtual KURL baseURI() const OVERRIDE FINAL;

    String visibilityState() const;
    bool hidden() const;
    void didChangeVisibilityState();

    PassRefPtr<Node> adoptNode(PassRefPtr<Node> source, ExceptionState&);

    PassRefPtr<HTMLCollection> images();
    PassRefPtr<HTMLCollection> embeds();
    PassRefPtr<HTMLCollection> applets();
    PassRefPtr<HTMLCollection> links();
    PassRefPtr<HTMLCollection> forms();
    PassRefPtr<HTMLCollection> anchors();
    PassRefPtr<HTMLCollection> scripts();
    PassRefPtr<HTMLCollection> allForBinding();
    PassRefPtr<HTMLCollection> all();

    PassRefPtr<HTMLCollection> windowNamedItems(const AtomicString& name);
    PassRefPtr<HTMLCollection> documentNamedItems(const AtomicString& name);

    bool isHTMLDocument() const { return m_documentClasses & HTMLDocumentClass; }
    bool isXHTMLDocument() const { return m_documentClasses & XHTMLDocumentClass; }
    bool isXMLDocument() const { return m_documentClasses & XMLDocumentClass; }
    bool isImageDocument() const { return m_documentClasses & ImageDocumentClass; }
    bool isSVGDocument() const { return m_documentClasses & SVGDocumentClass; }
    bool isPluginDocument() const { return m_documentClasses & PluginDocumentClass; }
    bool isMediaDocument() const { return m_documentClasses & MediaDocumentClass; }

    bool hasSVGRootNode() const;

    bool isFrameSet() const;

    bool isSrcdocDocument() const { return m_isSrcdocDocument; }
    bool isMobileDocument() const { return m_isMobileDocument; }

    StyleResolver* styleResolver() const;
    StyleResolver& ensureStyleResolver() const;

    bool isViewSource() const { return m_isViewSource; }
    void setIsViewSource(bool);

    bool sawElementsInKnownNamespaces() const { return m_sawElementsInKnownNamespaces; }

    void notifyRemovePendingSheetIfNeeded();

    bool isRenderingReady() const { return haveImportsLoaded() && haveStylesheetsLoaded(); }
    bool isScriptExecutionReady() const { return isRenderingReady(); }

    // This is a DOM function.
    StyleSheetList* styleSheets();

    StyleEngine* styleEngine() { return m_styleEngine.get(); }

    bool gotoAnchorNeededAfterStylesheetsLoad() { return m_gotoAnchorNeededAfterStylesheetsLoad; }
    void setGotoAnchorNeededAfterStylesheetsLoad(bool b) { m_gotoAnchorNeededAfterStylesheetsLoad = b; }

    // Called when one or more stylesheets in the document may have been added, removed, or changed.
    void styleResolverChanged(RecalcStyleTime, StyleResolverUpdateMode = FullStyleUpdate);

    // FIXME: Switch all callers of styleResolverChanged to these or better ones and then make them
    // do something smarter.
    void removedStyleSheet(StyleSheet*, RecalcStyleTime when = RecalcStyleDeferred, StyleResolverUpdateMode = FullStyleUpdate);
    void addedStyleSheet(StyleSheet*, RecalcStyleTime when = RecalcStyleDeferred) { styleResolverChanged(when); }
    void modifiedStyleSheet(StyleSheet*, RecalcStyleTime when = RecalcStyleDeferred, StyleResolverUpdateMode = FullStyleUpdate);
    void changedSelectorWatch() { styleResolverChanged(RecalcStyleDeferred); }

    void scheduleUseShadowTreeUpdate(SVGUseElement&);
    void unscheduleUseShadowTreeUpdate(SVGUseElement&);

    // FIXME: This should be eliminated and elements that use it should be made to
    // always have a layer so they don't need to go about creating one from reasons
    // external to style.
    void scheduleLayerUpdate(Element&);
    void unscheduleLayerUpdate(Element&);

    void evaluateMediaQueryList();

    FormController& formController();
    Vector<String> formElementsState() const;
    void setStateForNewFormElements(const Vector<String>&);

    FrameView* view() const; // can be null
    LocalFrame* frame() const { return m_frame; } // can be null
    FrameHost* frameHost() const; // can be null
    Page* page() const; // can be null
    Settings* settings() const; // can be null

    float devicePixelRatio() const;

    PassRefPtrWillBeRawPtr<Range> createRange();

    PassRefPtr<NodeIterator> createNodeIterator(Node* root, ExceptionState&);
    PassRefPtr<NodeIterator> createNodeIterator(Node* root, unsigned whatToShow, ExceptionState&);
    PassRefPtr<NodeIterator> createNodeIterator(Node* root, unsigned whatToShow, PassRefPtr<NodeFilter>, ExceptionState&);

    PassRefPtr<TreeWalker> createTreeWalker(Node* root, ExceptionState&);
    PassRefPtr<TreeWalker> createTreeWalker(Node* root, unsigned whatToShow, ExceptionState&);
    PassRefPtr<TreeWalker> createTreeWalker(Node* root, unsigned whatToShow, PassRefPtr<NodeFilter>, ExceptionState&);

    // Special support for editing
    PassRefPtr<Text> createEditingTextNode(const String&);

    void setupFontBuilder(RenderStyle* documentStyle);

    void updateRenderTreeIfNeeded() { updateRenderTree(NoChange); }
    void updateRenderTreeForNodeIfNeeded(Node*);
    void updateLayout();
    enum RunPostLayoutTasks {
        RunPostLayoutTasksAsyhnchronously,
        RunPostLayoutTasksSynchronously,
    };
    void updateLayoutIgnorePendingStylesheets(RunPostLayoutTasks = RunPostLayoutTasksAsyhnchronously);
    PassRefPtr<RenderStyle> styleForElementIgnoringPendingStylesheets(Element*);
    PassRefPtr<RenderStyle> styleForPage(int pageIndex);

    void updateDistributionForNodeIfNeeded(Node*);

    // Returns true if page box (margin boxes and page borders) is visible.
    bool isPageBoxVisible(int pageIndex);

    // Returns the preferred page size and margins in pixels, assuming 96
    // pixels per inch. pageSize, marginTop, marginRight, marginBottom,
    // marginLeft must be initialized to the default values that are used if
    // auto is specified.
    void pageSizeAndMarginsInPixels(int pageIndex, IntSize& pageSize, int& marginTop, int& marginRight, int& marginBottom, int& marginLeft);

    ResourceFetcher* fetcher() { return m_fetcher.get(); }

    virtual void attach(const AttachContext& = AttachContext()) OVERRIDE;
    virtual void detach(const AttachContext& = AttachContext()) OVERRIDE;
    void prepareForDestruction();

    RenderView* renderView() const { return m_renderView; }

    AXObjectCache* existingAXObjectCache() const;
    AXObjectCache* axObjectCache() const;
    void clearAXObjectCache();

    // to get visually ordered hebrew and arabic pages right
    bool visuallyOrdered() const { return m_visuallyOrdered; }

    DocumentLoader* loader() const;

    void open(Document* ownerDocument = 0);
    PassRefPtr<DocumentParser> implicitOpen();

    // close() is the DOM API document.close()
    void close();
    // In some situations (see the code), we ignore document.close().
    // explicitClose() bypass these checks and actually tries to close the
    // input stream.
    void explicitClose();
    // implicitClose() actually does the work of closing the input stream.
    void implicitClose();

    bool dispatchBeforeUnloadEvent(Chrome&, bool&);
    void dispatchUnloadEvents();

    enum PageDismissalType {
        NoDismissal = 0,
        BeforeUnloadDismissal = 1,
        PageHideDismissal = 2,
        UnloadDismissal = 3
    };
    PageDismissalType pageDismissalEventBeingDispatched() const;

    void cancelParsing();

    void write(const SegmentedString& text, Document* ownerDocument = 0);
    void write(const String& text, Document* ownerDocument = 0);
    void writeln(const String& text, Document* ownerDocument = 0);

    bool wellFormed() const { return m_wellFormed; }

    const KURL& url() const { return m_url; }
    void setURL(const KURL&);

    // To understand how these concepts relate to one another, please see the
    // comments surrounding their declaration.
    const KURL& baseURL() const { return m_baseURL; }
    void setBaseURLOverride(const KURL&);
    const KURL& baseURLOverride() const { return m_baseURLOverride; }
    const KURL& baseElementURL() const { return m_baseElementURL; }
    const AtomicString& baseTarget() const { return m_baseTarget; }
    void processBaseElement();

    KURL completeURL(const String&) const;
    KURL completeURLWithOverride(const String&, const KURL& baseURLOverride) const;

    virtual String userAgent(const KURL&) const OVERRIDE FINAL;
    virtual void disableEval(const String& errorMessage) OVERRIDE FINAL;

    bool canNavigate(LocalFrame* targetFrame);
    LocalFrame* findUnsafeParentScrollPropagationBoundary();

    CSSStyleSheet& elementSheet();

    virtual PassRefPtr<DocumentParser> createParser();
    DocumentParser* parser() const { return m_parser.get(); }
    ScriptableDocumentParser* scriptableDocumentParser() const;

    bool printing() const { return m_printing; }
    void setPrinting(bool p) { m_printing = p; }

    bool paginatedForScreen() const { return m_paginatedForScreen; }
    void setPaginatedForScreen(bool p) { m_paginatedForScreen = p; }

    bool paginated() const { return printing() || paginatedForScreen(); }

    enum CompatibilityMode { QuirksMode, LimitedQuirksMode, NoQuirksMode };

    void setCompatibilityMode(CompatibilityMode m);
    void lockCompatibilityMode() { m_compatibilityModeLocked = true; }
    CompatibilityMode compatibilityMode() const { return m_compatibilityMode; }

    String compatMode() const;

    bool inQuirksMode() const { return m_compatibilityMode == QuirksMode; }
    bool inLimitedQuirksMode() const { return m_compatibilityMode == LimitedQuirksMode; }
    bool inNoQuirksMode() const { return m_compatibilityMode == NoQuirksMode; }

    enum ReadyState {
        Loading,
        Interactive,
        Complete
    };
    void setReadyState(ReadyState);
    bool isLoadCompleted();

    void setParsing(bool);
    bool parsing() const { return m_isParsing; }

    void setHistoryItemDocumentStateDirty(bool dirty) { m_historyItemDocumentStateDirty = dirty; }
    bool historyItemDocumentStateDirty() const { return m_historyItemDocumentStateDirty; }

    bool shouldScheduleLayout() const;
    bool shouldParserYieldAgressivelyBeforeScriptExecution();
    int elapsedTime() const;

    TextLinkColors& textLinkColors() { return m_textLinkColors; }
    VisitedLinkState& visitedLinkState() const { return *m_visitedLinkState; }

    MouseEventWithHitTestResults prepareMouseEvent(const HitTestRequest&, const LayoutPoint&, const PlatformMouseEvent&);

    /* Newly proposed CSS3 mechanism for selecting alternate
       stylesheets using the DOM. May be subject to change as
       spec matures. - dwh
    */
    String preferredStylesheetSet() const;
    String selectedStylesheetSet() const;
    void setSelectedStylesheetSet(const String&);

    bool setFocusedElement(PassRefPtr<Element>, FocusType = FocusTypeNone);
    Element* focusedElement() const { return m_focusedElement.get(); }
    UserActionElementSet& userActionElements()  { return m_userActionElements; }
    const UserActionElementSet& userActionElements() const { return m_userActionElements; }
    void setNeedsFocusedElementCheck();
    void setAutofocusElement(Element*);
    Element* autofocusElement() const { return m_autofocusElement.get(); }

    void setHoverNode(PassRefPtr<Node>);
    Node* hoverNode() const { return m_hoverNode.get(); }

    void setActiveHoverElement(PassRefPtr<Element>);
    Element* activeHoverElement() const { return m_activeHoverElement.get(); }

    void removeFocusedElementOfSubtree(Node*, bool amongChildrenOnly = false);
    void hoveredNodeDetached(Node*);
    void activeChainNodeDetached(Node*);

    void updateHoverActiveState(const HitTestRequest&, Element*, const PlatformMouseEvent* = 0);

    // Updates for :target (CSS3 selector).
    void setCSSTarget(Element*);
    Element* cssTarget() const { return m_cssTarget; }

    void scheduleRenderTreeUpdateIfNeeded();
    bool hasPendingForcedStyleRecalc() const;

    void registerNodeList(LiveNodeListBase*);
    void unregisterNodeList(LiveNodeListBase*);
    void incrementNodeListWithIdNameCacheCount();
    void decrementNodeListWithIdNameCacheCount();
    bool shouldInvalidateNodeListCaches(const QualifiedName* attrName = 0) const;
    void invalidateNodeListCaches(const QualifiedName* attrName);

    void attachNodeIterator(NodeIterator*);
    void detachNodeIterator(NodeIterator*);
    void moveNodeIteratorsToNewDocument(Node&, Document&);

    void attachRange(Range*);
    void detachRange(Range*);

    void updateRangesAfterChildrenChanged(ContainerNode*);
    // nodeChildrenWillBeRemoved is used when removing all node children at once.
    void nodeChildrenWillBeRemoved(ContainerNode&);
    // nodeWillBeRemoved is only safe when removing one node at a time.
    void nodeWillBeRemoved(Node&);
    bool canReplaceChild(const Node& newChild, const Node& oldChild) const;

    void didInsertText(Node*, unsigned offset, unsigned length);
    void didRemoveText(Node*, unsigned offset, unsigned length);
    void didMergeTextNodes(Text& oldNode, unsigned offset);
    void didSplitTextNode(Text& oldNode);

    void clearDOMWindow() { m_domWindow = 0; }
    DOMWindow* domWindow() const { return m_domWindow; }

    // Helper functions for forwarding DOMWindow event related tasks to the DOMWindow if it exists.
    void setWindowAttributeEventListener(const AtomicString& eventType, PassRefPtr<EventListener>);
    EventListener* getWindowAttributeEventListener(const AtomicString& eventType);

    PassRefPtrWillBeRawPtr<Event> createEvent(const String& eventType, ExceptionState&);
    PassRefPtrWillBeRawPtr<Event> createEvent(ExceptionState&);

    // keep track of what types of event listeners are registered, so we don't
    // dispatch events unnecessarily
    enum ListenerType {
        DOMSUBTREEMODIFIED_LISTENER          = 1,
        DOMNODEINSERTED_LISTENER             = 1 << 1,
        DOMNODEREMOVED_LISTENER              = 1 << 2,
        DOMNODEREMOVEDFROMDOCUMENT_LISTENER  = 1 << 3,
        DOMNODEINSERTEDINTODOCUMENT_LISTENER = 1 << 4,
        DOMCHARACTERDATAMODIFIED_LISTENER    = 1 << 5,
        OVERFLOWCHANGED_LISTENER             = 1 << 6,
        ANIMATIONEND_LISTENER                = 1 << 7,
        ANIMATIONSTART_LISTENER              = 1 << 8,
        ANIMATIONITERATION_LISTENER          = 1 << 9,
        TRANSITIONEND_LISTENER               = 1 << 10,
        SCROLL_LISTENER                      = 1 << 12
        // 4 bits remaining
    };

    bool hasListenerType(ListenerType listenerType) const { return (m_listenerTypes & listenerType); }
    void addListenerTypeIfNeeded(const AtomicString& eventType);

    bool hasMutationObserversOfType(MutationObserver::MutationType type) const
    {
        return m_mutationObserverTypes & type;
    }
    bool hasMutationObservers() const { return m_mutationObserverTypes; }
    void addMutationObserverTypes(MutationObserverOptions types) { m_mutationObserverTypes |= types; }

    CSSStyleDeclaration* getOverrideStyle(Element*, const String& pseudoElt);

    /**
     * Handles a HTTP header equivalent set by a meta tag using <meta http-equiv="..." content="...">. This is called
     * when a meta tag is encountered during document parsing, and also when a script dynamically changes or adds a meta
     * tag. This enables scripts to use meta tags to perform refreshes and set expiry dates in addition to them being
     * specified in a HTML file.
     *
     * @param equiv The http header name (value of the meta tag's "equiv" attribute)
     * @param content The header value (value of the meta tag's "content" attribute)
     * @param inDocumentHeadElement Is the element in the document's <head> element?
     */
    void processHttpEquiv(const AtomicString& equiv, const AtomicString& content, bool inDocumentHeadElement);
    void updateViewportDescription();
    void processReferrerPolicy(const String& policy);

    // Returns the owning element in the parent document.
    // Returns 0 if this is the top level document.
    HTMLFrameOwnerElement* ownerElement() const;

    String title() const { return m_title; }
    void setTitle(const String&);

    Element* titleElement() const { return m_titleElement.get(); }
    void setTitleElement(const String& title, Element* titleElement);
    void removeTitle(Element* titleElement);

    String cookie(ExceptionState&) const;
    void setCookie(const String&, ExceptionState&);

    const AtomicString& referrer() const;

    String domain() const;
    void setDomain(const String& newDomain, ExceptionState&);

    String lastModified() const;

    // The cookieURL is used to query the cookie database for this document's
    // cookies. For example, if the cookie URL is http://example.com, we'll
    // use the non-Secure cookies for example.com when computing
    // document.cookie.
    //
    // Q: How is the cookieURL different from the document's URL?
    // A: The two URLs are the same almost all the time.  However, if one
    //    document inherits the security context of another document, it
    //    inherits its cookieURL but not its URL.
    //
    const KURL& cookieURL() const { return m_cookieURL; }
    void setCookieURL(const KURL& url) { m_cookieURL = url; }

    const KURL& firstPartyForCookies() const;

    // The following implements the rule from HTML 4 for what valid names are.
    // To get this right for all the XML cases, we probably have to improve this or move it
    // and make it sensitive to the type of document.
    static bool isValidName(const String&);

    // The following breaks a qualified name into a prefix and a local name.
    // It also does a validity check, and returns false if the qualified name
    // is invalid.  It also sets ExceptionCode when name is invalid.
    static bool parseQualifiedName(const AtomicString& qualifiedName, AtomicString& prefix, AtomicString& localName, ExceptionState&);

    // Checks to make sure prefix and namespace do not conflict (per DOM Core 3)
    static bool hasValidNamespaceForElements(const QualifiedName&);
    static bool hasValidNamespaceForAttributes(const QualifiedName&);

    HTMLElement* body() const;
    void setBody(PassRefPtr<HTMLElement>, ExceptionState&);

    HTMLHeadElement* head();

    // Decide which element is to define the viewport's overflow policy. If |rootStyle| is set, use
    // that as the style for the root element, rather than obtaining it on our own. The reason for
    // this is that style may not have been associated with the elements yet - in which case it may
    // have been calculated on the fly (without associating it with the actual element) somewhere.
    Element* viewportDefiningElement(RenderStyle* rootStyle = 0) const;

    DocumentMarkerController& markers() const { return *m_markers; }

    bool directionSetOnDocumentElement() const { return m_directionSetOnDocumentElement; }
    bool writingModeSetOnDocumentElement() const { return m_writingModeSetOnDocumentElement; }
    void setDirectionSetOnDocumentElement(bool b) { m_directionSetOnDocumentElement = b; }
    void setWritingModeSetOnDocumentElement(bool b) { m_writingModeSetOnDocumentElement = b; }

    bool execCommand(const String& command, bool userInterface = false, const String& value = String());
    bool queryCommandEnabled(const String& command);
    bool queryCommandIndeterm(const String& command);
    bool queryCommandState(const String& command);
    bool queryCommandSupported(const String& command);
    String queryCommandValue(const String& command);

    KURL openSearchDescriptionURL();

    // designMode support
    enum InheritedBool { off = false, on = true, inherit };
    void setDesignMode(InheritedBool value);
    InheritedBool getDesignMode() const;
    bool inDesignMode() const;

    Document* parentDocument() const;
    Document& topDocument() const;
    WeakPtr<Document> contextDocument();

    ScriptRunner* scriptRunner() { return m_scriptRunner.get(); }

    HTMLScriptElement* currentScript() const { return !m_currentScriptStack.isEmpty() ? m_currentScriptStack.last().get() : 0; }
    void pushCurrentScript(PassRefPtr<HTMLScriptElement>);
    void popCurrentScript();

    void applyXSLTransform(ProcessingInstruction* pi);
    PassRefPtr<Document> transformSourceDocument() { return m_transformSourceDocument; }
    void setTransformSourceDocument(Document* doc) { m_transformSourceDocument = doc; }

    void setTransformSource(PassOwnPtr<TransformSource>);
    TransformSource* transformSource() const { return m_transformSource.get(); }

    void incDOMTreeVersion() { ASSERT(!inStyleRecalc()); m_domTreeVersion = ++s_globalTreeVersion; }
    uint64_t domTreeVersion() const { return m_domTreeVersion; }

    enum PendingSheetLayout { NoLayoutWithPendingSheets, DidLayoutWithPendingSheets, IgnoreLayoutWithPendingSheets };

    bool didLayoutWithPendingStylesheets() const { return m_pendingSheetLayout == DidLayoutWithPendingSheets; }

    bool hasNodesWithPlaceholderStyle() const { return m_hasNodesWithPlaceholderStyle; }
    void setHasNodesWithPlaceholderStyle() { m_hasNodesWithPlaceholderStyle = true; }

    Vector<IconURL> iconURLs(int iconTypesMask);

    void setUseSecureKeyboardEntryWhenActive(bool);
    bool useSecureKeyboardEntryWhenActive() const;

    void updateFocusAppearanceSoon(bool restorePreviousSelection);
    void cancelFocusAppearanceUpdate();

    // Extension for manipulating canvas drawing contexts for use in CSS
    void getCSSCanvasContext(const String& type, const String& name, int width, int height, bool&, RefPtr<CanvasRenderingContext2D>&, bool&, RefPtr<WebGLRenderingContext>&);
    HTMLCanvasElement& getCSSCanvasElement(const String& name);

    bool isDNSPrefetchEnabled() const { return m_isDNSPrefetchEnabled; }
    void parseDNSPrefetchControlHeader(const String&);

    // FIXME(crbug.com/305497): This should be removed once DOMWindow is an ExecutionContext.
    virtual void postTask(PassOwnPtr<ExecutionContextTask>) OVERRIDE; // Executes the task on context's thread asynchronously.

    virtual void tasksWereSuspended() OVERRIDE FINAL;
    virtual void tasksWereResumed() OVERRIDE FINAL;
    virtual void suspendScheduledTasks() OVERRIDE FINAL;
    virtual void resumeScheduledTasks() OVERRIDE FINAL;
    virtual bool tasksNeedSuspension() OVERRIDE FINAL;

    void finishedParsing();

    void setEncodingData(const DocumentEncodingData& newData);
    const WTF::TextEncoding& encoding() const { return m_encodingData.encoding(); }

    bool encodingWasDetectedHeuristically() const { return m_encodingData.wasDetectedHeuristically(); }
    bool sawDecodingError() const { return m_encodingData.sawDecodingError(); }

    void setAnnotatedRegionsDirty(bool f) { m_annotatedRegionsDirty = f; }
    bool annotatedRegionsDirty() const { return m_annotatedRegionsDirty; }
    bool hasAnnotatedRegions () const { return m_hasAnnotatedRegions; }
    void setHasAnnotatedRegions(bool f) { m_hasAnnotatedRegions = f; }
    const Vector<AnnotatedRegionValue>& annotatedRegions() const;
    void setAnnotatedRegions(const Vector<AnnotatedRegionValue>&);

    virtual void removeAllEventListeners() OVERRIDE FINAL;

    const SVGDocumentExtensions* svgExtensions();
    SVGDocumentExtensions& accessSVGExtensions();

    void initSecurityContext();
    void initSecurityContext(const DocumentInit&);
    void initContentSecurityPolicy(const ContentSecurityPolicyResponseHeaders&);

    bool allowInlineEventHandlers(Node*, EventListener*, const String& contextURL, const WTF::OrdinalNumber& contextLine);
    bool allowExecutingScripts(Node*);

    void statePopped(PassRefPtr<SerializedScriptValue>);

    enum LoadEventProgress {
        LoadEventNotRun,
        LoadEventTried,
        LoadEventInProgress,
        LoadEventCompleted,
        BeforeUnloadEventInProgress,
        BeforeUnloadEventCompleted,
        PageHideInProgress,
        UnloadEventInProgress,
        UnloadEventHandled
    };
    bool loadEventStillNeeded() const { return m_loadEventProgress == LoadEventNotRun; }
    bool processingLoadEvent() const { return m_loadEventProgress == LoadEventInProgress; }
    bool loadEventFinished() const { return m_loadEventProgress >= LoadEventCompleted; }
    bool unloadStarted() const { return m_loadEventProgress >= PageHideInProgress; }

    void setContainsPlugins() { m_containsPlugins = true; }
    bool containsPlugins() const { return m_containsPlugins; }

    virtual bool isContextThread() const OVERRIDE FINAL;
    virtual bool isJSExecutionForbidden() const OVERRIDE FINAL { return false; }

    bool containsValidityStyleRules() const { return m_containsValidityStyleRules; }
    void setContainsValidityStyleRules() { m_containsValidityStyleRules = true; }

    void enqueueResizeEvent();
    void enqueueScrollEventForNode(Node*);
    void enqueueAnimationFrameEvent(PassRefPtrWillBeRawPtr<Event>);

    bool hasFullscreenElementStack() const { return m_hasFullscreenElementStack; }
    void setHasFullscreenElementStack() { m_hasFullscreenElementStack = true; }

    void webkitExitPointerLock();
    Element* webkitPointerLockElement() const;

    // Used to allow element that loads data without going through a FrameLoader to delay the 'load' event.
    void incrementLoadEventDelayCount() { ++m_loadEventDelayCount; }
    void decrementLoadEventDelayCount();
    bool isDelayingLoadEvent() const { return m_loadEventDelayCount; }
    void loadPluginsSoon();

    PassRefPtrWillBeRawPtr<Touch> createTouch(DOMWindow*, EventTarget*, int identifier, int pageX, int pageY, int screenX, int screenY, int radiusX, int radiusY, float rotationAngle, float force) const;
    PassRefPtrWillBeRawPtr<TouchList> createTouchList(WillBeHeapVector<RefPtrWillBeMember<Touch> >&) const;

    const DocumentTiming& timing() const { return m_documentTiming; }

    int requestAnimationFrame(PassOwnPtr<RequestAnimationFrameCallback>);
    void cancelAnimationFrame(int id);
    void serviceScriptedAnimations(double monotonicAnimationStartTime);

    virtual EventTarget* errorEventTarget() OVERRIDE FINAL;
    virtual void logExceptionToConsole(const String& errorMessage, const String& sourceURL, int lineNumber, int columnNumber, PassRefPtr<ScriptCallStack>) OVERRIDE FINAL;

    void initDNSPrefetch();

    bool hasTouchEventHandlers() const { return (m_touchEventTargets.get()) ? m_touchEventTargets->size() : false; }

    // Called when a single touch event handler has been added or removed for a node.
    // The Node should always be in this Document, except for child Documents which report
    // themselves to their parent exactly once if they have any touch handlers.
    // Handlers added/removed from the DOMWindow are reported as the Document.
    void didAddTouchEventHandler(Node*);
    void didRemoveTouchEventHandler(Node* handler) { didRemoveTouchEventHandler(handler, false); }

    // Called whenever all touch event handlers have been removed for a node (such as when the
    // node itself is being removed from the document).
    void didClearTouchEventHandlers(Node* handler) { didRemoveTouchEventHandler(handler, true); }

    const TouchEventTargetSet* touchEventTargets() const { return m_touchEventTargets.get(); }

    bool isInDocumentWrite() { return m_writeRecursionDepth > 0; }

    IntSize initialViewportSize() const;

    // There are currently two parallel autosizing implementations: TextAutosizer and FastTextAutosizer.
    // See http://tinyurl.com/chromium-fast-autosizer for more details.
    TextAutosizer* textAutosizer();
    FastTextAutosizer* fastTextAutosizer();

    PassRefPtr<Element> createElement(const AtomicString& localName, const AtomicString& typeExtension, ExceptionState&);
    PassRefPtr<Element> createElementNS(const AtomicString& namespaceURI, const AtomicString& qualifiedName, const AtomicString& typeExtension, ExceptionState&);
    ScriptValue registerElement(WebCore::NewScriptState*, const AtomicString& name, ExceptionState&);
    ScriptValue registerElement(WebCore::NewScriptState*, const AtomicString& name, const Dictionary& options, ExceptionState&, CustomElement::NameSet validNames = CustomElement::StandardNames);
    CustomElementRegistrationContext* registrationContext() { return m_registrationContext.get(); }

    void setImport(HTMLImport*);
    HTMLImport* import() const { return m_import; }
    bool haveImportsLoaded() const;
    void didLoadAllImports();

    void adjustFloatQuadsForScrollAndAbsoluteZoom(Vector<FloatQuad>&, RenderObject&);
    void adjustFloatRectForScrollAndAbsoluteZoom(FloatRect&, RenderObject&);

    bool hasActiveParser();
    unsigned activeParserCount() { return m_activeParserCount; }
    void incrementActiveParserCount() { ++m_activeParserCount; }
    void decrementActiveParserCount();

    void setContextFeatures(ContextFeatures&);
    ContextFeatures& contextFeatures() const { return *m_contextFeatures; }

    ElementDataCache* elementDataCache() { return m_elementDataCache.get(); }

    void didLoadAllScriptBlockingResources();
    void didRemoveAllPendingStylesheet();
    void setNeedsNotifyRemoveAllPendingStylesheet() { m_needsNotifyRemoveAllPendingStylesheet = true; }
    void clearStyleResolver();

    bool inStyleRecalc() const { return m_lifecycle.state() == DocumentLifecycle::InStyleRecalc; }

    // Return a Locale for the default locale if the argument is null or empty.
    Locale& getCachedLocale(const AtomicString& locale = nullAtom);

    AnimationClock& animationClock() { return *m_animationClock; }
    DocumentTimeline& timeline() const { return *m_timeline; }
    DocumentTimeline& transitionTimeline() const { return *m_transitionTimeline; }
    CSSPendingAnimations& cssPendingAnimations() { return m_cssPendingAnimations; }

    void addToTopLayer(Element*, const Element* before = 0);
    void removeFromTopLayer(Element*);
    const Vector<RefPtr<Element> >& topLayerElements() const { return m_topLayerElements; }
    HTMLDialogElement* activeModalDialog() const;

    // A non-null m_templateDocumentHost implies that |this| was created by ensureTemplateDocument().
    bool isTemplateDocument() const { return !!m_templateDocumentHost; }
    Document& ensureTemplateDocument();
    Document* templateDocumentHost() { return m_templateDocumentHost; }

    void didAssociateFormControl(Element*);

    void addConsoleMessageWithRequestIdentifier(MessageSource, MessageLevel, const String& message, unsigned long requestIdentifier);

    virtual DOMWindow* executingWindow() OVERRIDE FINAL;
    LocalFrame* executingFrame();

    DocumentLifecycleNotifier& lifecycleNotifier();
    DocumentLifecycle& lifecycle() { return m_lifecycle; }
    bool isActive() const { return m_lifecycle.isActive(); }
    bool isStopped() const { return m_lifecycle.state() == DocumentLifecycle::Stopped; }

    enum HttpRefreshType {
        HttpRefreshFromHeader,
        HttpRefreshFromMetaTag
    };
    void maybeHandleHttpRefresh(const String&, HttpRefreshType);

    void updateSecurityOrigin(PassRefPtr<SecurityOrigin>);
    PassOwnPtr<LifecycleNotifier<Document> > createLifecycleNotifier();

    void setHasViewportUnits() { m_hasViewportUnits = true; }
    bool hasViewportUnits() const { return m_hasViewportUnits; }
    void notifyResizeForViewportUnits();

    void registerVisibilityObserver(DocumentVisibilityObserver*);
    void unregisterVisibilityObserver(DocumentVisibilityObserver*);

    // FIXME: Remove this method once we have input routing in the browser
    // process. See http://crbug.com/339659.
    virtual void defaultEventHandler(Event*) OVERRIDE;

    void updateStyleInvalidationIfNeeded();

protected:
    Document(const DocumentInit&, DocumentClassFlags = DefaultDocumentClass);

    virtual void didUpdateSecurityOrigin() OVERRIDE FINAL;

    void clearXMLVersion() { m_xmlVersion = String(); }

    virtual void dispose() OVERRIDE;

    virtual PassRefPtr<Document> cloneDocumentWithoutChildren();

    bool importContainerNodeChildren(ContainerNode* oldContainerNode, PassRefPtr<ContainerNode> newContainerNode, ExceptionState&);

private:
    friend class Node;
    friend class IgnoreDestructiveWriteCountIncrementer;

    ScriptedAnimationController& ensureScriptedAnimationController();
    virtual SecurityContext& securityContext() OVERRIDE FINAL { return *this; }
    virtual EventQueue* eventQueue() const OVERRIDE FINAL;

    void scheduleRenderTreeUpdate();

    // FIXME: Rename the StyleRecalc state to RenderTreeUpdate.
    bool hasPendingStyleRecalc() const { return m_lifecycle.state() == DocumentLifecycle::StyleRecalcPending; }

    void inheritHtmlAndBodyElementStyles(StyleRecalcChange);

    bool dirtyElementsForLayerUpdate();
    void updateDistributionIfNeeded();
    void updateUseShadowTreesIfNeeded();
    void evaluateMediaQueryListIfNeeded();

    void updateRenderTree(StyleRecalcChange);
    void updateStyle(StyleRecalcChange);

    void detachParser();

    virtual bool isDocument() const OVERRIDE FINAL { return true; }

    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0) OVERRIDE;

    virtual String nodeName() const OVERRIDE FINAL;
    virtual NodeType nodeType() const OVERRIDE FINAL;
    virtual bool childTypeAllowed(NodeType) const OVERRIDE FINAL;
    virtual PassRefPtr<Node> cloneNode(bool deep = true) OVERRIDE FINAL;
    void cloneDataFromDocument(const Document&);

    virtual void refExecutionContext() OVERRIDE FINAL { ref(); }
    virtual void derefExecutionContext() OVERRIDE FINAL { deref(); }

    virtual const KURL& virtualURL() const OVERRIDE FINAL; // Same as url(), but needed for ExecutionContext to implement it without a performance loss for direct calls.
    virtual KURL virtualCompleteURL(const String&) const OVERRIDE FINAL; // Same as completeURL() for the same reason as above.

    virtual void reportBlockedScriptExecutionToInspector(const String& directiveText) OVERRIDE FINAL;
    virtual void addMessage(MessageSource, MessageLevel, const String& message, const String& sourceURL, unsigned lineNumber, ScriptState*) OVERRIDE FINAL;
    void internalAddMessage(MessageSource, MessageLevel, const String& message, const String& sourceURL, unsigned lineNumber, PassRefPtr<ScriptCallStack>, ScriptState*);

    virtual double timerAlignmentInterval() const OVERRIDE FINAL;

    void updateTitle(const String&);
    void updateFocusAppearanceTimerFired(Timer<Document>*);
    void updateBaseURL();

    void executeScriptsWaitingForResourcesIfNeeded();

    PassRefPtr<NodeList> handleZeroPadding(const HitTestRequest&, HitTestResult&) const;

    void loadEventDelayTimerFired(Timer<Document>*);
    void pluginLoadingTimerFired(Timer<Document>*);

    PageVisibilityState pageVisibilityState() const;

    PassRefPtr<HTMLCollection> ensureCachedCollection(CollectionType);

    // Note that dispatching a window load event may cause the DOMWindow to be detached from
    // the LocalFrame, so callers should take a reference to the DOMWindow (which owns us) to
    // prevent the Document from getting blown away from underneath them.
    void dispatchWindowLoadEvent();

    void addListenerType(ListenerType listenerType) { m_listenerTypes |= listenerType; }
    void addMutationEventListenerTypeIfEnabled(ListenerType);

    void didAssociateFormControlsTimerFired(Timer<Document>*);

    void clearFocusedElementSoon();
    void clearFocusedElementTimerFired(Timer<Document>*);

    void processHttpEquivDefaultStyle(const AtomicString& content);
    void processHttpEquivRefresh(const AtomicString& content);
    void processHttpEquivSetCookie(const AtomicString& content);
    void processHttpEquivXFrameOptions(const AtomicString& content);
    void processHttpEquivContentSecurityPolicy(const AtomicString& equiv, const AtomicString& content);

    void didRemoveTouchEventHandler(Node*, bool clearAll);

    bool needsRenderTreeUpdate() const;
    bool shouldScheduleRenderTreeUpdate() const;
    bool haveStylesheetsLoaded() const;

    DocumentLifecycle m_lifecycle;

    bool m_hasNodesWithPlaceholderStyle;
    bool m_needsNotifyRemoveAllPendingStylesheet;
    bool m_evaluateMediaQueriesOnStyleRecalc;

    // If we do ignore the pending stylesheet count, then we need to add a boolean
    // to track that this happened so that we can do a full repaint when the stylesheets
    // do eventually load.
    PendingSheetLayout m_pendingSheetLayout;

    LocalFrame* m_frame;
    DOMWindow* m_domWindow;
    HTMLImport* m_import;

    RefPtr<ResourceFetcher> m_fetcher;
    RefPtr<DocumentParser> m_parser;
    unsigned m_activeParserCount;
    RefPtr<ContextFeatures> m_contextFeatures;

    bool m_wellFormed;

    // Document URLs.
    KURL m_url; // Document.URL: The URL from which this document was retrieved.
    KURL m_baseURL; // Node.baseURI: The URL to use when resolving relative URLs.
    KURL m_baseURLOverride; // An alternative base URL that takes precedence over m_baseURL (but not m_baseElementURL).
    KURL m_baseElementURL; // The URL set by the <base> element.
    KURL m_cookieURL; // The URL to use for cookie access.

    AtomicString m_baseTarget;

    // Mime-type of the document in case it was cloned or created by XHR.
    AtomicString m_mimeType;

    RefPtr<DocumentType> m_docType;
    OwnPtr<DOMImplementation> m_implementation;

    RefPtrWillBePersistent<CSSStyleSheet> m_elemSheet;

    bool m_printing;
    bool m_paginatedForScreen;

    CompatibilityMode m_compatibilityMode;
    bool m_compatibilityModeLocked; // This is cheaper than making setCompatibilityMode virtual.

    bool m_hasAutofocused;
    Timer<Document> m_clearFocusedElementTimer;
    RefPtr<Element> m_autofocusElement;
    RefPtr<Element> m_focusedElement;
    RefPtr<Node> m_hoverNode;
    RefPtr<Element> m_activeHoverElement;
    RefPtr<Element> m_documentElement;
    UserActionElementSet m_userActionElements;

    uint64_t m_domTreeVersion;
    static uint64_t s_globalTreeVersion;

    HashSet<NodeIterator*> m_nodeIterators;
    HashSet<Range*> m_ranges;

    unsigned short m_listenerTypes;

    MutationObserverOptions m_mutationObserverTypes;

    OwnPtrWillBePersistent<StyleEngine> m_styleEngine;
    RefPtrWillBePersistent<StyleSheetList> m_styleSheetList;

    OwnPtr<FormController> m_formController;

    TextLinkColors m_textLinkColors;
    const OwnPtr<VisitedLinkState> m_visitedLinkState;

    bool m_visuallyOrdered;
    ReadyState m_readyState;
    bool m_isParsing;
    bool m_historyItemDocumentStateDirty;

    bool m_gotoAnchorNeededAfterStylesheetsLoad;
    bool m_isDNSPrefetchEnabled;
    bool m_haveExplicitlyDisabledDNSPrefetch;
    bool m_containsValidityStyleRules;
    bool m_updateFocusAppearanceRestoresSelection;
    bool m_containsPlugins;

    // http://www.whatwg.org/specs/web-apps/current-work/#ignore-destructive-writes-counter
    unsigned m_ignoreDestructiveWriteCount;

    String m_title;
    String m_rawTitle;
    bool m_titleSetExplicitly;
    RefPtr<Element> m_titleElement;

    OwnPtr<AXObjectCache> m_axObjectCache;
    OwnPtr<DocumentMarkerController> m_markers;

    Timer<Document> m_updateFocusAppearanceTimer;

    Element* m_cssTarget;

    LoadEventProgress m_loadEventProgress;

    double m_startTime;

    OwnPtr<ScriptRunner> m_scriptRunner;

    Vector<RefPtr<HTMLScriptElement> > m_currentScriptStack;

    OwnPtr<TransformSource> m_transformSource;
    RefPtr<Document> m_transformSourceDocument;

    String m_xmlEncoding;
    String m_xmlVersion;
    unsigned m_xmlStandalone : 2;
    unsigned m_hasXMLDeclaration : 1;

    AtomicString m_contentLanguage;

    DocumentEncodingData m_encodingData;

    InheritedBool m_designMode;

    HashSet<LiveNodeListBase*> m_listsInvalidatedAtDocument;
    unsigned m_nodeListCounts[numNodeListInvalidationTypes];

    OwnPtr<SVGDocumentExtensions> m_svgExtensions;

    Vector<AnnotatedRegionValue> m_annotatedRegions;
    bool m_hasAnnotatedRegions;
    bool m_annotatedRegionsDirty;

    HashMap<String, RefPtr<HTMLCanvasElement> > m_cssCanvasElements;

    OwnPtr<SelectorQueryCache> m_selectorQueryCache;

    bool m_useSecureKeyboardEntryWhenActive;

    DocumentClassFlags m_documentClasses;

    bool m_isViewSource;
    bool m_sawElementsInKnownNamespaces;
    bool m_isSrcdocDocument;
    bool m_isMobileDocument;

    RenderView* m_renderView;

    WeakPtrFactory<Document> m_weakFactory;
    WeakPtr<Document> m_contextDocument;

    bool m_hasFullscreenElementStack; // For early return in FullscreenElementStack::fromIfExists()

    Vector<RefPtr<Element> > m_topLayerElements;

    int m_loadEventDelayCount;
    Timer<Document> m_loadEventDelayTimer;
    Timer<Document> m_pluginLoadingTimer;

    ViewportDescription m_viewportDescription;
    ViewportDescription m_legacyViewportDescription;
    Length m_viewportDefaultMinWidth;

    bool m_didSetReferrerPolicy;
    ReferrerPolicy m_referrerPolicy;

    bool m_directionSetOnDocumentElement;
    bool m_writingModeSetOnDocumentElement;
    DocumentTiming m_documentTiming;
    RefPtrWillBePersistent<MediaQueryMatcher> m_mediaQueryMatcher;
    bool m_writeRecursionIsTooDeep;
    unsigned m_writeRecursionDepth;

    OwnPtr<TouchEventTargetSet> m_touchEventTargets;

    RefPtr<ScriptedAnimationController> m_scriptedAnimationController;
    OwnPtr<MainThreadTaskRunner> m_taskRunner;
    OwnPtr<TextAutosizer> m_textAutosizer;
    OwnPtr<FastTextAutosizer> m_fastTextAutosizer;

    RefPtr<CustomElementRegistrationContext> m_registrationContext;

    void elementDataCacheClearTimerFired(Timer<Document>*);
    Timer<Document> m_elementDataCacheClearTimer;

    OwnPtr<ElementDataCache> m_elementDataCache;

#ifndef NDEBUG
    bool m_didDispatchViewportPropertiesChanged;
#endif

    typedef HashMap<AtomicString, OwnPtr<Locale> > LocaleIdentifierToLocaleMap;
    LocaleIdentifierToLocaleMap m_localeCache;

    OwnPtr<AnimationClock> m_animationClock;
    RefPtr<DocumentTimeline> m_timeline;
    RefPtr<DocumentTimeline> m_transitionTimeline;
    CSSPendingAnimations m_cssPendingAnimations;

    RefPtr<Document> m_templateDocument;
    Document* m_templateDocumentHost; // Manually managed weakref (backpointer from m_templateDocument).

    Timer<Document> m_didAssociateFormControlsTimer;
    HashSet<RefPtr<Element> > m_associatedFormControls;

    HashSet<SVGUseElement*> m_useElementsNeedingUpdate;
    HashSet<Element*> m_layerUpdateElements;

    bool m_hasViewportUnits;

    HashSet<DocumentVisibilityObserver*> m_visibilityObservers;
};

inline void Document::notifyRemovePendingSheetIfNeeded()
{
    if (m_needsNotifyRemoveAllPendingStylesheet)
        didRemoveAllPendingStylesheet();
}

inline bool Document::shouldOverrideLegacyDescription(ViewportDescription::Type origin)
{
    // The different (legacy) meta tags have different priorities based on the type
    // regardless of which order they appear in the DOM. The priority is given by the
    // ViewportDescription::Type enum.
    return origin >= m_legacyViewportDescription.type;
}

inline void Document::incrementNodeListWithIdNameCacheCount()
{
    m_nodeListCounts[InvalidateOnIdNameAttrChange]++;
}

inline void Document::decrementNodeListWithIdNameCacheCount()
{
    ASSERT(m_nodeListCounts[InvalidateOnIdNameAttrChange] > 0);
    m_nodeListCounts[InvalidateOnIdNameAttrChange]--;
}

inline void Document::scheduleRenderTreeUpdateIfNeeded()
{
    // Inline early out to avoid the function calls below.
    if (hasPendingStyleRecalc())
        return;
    if (shouldScheduleRenderTreeUpdate() && needsRenderTreeUpdate())
        scheduleRenderTreeUpdate();
}

DEFINE_TYPE_CASTS(Document, ExecutionContextClient, client, client->isDocument(), client.isDocument());
DEFINE_TYPE_CASTS(Document, ExecutionContext, context, context->isDocument(), context.isDocument());
DEFINE_NODE_TYPE_CASTS(Document, isDocumentNode());

#define DEFINE_DOCUMENT_TYPE_CASTS(thisType) \
    DEFINE_TYPE_CASTS(thisType, Document, document, document->is##thisType(), document.is##thisType())

// All these varations are needed to avoid ambiguous overloads with the Node and TreeScope versions.
inline bool operator==(const Document& a, const Document& b) { return &a == &b; }
inline bool operator==(const Document& a, const Document* b) { return &a == b; }
inline bool operator==(const Document* a, const Document& b) { return a == &b; }
inline bool operator!=(const Document& a, const Document& b) { return !(a == b); }
inline bool operator!=(const Document& a, const Document* b) { return !(a == b); }
inline bool operator!=(const Document* a, const Document& b) { return !(a == b); }

// Put these methods here, because they require the Document definition, but we really want to inline them.

inline bool Node::isDocumentNode() const
{
    return this == document();
}

Node* eventTargetNodeForDocument(Document*);

} // namespace WebCore

#endif // Document_h
