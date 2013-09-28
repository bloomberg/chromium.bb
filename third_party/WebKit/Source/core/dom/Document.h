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
#include "core/dom/ContainerNode.h"
#include "core/dom/CustomElement.h"
#include "core/dom/DOMTimeStamp.h"
#include "core/dom/DocumentInit.h"
#include "core/dom/DocumentTiming.h"
#include "core/dom/IconURL.h"
#include "core/dom/MutationObserver.h"
#include "core/dom/QualifiedName.h"
#include "core/dom/ScriptExecutionContext.h"
#include "core/dom/TextLinkColors.h"
#include "core/dom/TreeScope.h"
#include "core/dom/UserActionElementSet.h"
#include "core/dom/ViewportArguments.h"
#include "core/html/CollectionType.h"
#include "core/page/FocusDirection.h"
#include "core/page/PageVisibilityState.h"
#include "weborigin/ReferrerPolicy.h"
#include "core/platform/Timer.h"
#include "core/rendering/HitTestRequest.h"
#include "wtf/HashSet.h"
#include "wtf/OwnPtr.h"
#include "wtf/PassOwnPtr.h"
#include "wtf/PassRefPtr.h"
#include "wtf/WeakPtr.h"

namespace WebCore {

class AXObjectCache;
class Attr;
class CDATASection;
class CSSStyleDeclaration;
class CSSStyleSheet;
class CSSStyleSheetResource;
class CanvasRenderingContext;
class CharacterData;
class Chrome;
class Comment;
class ContentSecurityPolicyResponseHeaders;
class ContextFeatures;
class CustomElementRegistrationContext;
class DOMImplementation;
class DOMNamedFlowCollection;
class DOMSecurityPolicy;
class DOMSelection;
class DOMWindow;
class DOMWrapperWorld;
class Database;
class DatabaseThread;
class DocumentEventQueue;
class DocumentFragment;
class DocumentLifecycleNotifier;
class DocumentLifecycleObserver;
class DocumentLoader;
class DocumentMarkerController;
class DocumentParser;
class DocumentSharedObjectPool;
class DocumentTimeline;
class DocumentType;
class Element;
class Event;
class EventListener;
class ExceptionState;
class FloatQuad;
class FloatRect;
class FormController;
class Frame;
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
class MediaQueryList;
class MediaQueryMatcher;
class MouseEventWithHitTestResults;
class NamedFlowCollection;
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
class TextResourceDecoder;
class Touch;
class TouchList;
class TransformSource;
class TreeWalker;
class VisitedLinkState;
class XMLHttpRequest;

struct AnnotatedRegionValue;

class FontFaceSet;

typedef int ExceptionCode;

enum PageshowEventPersistence {
    PageshowEventNotPersisted = 0,
    PageshowEventPersisted = 1
};

enum StyleResolverUpdateType { RecalcStyleImmediately, RecalcStyleDeferred };

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
    InvalidateOnItemAttrChange,
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
};

typedef unsigned char DocumentClassFlags;

class Document : public ContainerNode, public TreeScope, public ScriptExecutionContext {
public:
    static PassRefPtr<Document> create(const DocumentInit& initializer = DocumentInit())
    {
        return adoptRef(new Document(initializer));
    }
    static PassRefPtr<Document> createXHTML(const DocumentInit& initializer = DocumentInit())
    {
        return adoptRef(new Document(initializer, XHTMLDocumentClass));
    }
    virtual ~Document();

    MediaQueryMatcher* mediaQueryMatcher();

    using ContainerNode::ref;
    using ContainerNode::deref;

    virtual bool canContainRangeEndPoint() const { return true; }

    SelectorQueryCache* selectorQueryCache();

    // DOM methods & attributes for Document

    DEFINE_ATTRIBUTE_EVENT_LISTENER(abort);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(change);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(click);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(contextmenu);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(dblclick);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(dragenter);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(dragover);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(dragleave);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(drop);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(dragstart);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(drag);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(dragend);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(input);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(invalid);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(keydown);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(keypress);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(keyup);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(mousedown);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(mouseenter);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(mouseleave);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(mousemove);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(mouseout);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(mouseover);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(mouseup);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(mousewheel);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(scroll);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(select);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(submit);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(wheel);

    DEFINE_ATTRIBUTE_EVENT_LISTENER(blur);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(error);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(focus);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(load);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(readystatechange);

    // WebKit extensions
    DEFINE_ATTRIBUTE_EVENT_LISTENER(beforecut);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(cut);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(beforecopy);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(copy);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(beforepaste);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(paste);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(reset);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(search);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(selectstart);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(selectionchange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(touchstart);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(touchmove);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(touchend);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(touchcancel);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitfullscreenchange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitfullscreenerror);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitpointerlockchange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitpointerlockerror);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitvisibilitychange);
    DEFINE_ATTRIBUTE_EVENT_LISTENER(securitypolicyviolation);

    bool shouldOverrideLegacyViewport(ViewportArguments::Type);
    void setViewportArguments(const ViewportArguments&);
    const ViewportArguments& viewportArguments() const { return m_viewportArguments; }
#ifndef NDEBUG
    bool didDispatchViewportPropertiesChanged() const { return m_didDispatchViewportPropertiesChanged; }
#endif
    bool hasLegacyViewportTag() const { return m_legacyViewportArguments.isLegacyViewportType(); }

    void setReferrerPolicy(ReferrerPolicy referrerPolicy) { m_referrerPolicy = referrerPolicy; }
    ReferrerPolicy referrerPolicy() const { return m_referrerPolicy; }

    void setDoctype(PassRefPtr<DocumentType>);
    DocumentType* doctype() const { return m_docType.get(); }

    DOMImplementation* implementation();

    Element* documentElement() const
    {
        return m_documentElement.get();
    }

    bool hasManifest() const;

    PassRefPtr<Element> createElement(const AtomicString& name, ExceptionState&);
    PassRefPtr<DocumentFragment> createDocumentFragment();
    PassRefPtr<Text> createTextNode(const String& data);
    PassRefPtr<Comment> createComment(const String& data);
    PassRefPtr<CDATASection> createCDATASection(const String& data, ExceptionState&);
    PassRefPtr<ProcessingInstruction> createProcessingInstruction(const String& target, const String& data, ExceptionState&);
    PassRefPtr<Attr> createAttribute(const String& name, ExceptionState&);
    PassRefPtr<Attr> createAttributeNS(const String& namespaceURI, const String& qualifiedName, ExceptionState&, bool shouldIgnoreNamespaceChecks = false);
    PassRefPtr<Node> importNode(Node* importedNode, ExceptionState& ec) { return importNode(importedNode, true, ec); }
    PassRefPtr<Node> importNode(Node* importedNode, bool deep, ExceptionState&);
    PassRefPtr<Element> createElementNS(const String& namespaceURI, const String& qualifiedName, ExceptionState&);
    PassRefPtr<Element> createElement(const QualifiedName&, bool createdByParser);

    bool cssStickyPositionEnabled() const;
    bool cssCompositingEnabled() const;
    PassRefPtr<DOMNamedFlowCollection> webkitGetNamedFlows();

    NamedFlowCollection* namedFlows();

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
        HitTestRequest::HitTestRequestType hitType = HitTestRequest::ReadOnly | HitTestRequest::Active | HitTestRequest::DisallowShadowContent) const;
    Element* elementFromPoint(int x, int y) const;
    PassRefPtr<Range> caretRangeFromPoint(int x, int y);

    String readyState() const;

    String defaultCharset() const;

    String inputEncoding() const { return Document::encodingName(); }
    String charset() const { return Document::encodingName(); }
    String characterSet() const { return Document::encodingName(); }

    String encodingName() const;

    void setCharset(const String&);

    void setContent(const String&);

    String suggestedMIMEType() const;

    String contentLanguage() const { return m_contentLanguage; }
    void setContentLanguage(const String&);

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

    String documentURI() const { return m_documentURI; }

    virtual KURL baseURI() const;

    String webkitVisibilityState() const;
    bool webkitHidden() const;
    void dispatchVisibilityStateChangeEvent();

    DOMSecurityPolicy* securityPolicy();

    PassRefPtr<Node> adoptNode(PassRefPtr<Node> source, ExceptionState&);

    PassRefPtr<HTMLCollection> images();
    PassRefPtr<HTMLCollection> embeds();
    PassRefPtr<HTMLCollection> plugins(); // an alias for embeds() required for the JS DOM bindings.
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
    bool isImageDocument() const { return m_documentClasses & ImageDocumentClass; }
    bool isSVGDocument() const { return m_documentClasses & SVGDocumentClass; }
    bool isPluginDocument() const { return m_documentClasses & PluginDocumentClass; }
    bool isMediaDocument() const { return m_documentClasses & MediaDocumentClass; }

    bool hasSVGRootNode() const;

    bool isFrameSet() const;

    bool isSrcdocDocument() const { return m_isSrcdocDocument; }
    bool isMobileDocument() const { return m_isMobileDocument; }

    StyleResolver* styleResolverIfExists() const { return m_styleResolver.get(); }

    bool isViewSource() const { return m_isViewSource; }
    void setIsViewSource(bool);

    bool sawElementsInKnownNamespaces() const { return m_sawElementsInKnownNamespaces; }

    StyleResolver* styleResolver()
    {
        if (!m_styleResolver)
            createStyleResolver();
        return m_styleResolver.get();
    }

    void notifyRemovePendingSheetIfNeeded();

    bool haveStylesheetsLoaded() const;
    bool haveStylesheetsAndImportsLoaded() const { return haveImportsLoaded() && haveStylesheetsLoaded(); }

    // This is a DOM function.
    StyleSheetList* styleSheets();

    StyleEngine* styleEngine() { return m_styleEngine.get(); }

    bool gotoAnchorNeededAfterStylesheetsLoad() { return m_gotoAnchorNeededAfterStylesheetsLoad; }
    void setGotoAnchorNeededAfterStylesheetsLoad(bool b) { m_gotoAnchorNeededAfterStylesheetsLoad = b; }

    // Called when one or more stylesheets in the document may have been added, removed, or changed.
    void styleResolverChanged(StyleResolverUpdateType, StyleResolverUpdateMode = FullStyleUpdate);

    // FIXME: Switch all callers of styleResolverChanged to these or better ones and then make them
    // do something smarter.
    void removedStyleSheet(StyleSheet*, StyleResolverUpdateType type = RecalcStyleDeferred) { styleResolverChanged(type); }
    void addedStyleSheet(StyleSheet*, StyleResolverUpdateType type = RecalcStyleDeferred) { styleResolverChanged(type); }
    void modifiedStyleSheet(StyleSheet*, StyleResolverUpdateType type = RecalcStyleDeferred) { styleResolverChanged(type); }

    void didAccessStyleResolver() { ++m_styleResolverAccessCount; }

    void evaluateMediaQueryList();

    // Never returns 0.
    FormController* formController();
    Vector<String> formElementsState() const;
    void setStateForNewFormElements(const Vector<String>&);

    FrameView* view() const; // can be null
    Frame* frame() const { return m_frame; } // can be null
    Page* page() const; // can be null
    Settings* settings() const; // can be null

    float devicePixelRatio() const;

    PassRefPtr<Range> createRange();

    PassRefPtr<NodeIterator> createNodeIterator(Node* root, ExceptionState&);
    PassRefPtr<NodeIterator> createNodeIterator(Node* root, unsigned whatToShow, ExceptionState&);
    PassRefPtr<NodeIterator> createNodeIterator(Node* root, unsigned whatToShow, PassRefPtr<NodeFilter>, ExceptionState&);
    PassRefPtr<NodeIterator> createNodeIterator(Node* root, unsigned whatToShow, PassRefPtr<NodeFilter>, bool expandEntityReferences, ExceptionState&);

    PassRefPtr<TreeWalker> createTreeWalker(Node* root, ExceptionState&);
    PassRefPtr<TreeWalker> createTreeWalker(Node* root, unsigned whatToShow, ExceptionState&);
    PassRefPtr<TreeWalker> createTreeWalker(Node* root, unsigned whatToShow, PassRefPtr<NodeFilter>, ExceptionState&);
    PassRefPtr<TreeWalker> createTreeWalker(Node* root, unsigned whatToShow, PassRefPtr<NodeFilter>, bool expandEntityReferences, ExceptionState&);

    // Special support for editing
    PassRefPtr<CSSStyleDeclaration> createCSSStyleDeclaration();
    PassRefPtr<Text> createEditingTextNode(const String&);

    void setStyleDependentState(RenderStyle* documentStyle);
    void inheritHtmlAndBodyElementStyles(StyleRecalcChange);
    void recalcStyle(StyleRecalcChange);
    void updateStyleIfNeeded();
    void updateStyleForNodeIfNeeded(Node*);
    void updateLayout();
    void updateLayoutIgnorePendingStylesheets();
    void partialUpdateLayoutIgnorePendingStylesheets(Node*);
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

    // Implemented in RenderView.h to avoid a cyclic header dependency this just
    // returns renderer so callers can avoid verbose casts.
    RenderView* renderView() const;

    // Shadow the implementations on Node to provide faster access for documents.
    RenderObject* renderer() const { return m_renderer; }
    void setRenderer(RenderObject* renderer)
    {
        m_renderer = renderer;
        Node::setRenderer(renderer);
    }

    AXObjectCache* existingAXObjectCache() const;
    AXObjectCache* axObjectCache() const;
    void clearAXObjectCache();

    // to get visually ordered hebrew and arabic pages right
    void setVisuallyOrdered();
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

    bool dispatchBeforeUnloadEvent(Chrome&, Document* navigatingDocument);
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
    const String& baseTarget() const { return m_baseTarget; }
    void processBaseElement();

    KURL completeURL(const String&) const;
    KURL completeURL(const String&, const KURL& baseURLOverride) const;

    virtual String userAgent(const KURL&) const;

    virtual void disableEval(const String& errorMessage);

    bool canNavigate(Frame* targetFrame);
    Frame* findUnsafeParentScrollPropagationBoundary();

    CSSStyleSheet* elementSheet();

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
    void setParsing(bool);
    bool parsing() const { return m_bParsing; }
    int minimumLayoutDelay();

    bool shouldScheduleLayout();
    bool shouldParserYieldAgressivelyBeforeScriptExecution();
    int elapsedTime() const;

    TextLinkColors& textLinkColors() { return m_textLinkColors; }
    VisitedLinkState* visitedLinkState() const { return m_visitedLinkState.get(); }

    MouseEventWithHitTestResults prepareMouseEvent(const HitTestRequest&, const LayoutPoint&, const PlatformMouseEvent&);

    /* Newly proposed CSS3 mechanism for selecting alternate
       stylesheets using the DOM. May be subject to change as
       spec matures. - dwh
    */
    String preferredStylesheetSet() const;
    String selectedStylesheetSet() const;
    void setSelectedStylesheetSet(const String&);

    bool setFocusedElement(PassRefPtr<Element>, FocusDirection = FocusDirectionNone);
    Element* focusedElement() const { return m_focusedElement.get(); }
    UserActionElementSet& userActionElements()  { return m_userActionElements; }
    const UserActionElementSet& userActionElements() const { return m_userActionElements; }
    void setNeedsFocusedElementCheck();
    void didRunCheckFocusedElementTask() { m_didPostCheckFocusedElementTask = false; }

    // The m_ignoreAutofocus flag specifies whether or not the document has been changed by the user enough
    // for WebCore to ignore the autofocus attribute on any form controls
    bool ignoreAutofocus() const { return m_ignoreAutofocus; };
    void setIgnoreAutofocus(bool shouldIgnore = true) { m_ignoreAutofocus = shouldIgnore; };

    void setHoverNode(PassRefPtr<Node>);
    Node* hoverNode() const { return m_hoverNode.get(); }

    void setActiveElement(PassRefPtr<Element>);
    Element* activeElement() const { return m_activeElement.get(); }

    void removeFocusedElementOfSubtree(Node*, bool amongChildrenOnly = false);
    void hoveredNodeDetached(Node*);
    void activeChainNodeDetached(Node*);

    void updateHoverActiveState(const HitTestRequest&, Element*, const PlatformMouseEvent* = 0);

    // Updates for :target (CSS3 selector).
    void setCSSTarget(Element*);
    Element* cssTarget() const { return m_cssTarget; }

    void scheduleStyleRecalc();
    void unscheduleStyleRecalc();
    bool hasPendingStyleRecalc() const;
    bool hasPendingForcedStyleRecalc() const;
    void styleRecalcTimerFired(Timer<Document>*);

    void registerNodeList(LiveNodeListBase*);
    void unregisterNodeList(LiveNodeListBase*);
    bool shouldInvalidateNodeListCaches(const QualifiedName* attrName = 0) const;
    void invalidateNodeListCaches(const QualifiedName* attrName);

    void attachNodeIterator(NodeIterator*);
    void detachNodeIterator(NodeIterator*);
    void moveNodeIteratorsToNewDocument(Node*, Document*);

    void attachRange(Range*);
    void detachRange(Range*);

    void updateRangesAfterChildrenChanged(ContainerNode*);
    // nodeChildrenWillBeRemoved is used when removing all node children at once.
    void nodeChildrenWillBeRemoved(ContainerNode*);
    // nodeWillBeRemoved is only safe when removing one node at a time.
    void nodeWillBeRemoved(Node*);
    bool canReplaceChild(Node* newChild, Node* oldChild);

    void didInsertText(Node*, unsigned offset, unsigned length);
    void didRemoveText(Node*, unsigned offset, unsigned length);
    void didMergeTextNodes(Text* oldNode, unsigned offset);
    void didSplitTextNode(Text* oldNode);

    void setDOMWindow(DOMWindow* domWindow) { m_domWindow = domWindow; }
    DOMWindow* domWindow() const { return m_domWindow; }
    // In DOM Level 2, the Document's DOMWindow is called the defaultView.
    DOMWindow* defaultView() const { return domWindow(); }

    // Helper functions for forwarding DOMWindow event related tasks to the DOMWindow if it exists.
    void setWindowAttributeEventListener(const AtomicString& eventType, PassRefPtr<EventListener>, DOMWrapperWorld* isolatedWorld = 0);
    EventListener* getWindowAttributeEventListener(const AtomicString& eventType, DOMWrapperWorld* isolatedWorld);
    void dispatchWindowEvent(PassRefPtr<Event>, PassRefPtr<EventTarget> = 0);

    PassRefPtr<Event> createEvent(const String& eventType, ExceptionState&);

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
        BEFORELOAD_LISTENER                  = 1 << 11,
        SCROLL_LISTENER                      = 1 << 12
        // 3 bits remaining
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
     */
    void processHttpEquiv(const String& equiv, const String& content);
    void processViewport(const String& features, ViewportArguments::Type origin);
    void updateViewportArguments();
    void processReferrerPolicy(const String& policy);

    // Returns the owning element in the parent document.
    // Returns 0 if this is the top level document.
    HTMLFrameOwnerElement* ownerElement() const;

    HTMLIFrameElement* seamlessParentIFrame() const;
    bool shouldDisplaySeamlesslyWithParent() const;

    String title() const { return m_title; }
    void setTitle(const String&);

    Element* titleElement() const { return m_titleElement.get(); }
    void setTitleElement(const String& title, Element* titleElement);
    void removeTitle(Element* titleElement);

    String cookie(ExceptionState&) const;
    void setCookie(const String&, ExceptionState&);

    String referrer() const;

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
    static bool parseQualifiedName(const String& qualifiedName, String& prefix, String& localName, ExceptionState&);

    // Checks to make sure prefix and namespace do not conflict (per DOM Core 3)
    static bool hasValidNamespaceForElements(const QualifiedName&);
    static bool hasValidNamespaceForAttributes(const QualifiedName&);

    HTMLElement* body() const;
    void setBody(PassRefPtr<HTMLElement>, ExceptionState&);

    HTMLHeadElement* head();

    DocumentMarkerController* markers() const { return m_markers.get(); }

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
    Document* topDocument() const;
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

    void incDOMTreeVersion() { m_domTreeVersion = ++s_globalTreeVersion; }
    uint64_t domTreeVersion() const { return m_domTreeVersion; }

    enum PendingSheetLayout { NoLayoutWithPendingSheets, DidLayoutWithPendingSheets, IgnoreLayoutWithPendingSheets };

    bool didLayoutWithPendingStylesheets() const { return m_pendingSheetLayout == DidLayoutWithPendingSheets; }

    bool hasNodesWithPlaceholderStyle() const { return m_hasNodesWithPlaceholderStyle; }
    void setHasNodesWithPlaceholderStyle() { m_hasNodesWithPlaceholderStyle = true; }

    const Vector<IconURL>& shortcutIconURLs();
    const Vector<IconURL>& iconURLs(int iconTypesMask);

    void setUseSecureKeyboardEntryWhenActive(bool);
    bool useSecureKeyboardEntryWhenActive() const;

    void updateFocusAppearanceSoon(bool restorePreviousSelection);
    void cancelFocusAppearanceUpdate();

    // Extension for manipulating canvas drawing contexts for use in CSS
    CanvasRenderingContext* getCSSCanvasContext(const String& type, const String& name, int width, int height);
    HTMLCanvasElement* getCSSCanvasElement(const String& name);

    bool isDNSPrefetchEnabled() const { return m_isDNSPrefetchEnabled; }
    void parseDNSPrefetchControlHeader(const String&);

    virtual void postTask(PassOwnPtr<Task>); // Executes the task on context's thread asynchronously.

    void suspendScriptedAnimationControllerCallbacks();
    void resumeScriptedAnimationControllerCallbacks();

    void finishedParsing();

    void documentWillBecomeInactive();

    void setDecoder(PassRefPtr<TextResourceDecoder>);
    TextResourceDecoder* decoder() const { return m_decoder.get(); }

    void setEncoding(const WTF::TextEncoding&);
    const WTF::TextEncoding& encoding() const { return m_encoding; }

    String displayStringModifiedByEncoding(const String&) const;
    PassRefPtr<StringImpl> displayStringModifiedByEncoding(PassRefPtr<StringImpl>) const;
    void displayBufferModifiedByEncoding(LChar* buffer, unsigned len) const
    {
        displayBufferModifiedByEncodingInternal(buffer, len);
    }
    void displayBufferModifiedByEncoding(UChar* buffer, unsigned len) const
    {
        displayBufferModifiedByEncodingInternal(buffer, len);
    }

    void setAnnotatedRegionsDirty(bool f) { m_annotatedRegionsDirty = f; }
    bool annotatedRegionsDirty() const { return m_annotatedRegionsDirty; }
    bool hasAnnotatedRegions () const { return m_hasAnnotatedRegions; }
    void setHasAnnotatedRegions(bool f) { m_hasAnnotatedRegions = f; }
    const Vector<AnnotatedRegionValue>& annotatedRegions() const;
    void setAnnotatedRegions(const Vector<AnnotatedRegionValue>&);

    virtual void removeAllEventListeners();

    const SVGDocumentExtensions* svgExtensions();
    SVGDocumentExtensions* accessSVGExtensions();

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

    virtual bool isContextThread() const;
    virtual bool isJSExecutionForbidden() const { return false; }

    bool containsValidityStyleRules() const { return m_containsValidityStyleRules; }
    void setContainsValidityStyleRules() { m_containsValidityStyleRules = true; }

    void enqueueWindowEvent(PassRefPtr<Event>);
    void enqueueDocumentEvent(PassRefPtr<Event>);
    void enqueuePageshowEvent(PageshowEventPersistence);
    void enqueueHashchangeEvent(const String& oldURL, const String& newURL);
    void enqueuePopstateEvent(PassRefPtr<SerializedScriptValue> stateObject);
    void enqueueScrollEventForNode(Node*);

    const QualifiedName& idAttributeName() const { return m_idAttributeName; }

    bool hasFullscreenElementStack() const { return m_hasFullscreenElementStack; }
    void setHasFullscreenElementStack() { m_hasFullscreenElementStack = true; }

    void webkitExitPointerLock();
    Element* webkitPointerLockElement() const;

    // Used to allow element that loads data without going through a FrameLoader to delay the 'load' event.
    void incrementLoadEventDelayCount() { ++m_loadEventDelayCount; }
    void decrementLoadEventDelayCount();
    bool isDelayingLoadEvent() const { return m_loadEventDelayCount; }

    PassRefPtr<Touch> createTouch(DOMWindow*, EventTarget*, int identifier, int pageX, int pageY, int screenX, int screenY, int radiusX, int radiusY, float rotationAngle, float force) const;
    PassRefPtr<TouchList> createTouchList(Vector<RefPtr<Touch> >&) const;

    const DocumentTiming* timing() const { return &m_documentTiming; }

    int requestAnimationFrame(PassRefPtr<RequestAnimationFrameCallback>);
    void cancelAnimationFrame(int id);
    void serviceScriptedAnimations(double monotonicAnimationStartTime);

    virtual EventTarget* errorEventTarget();
    virtual void logExceptionToConsole(const String& errorMessage, const String& sourceURL, int lineNumber, int columnNumber, PassRefPtr<ScriptCallStack>);

    void initDNSPrefetch();

    double lastHandledUserGestureTimestamp() const { return m_lastHandledUserGestureTimestamp; }
    void resetLastHandledUserGestureTimestamp();

    bool hasTouchEventHandlers() const { return (m_touchEventTargets.get()) ? m_touchEventTargets->size() : false; }

    void didAddTouchEventHandler(Node*);
    void didRemoveTouchEventHandler(Node*);

    void didRemoveEventTargetNode(Node*);

    const TouchEventTargetSet* touchEventTargets() const { return m_touchEventTargets.get(); }

    bool isInDocumentWrite() { return m_writeRecursionDepth > 0; }

    void suspendScheduledTasks(ActiveDOMObject::ReasonForSuspension);
    void resumeScheduledTasks();

    IntSize initialViewportSize() const;

    TextAutosizer* textAutosizer() { return m_textAutosizer.get(); }

    PassRefPtr<Element> createElement(const AtomicString& localName, const AtomicString& typeExtension, ExceptionState&);
    PassRefPtr<Element> createElementNS(const AtomicString& namespaceURI, const String& qualifiedName, const AtomicString& typeExtension, ExceptionState&);
    ScriptValue registerElement(WebCore::ScriptState*, const AtomicString& name, ExceptionState&);
    ScriptValue registerElement(WebCore::ScriptState*, const AtomicString& name, const Dictionary& options, ExceptionState&, CustomElement::NameSet validNames = CustomElement::StandardNames);
    CustomElementRegistrationContext* registrationContext() { return m_registrationContext.get(); }

    void setImport(HTMLImport*);
    HTMLImport* import() const { return m_import; }
    bool haveImportsLoaded() const;
    void didLoadAllImports();

    void adjustFloatQuadsForScrollAndAbsoluteZoom(Vector<FloatQuad>&, RenderObject*);
    void adjustFloatRectForScrollAndAbsoluteZoom(FloatRect&, RenderObject*);

    bool hasActiveParser();
    unsigned activeParserCount() { return m_activeParserCount; }
    void incrementActiveParserCount() { ++m_activeParserCount; }
    void decrementActiveParserCount();

    void setContextFeatures(PassRefPtr<ContextFeatures>);
    ContextFeatures* contextFeatures() { return m_contextFeatures.get(); }

    DocumentSharedObjectPool* sharedObjectPool() { return m_sharedObjectPool.get(); }

    void didRemoveAllPendingStylesheet();
    void setNeedsNotifyRemoveAllPendingStylesheet() { m_needsNotifyRemoveAllPendingStylesheet = true; }
    void clearStyleResolver();
    void notifySeamlessChildDocumentsOfStylesheetUpdate() const;

    bool inStyleRecalc() { return m_inStyleRecalc; }

    // Return a Locale for the default locale if the argument is null or empty.
    Locale& getCachedLocale(const AtomicString& locale = nullAtom);

    DocumentTimeline* timeline() const { return m_timeline.get(); }

    void addToTopLayer(Element*, const Element* before = 0);
    void removeFromTopLayer(Element*);
    const Vector<RefPtr<Element> >& topLayerElements() const { return m_topLayerElements; }
    HTMLDialogElement* activeModalDialog() const;

    const Document* templateDocument() const;
    Document& ensureTemplateDocument();
    void setTemplateDocumentHost(Document* templateDocumentHost) { m_templateDocumentHost = templateDocumentHost; }
    Document* templateDocumentHost() { return m_templateDocumentHost; }

    void didAssociateFormControl(Element*);

    void addConsoleMessageWithRequestIdentifier(MessageSource, MessageLevel, const String& message, unsigned long requestIdentifier);

    virtual DOMWindow* executingWindow() OVERRIDE { return domWindow(); }
    virtual void userEventWasHandled() OVERRIDE { resetLastHandledUserGestureTimestamp(); }

    PassRefPtr<FontFaceSet> fonts();
    DocumentLifecycleNotifier* lifecycleNotifier();

    enum HttpRefreshType {
        HttpRefreshFromHeader,
        HttpRefreshFromMetaTag
    };
    void maybeHandleHttpRefresh(const String&, HttpRefreshType);

protected:
    Document(const DocumentInit&, DocumentClassFlags = DefaultDocumentClass);

    virtual void didUpdateSecurityOrigin() OVERRIDE;

    void clearXMLVersion() { m_xmlVersion = String(); }

    virtual void dispose() OVERRIDE;

private:
    friend class Node;
    friend class IgnoreDestructiveWriteCountIncrementer;

    virtual EventQueue* eventQueue() const FINAL;

    void updateDistributionIfNeeded();

    void detachParser();

    typedef void (*ArgumentsCallback)(const String& keyString, const String& valueString, Document*, void* data);
    void processArguments(const String& features, void* data, ArgumentsCallback);

    virtual bool isDocument() const OVERRIDE { return true; }

    virtual void childrenChanged(bool changedByParser = false, Node* beforeChange = 0, Node* afterChange = 0, int childCountDelta = 0);

    virtual String nodeName() const;
    virtual NodeType nodeType() const;
    virtual bool childTypeAllowed(NodeType) const;
    virtual PassRefPtr<Node> cloneNode(bool deep = true);

    virtual void refScriptExecutionContext() { ref(); }
    virtual void derefScriptExecutionContext() { deref(); }
    virtual PassOwnPtr<LifecycleNotifier> createLifecycleNotifier() OVERRIDE;

    virtual const KURL& virtualURL() const; // Same as url(), but needed for ScriptExecutionContext to implement it without a performance loss for direct calls.
    virtual KURL virtualCompleteURL(const String&) const; // Same as completeURL() for the same reason as above.

    virtual void addMessage(MessageSource, MessageLevel, const String& message, const String& sourceURL, unsigned lineNumber, ScriptState*);
    void internalAddMessage(MessageSource, MessageLevel, const String& message, const String& sourceURL, unsigned lineNumber, PassRefPtr<ScriptCallStack>, ScriptState*);

    virtual double timerAlignmentInterval() const;

    void updateTitle(const String&);
    void updateFocusAppearanceTimerFired(Timer<Document>*);
    void updateBaseURL();

    void createStyleResolver();

    void executeScriptsWaitingForResourcesIfNeeded();

    void seamlessParentUpdatedStylesheets();

    void recalcStyleForLayoutIgnoringPendingStylesheets();

    PassRefPtr<NodeList> handleZeroPadding(const HitTestRequest&, HitTestResult&) const;

    void loadEventDelayTimerFired(Timer<Document>*);

    void pendingTasksTimerFired(Timer<Document>*);

    static void didReceiveTask(void*);

    template <typename CharacterType>
    void displayBufferModifiedByEncodingInternal(CharacterType*, unsigned) const;

    PageVisibilityState visibilityState() const;

    PassRefPtr<HTMLCollection> ensureCachedCollection(CollectionType);

    // Note that dispatching a window load event may cause the DOMWindow to be detached from
    // the Frame, so callers should take a reference to the DOMWindow (which owns us) to
    // prevent the Document from getting blown away from underneath them.
    void dispatchWindowLoadEvent();

    void addListenerType(ListenerType listenerType) { m_listenerTypes |= listenerType; }
    void addMutationEventListenerTypeIfEnabled(ListenerType);

    void didAssociateFormControlsTimerFired(Timer<Document>*);
    void styleResolverThrowawayTimerFired(Timer<Document>*);

    void processHttpEquivDefaultStyle(const String& content);
    void processHttpEquivRefresh(const String& content);
    void processHttpEquivSetCookie(const String& content);
    void processHttpEquivXFrameOptions(const String& content);
    void processHttpEquivContentSecurityPolicy(const String& equiv, const String& content);

    // FIXME: This should probably be handled inside the style resolver itself.
    Timer<Document> m_styleResolverThrowawayTimer;
    unsigned m_styleResolverAccessCount;
    unsigned m_lastStyleResolverAccessCount;

    OwnPtr<StyleResolver> m_styleResolver;
    bool m_didCalculateStyleResolver;
    bool m_hasNodesWithPlaceholderStyle;
    bool m_needsNotifyRemoveAllPendingStylesheet;
    // But sometimes you need to ignore pending stylesheet count to
    // force an immediate layout when requested by JS.
    bool m_ignorePendingStylesheets;
    bool m_evaluateMediaQueriesOnStyleRecalc;

    // If we do ignore the pending stylesheet count, then we need to add a boolean
    // to track that this happened so that we can do a full repaint when the stylesheets
    // do eventually load.
    PendingSheetLayout m_pendingSheetLayout;

    Frame* m_frame;
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

    // Document.documentURI:
    // Although URL-like, Document.documentURI can actually be set to any
    // string by content.  Document.documentURI affects m_baseURL unless the
    // document contains a <base> element, in which case the <base> element
    // takes precedence.
    String m_documentURI;

    String m_baseTarget;

    RefPtr<DocumentType> m_docType;
    OwnPtr<DOMImplementation> m_implementation;

    RefPtr<CSSStyleSheet> m_elemSheet;

    bool m_printing;
    bool m_paginatedForScreen;

    bool m_ignoreAutofocus;

    CompatibilityMode m_compatibilityMode;
    bool m_compatibilityModeLocked; // This is cheaper than making setCompatibilityMode virtual.

    bool m_didPostCheckFocusedElementTask;
    RefPtr<Element> m_focusedElement;
    RefPtr<Node> m_hoverNode;
    RefPtr<Element> m_activeElement;
    RefPtr<Element> m_documentElement;
    UserActionElementSet m_userActionElements;

    uint64_t m_domTreeVersion;
    static uint64_t s_globalTreeVersion;

    HashSet<NodeIterator*> m_nodeIterators;
    HashSet<Range*> m_ranges;

    unsigned short m_listenerTypes;

    MutationObserverOptions m_mutationObserverTypes;

    OwnPtr<StyleEngine> m_styleEngine;
    RefPtr<StyleSheetList> m_styleSheetList;

    OwnPtr<FormController> m_formController;

    TextLinkColors m_textLinkColors;
    OwnPtr<VisitedLinkState> m_visitedLinkState;

    bool m_loadingSheet;
    bool m_visuallyOrdered;
    ReadyState m_readyState;
    bool m_bParsing;

    Timer<Document> m_styleRecalcTimer;
    bool m_inStyleRecalc;

    bool m_gotoAnchorNeededAfterStylesheetsLoad;
    bool m_isDNSPrefetchEnabled;
    bool m_haveExplicitlyDisabledDNSPrefetch;
    bool m_containsValidityStyleRules;
    bool m_updateFocusAppearanceRestoresSelection;

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

    RefPtr<SerializedScriptValue> m_pendingStateObject;
    double m_startTime;
    bool m_overMinimumLayoutThreshold;

    OwnPtr<ScriptRunner> m_scriptRunner;

    Vector<RefPtr<HTMLScriptElement> > m_currentScriptStack;

    OwnPtr<TransformSource> m_transformSource;
    RefPtr<Document> m_transformSourceDocument;

    String m_xmlEncoding;
    String m_xmlVersion;
    unsigned m_xmlStandalone : 2;
    unsigned m_hasXMLDeclaration : 1;

    String m_contentLanguage;

    RefPtr<TextResourceDecoder> m_decoder;
    WTF::TextEncoding m_encoding;

    InheritedBool m_designMode;

    HashSet<LiveNodeListBase*> m_listsInvalidatedAtDocument;
    unsigned m_nodeListCounts[numNodeListInvalidationTypes];

    OwnPtr<SVGDocumentExtensions> m_svgExtensions;

    Vector<AnnotatedRegionValue> m_annotatedRegions;
    bool m_hasAnnotatedRegions;
    bool m_annotatedRegionsDirty;

    HashMap<String, RefPtr<HTMLCanvasElement> > m_cssCanvasElements;

    Vector<IconURL> m_iconURLs;

    OwnPtr<SelectorQueryCache> m_selectorQueryCache;

    bool m_useSecureKeyboardEntryWhenActive;

    DocumentClassFlags m_documentClasses;

    bool m_isViewSource;
    bool m_sawElementsInKnownNamespaces;
    bool m_isSrcdocDocument;
    bool m_isMobileDocument;

    RenderObject* m_renderer;
    RefPtr<DocumentEventQueue> m_eventQueue;

    WeakPtrFactory<Document> m_weakFactory;
    WeakPtr<Document> m_contextDocument;

    QualifiedName m_idAttributeName;

    bool m_hasFullscreenElementStack; // For early return in FullscreenElementStack::fromIfExists()

    Vector<RefPtr<Element> > m_topLayerElements;

    int m_loadEventDelayCount;
    Timer<Document> m_loadEventDelayTimer;

    ViewportArguments m_viewportArguments;
    ViewportArguments m_legacyViewportArguments;

    ReferrerPolicy m_referrerPolicy;

    bool m_directionSetOnDocumentElement;
    bool m_writingModeSetOnDocumentElement;

    bool m_didAllowNavigationViaBeforeUnloadConfirmationPanel;

    DocumentTiming m_documentTiming;
    RefPtr<MediaQueryMatcher> m_mediaQueryMatcher;
    bool m_writeRecursionIsTooDeep;
    unsigned m_writeRecursionDepth;

    OwnPtr<TouchEventTargetSet> m_touchEventTargets;

    double m_lastHandledUserGestureTimestamp;

    RefPtr<ScriptedAnimationController> m_scriptedAnimationController;

    Timer<Document> m_pendingTasksTimer;
    Vector<OwnPtr<Task> > m_pendingTasks;

    OwnPtr<TextAutosizer> m_textAutosizer;

    RefPtr<CustomElementRegistrationContext> m_registrationContext;

    bool m_scheduledTasksAreSuspended;

    RefPtr<NamedFlowCollection> m_namedFlows;

    RefPtr<DOMSecurityPolicy> m_domSecurityPolicy;

    void sharedObjectPoolClearTimerFired(Timer<Document>*);
    Timer<Document> m_sharedObjectPoolClearTimer;

    OwnPtr<DocumentSharedObjectPool> m_sharedObjectPool;

#ifndef NDEBUG
    bool m_didDispatchViewportPropertiesChanged;
#endif

    typedef HashMap<AtomicString, OwnPtr<Locale> > LocaleIdentifierToLocaleMap;
    LocaleIdentifierToLocaleMap m_localeCache;

    RefPtr<DocumentTimeline> m_timeline;

    RefPtr<Document> m_templateDocument;
    Document* m_templateDocumentHost; // Manually managed weakref (backpointer from m_templateDocument).

    RefPtr<FontFaceSet> m_fonts;

    Timer<Document> m_didAssociateFormControlsTimer;
    HashSet<RefPtr<Element> > m_associatedFormControls;
};

inline void Document::notifyRemovePendingSheetIfNeeded()
{
    if (m_needsNotifyRemoveAllPendingStylesheet)
        didRemoveAllPendingStylesheet();
}

inline const Document* Document::templateDocument() const
{
    // If DOCUMENT does not have a browsing context, Let TEMPLATE CONTENTS OWNER be DOCUMENT and abort these steps.
    if (!m_frame)
        return this;

    return m_templateDocument.get();
}

inline bool Document::shouldOverrideLegacyViewport(ViewportArguments::Type origin)
{
    // The different (legacy) meta tags have different priorities based on the type
    // regardless of which order they appear in the DOM. The priority is given by the
    // ViewportArguments::Type enum.
    return origin >= m_legacyViewportArguments.type;
}

inline Document* toDocument(ScriptExecutionContext* scriptExecutionContext)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!scriptExecutionContext || scriptExecutionContext->isDocument());
    return static_cast<Document*>(scriptExecutionContext);
}

inline const Document* toDocument(const ScriptExecutionContext* scriptExecutionContext)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!scriptExecutionContext || scriptExecutionContext->isDocument());
    return static_cast<const Document*>(scriptExecutionContext);
}

inline Document* toDocument(Node* node)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!node || node->isDocumentNode());
    return static_cast<Document*>(node);
}

inline const Document* toDocument(const Node* node)
{
    ASSERT_WITH_SECURITY_IMPLICATION(!node || node->isDocumentNode());
    return static_cast<const Document*>(node);
}

// This will catch anyone doing an unnecessary cast.
void toDocument(const Document*);

// Put these methods here, because they require the Document definition, but we really want to inline them.

inline bool Node::isDocumentNode() const
{
    return this == documentInternal();
}

Node* eventTargetNodeForDocument(Document*);

} // namespace WebCore

#endif // Document_h
