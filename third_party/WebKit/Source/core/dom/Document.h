/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2012 Apple Inc. All
 * rights reserved.
 * Copyright (C) 2008, 2009 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
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

#include <memory>
#include <string>
#include <utility>

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptValue.h"
#include "core/CoreExport.h"
#include "core/dom/ContainerNode.h"
#include "core/dom/DocumentEncodingData.h"
#include "core/dom/DocumentInit.h"
#include "core/dom/DocumentLifecycle.h"
#include "core/dom/DocumentShutdownNotifier.h"
#include "core/dom/DocumentShutdownObserver.h"
#include "core/dom/DocumentTiming.h"
#include "core/dom/ExecutionContext.h"
#include "core/dom/MutationObserver.h"
#include "core/dom/SynchronousMutationNotifier.h"
#include "core/dom/SynchronousMutationObserver.h"
#include "core/dom/Text.h"
#include "core/dom/TextLinkColors.h"
#include "core/dom/TreeScope.h"
#include "core/dom/UserActionElementSet.h"
#include "core/dom/ViewportDescription.h"
#include "core/frame/DOMTimerCoordinator.h"
#include "core/frame/HostsUsingFeatures.h"
#include "core/html/custom/V0CustomElement.h"
#include "core/html/parser/ParserSynchronizationPolicy.h"
#include "core/page/PageVisibilityState.h"
#include "platform/Length.h"
#include "platform/Timer.h"
#include "platform/WebTaskRunner.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/loader/fetch/ClientHintsPreferences.h"
#include "platform/scroll/ScrollTypes.h"
#include "platform/weborigin/KURL.h"
#include "platform/weborigin/ReferrerPolicy.h"
#include "platform/wtf/HashSet.h"
#include "platform/wtf/RefPtr.h"
#include "public/platform/WebFocusType.h"
#include "public/platform/WebInsecureRequestPolicy.h"

namespace blink {

namespace mojom {
enum class EngagementLevel : int32_t;
}

class AnimationClock;
class DocumentTimeline;
class AXObjectCache;
class Attr;
class CDATASection;
class CSSStyleSheet;
class CanvasFontCache;
class ChromeClient;
class CompositorPendingAnimations;
class Comment;
class ComputedStyle;
class ConsoleMessage;
class ContextFeatures;
class V0CustomElementMicrotaskRunQueue;
class V0CustomElementRegistrationContext;
class DOMImplementation;
class DOMWindow;
class DocumentFragment;
class DocumentLoader;
class DocumentMarkerController;
class DocumentNameCollection;
class DocumentParser;
class DocumentState;
class DocumentType;
class Element;
class ElementDataCache;
class ElementRegistrationOptions;
class Event;
class EventFactoryBase;
class EventListener;
template <typename EventType>
class EventWithHitTestResults;
class ExceptionState;
class FloatQuad;
class FloatRect;
class FormController;
class FrameRequestCallback;
class HTMLAllCollection;
class HTMLBodyElement;
class HTMLCollection;
class HTMLDialogElement;
class HTMLElement;
class HTMLFrameOwnerElement;
class HTMLHeadElement;
class HTMLImportLoader;
class HTMLImportsController;
class HTMLLinkElement;
class HTMLScriptElementOrSVGScriptElement;
class HitTestRequest;
class IdleRequestCallback;
class IdleRequestOptions;
class IntersectionObserverController;
class LayoutPoint;
class LayoutView;
class LayoutViewItem;
class LiveNodeListBase;
class LocalDOMWindow;
class Locale;
class LocalFrame;
class LocalFrameView;
class Location;
class MediaQueryListListener;
class MediaQueryMatcher;
class NodeIterator;
class NthIndexCache;
class OriginAccessEntry;
class Page;
class ProcessingInstruction;
class PropertyRegistry;
class QualifiedName;
class Range;
class ResizeObserverController;
class ResourceFetcher;
class RootScrollerController;
class SVGDocumentExtensions;
class SVGUseElement;
class ScriptElementBase;
class ScriptRunner;
class ScriptableDocumentParser;
class ScriptedAnimationController;
class ScriptedIdleTaskController;
class SecurityOrigin;
class SegmentedString;
class SelectorQueryCache;
class SerializedScriptValue;
class Settings;
class SnapCoordinator;
class StringOrDictionary;
class StyleEngine;
class StyleResolver;
class StyleSheetList;
class TextAutosizer;
class Touch;
class TouchList;
class TransformSource;
class TreeWalker;
class V8NodeFilterCondition;
class VisitedLinkState;
class WebMouseEvent;
struct AnnotatedRegionValue;
struct FocusParams;
struct IconURL;

using MouseEventWithHitTestResults = EventWithHitTestResults<WebMouseEvent>;
using ExceptionCode = int;

enum NodeListInvalidationType {
  kDoNotInvalidateOnAttributeChanges = 0,
  kInvalidateOnClassAttrChange,
  kInvalidateOnIdNameAttrChange,
  kInvalidateOnNameAttrChange,
  kInvalidateOnForAttrChange,
  kInvalidateForFormControls,
  kInvalidateOnHRefAttrChange,
  kInvalidateOnAnyAttrChange,
};
const int kNumNodeListInvalidationTypes = kInvalidateOnAnyAttrChange + 1;

enum DocumentClass {
  kDefaultDocumentClass = 0,
  kHTMLDocumentClass = 1,
  kXHTMLDocumentClass = 1 << 1,
  kImageDocumentClass = 1 << 2,
  kPluginDocumentClass = 1 << 3,
  kMediaDocumentClass = 1 << 4,
  kSVGDocumentClass = 1 << 5,
  kXMLDocumentClass = 1 << 6,
};

enum ShadowCascadeOrder {
  kShadowCascadeNone,
  kShadowCascadeV0,
  kShadowCascadeV1
};

enum CreateElementFlags {
  kCreatedByParser = 1 << 0,
  // Synchronous custom elements flag:
  // https://dom.spec.whatwg.org/#concept-create-element
  // TODO(kojii): Remove these flags, add an option not to queue upgrade, and
  // let parser/DOM methods to upgrade synchronously when necessary.
  kSynchronousCustomElements = 0 << 1,
  kAsynchronousCustomElements = 1 << 1,

  // Aliases by callers.
  // Clone a node: https://dom.spec.whatwg.org/#concept-node-clone
  kCreatedByCloneNode = kAsynchronousCustomElements,
  kCreatedByImportNode = kCreatedByCloneNode,
  // https://dom.spec.whatwg.org/#dom-document-createelement
  kCreatedByCreateElement = kSynchronousCustomElements,
  // https://html.spec.whatwg.org/#create-an-element-for-the-token
  kCreatedByFragmentParser = kCreatedByParser | kAsynchronousCustomElements,
};

// Collect data about deferred loading of offscreen cross-origin documents. All
// cross-origin documents log Created. Only those that would load log a reason.
// We can then see the % of cross-origin documents that never have to load.
// See https://crbug.com/635105.
// Logged to UMA, don't re-arrange entries without creating a new histogram.
enum class WouldLoadReason {
  kInvalid,
  kCreated,
  k3ScreensAway,
  k2ScreensAway,
  k1ScreenAway,
  kVisible,
  // If outer and inner frames aren't in the same process we can't determine
  // if the inner frame is visible, so just load it.
  // TODO(dgrogan): Revisit after https://crbug.com/650433 is fixed.
  kNoParent,

  kCount,
};

using DocumentClassFlags = unsigned char;

class CORE_EXPORT Document : public ContainerNode,
                             public TreeScope,
                             public SecurityContext,
                             public ExecutionContext,
                             public DocumentShutdownNotifier,
                             public SynchronousMutationNotifier,
                             public Supplementable<Document> {
  DEFINE_WRAPPERTYPEINFO();
  USING_GARBAGE_COLLECTED_MIXIN(Document);

 public:
  static Document* Create(const DocumentInit& init) {
    return new Document(init);
  }
  static Document* CreateForTest() {
    return new Document(DocumentInit::Create());
  }
  // Factory for web-exposed Document constructor. The argument document must be
  // a document instance representing window.document, and it works as the
  // source of ExecutionContext and security origin of the new document.
  // https://dom.spec.whatwg.org/#dom-document-document
  static Document* Create(const Document&);
  ~Document() override;

  MediaQueryMatcher& GetMediaQueryMatcher();

  void MediaQueryAffectingValueChanged();

  using SecurityContext::GetSecurityOrigin;
  using SecurityContext::GetContentSecurityPolicy;
  using TreeScope::getElementById;

  bool CanContainRangeEndPoint() const override { return true; }

  SelectorQueryCache& GetSelectorQueryCache();

  // Focus Management.
  Element* ActiveElement() const;
  bool hasFocus() const;

  // DOM methods & attributes for Document

  DEFINE_ATTRIBUTE_EVENT_LISTENER(beforecopy);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(beforecut);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(beforepaste);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(copy);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(cut);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(paste);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(pointerlockchange);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(pointerlockerror);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(readystatechange);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(search);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(securitypolicyviolation);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(selectionchange);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(selectstart);

  bool ShouldMergeWithLegacyDescription(ViewportDescription::Type) const;
  bool ShouldOverrideLegacyDescription(ViewportDescription::Type) const;
  void SetViewportDescription(const ViewportDescription&);
  ViewportDescription GetViewportDescription() const;
  Length ViewportDefaultMinWidth() const { return viewport_default_min_width_; }

  String OutgoingReferrer() const override;
  ReferrerPolicy GetReferrerPolicy() const override;

  void SetDoctype(DocumentType*);
  DocumentType* doctype() const { return doc_type_.Get(); }

  DOMImplementation& implementation();

  Element* documentElement() const { return document_element_.Get(); }

  // Returns whether the Document has an AppCache manifest.
  bool HasAppCacheManifest() const;

  Location* location() const;

  DocumentFragment* createDocumentFragment();
  Text* createTextNode(const String& data);
  Comment* createComment(const String& data);
  CDATASection* createCDATASection(const String& data, ExceptionState&);
  ProcessingInstruction* createProcessingInstruction(const String& target,
                                                     const String& data,
                                                     ExceptionState&);
  Attr* createAttribute(const AtomicString& name, ExceptionState&);
  Attr* createAttributeNS(const AtomicString& namespace_uri,
                          const AtomicString& qualified_name,
                          ExceptionState&,
                          bool should_ignore_namespace_checks = false);
  Node* importNode(Node* imported_node, bool deep, ExceptionState&);
  Element* createElementNS(const AtomicString& namespace_uri,
                           const AtomicString& qualified_name,
                           ExceptionState&);
  Element* createElement(const QualifiedName&, CreateElementFlags);

  Element* ElementFromPoint(int x, int y) const;
  HeapVector<Member<Element>> ElementsFromPoint(int x, int y) const;
  Range* caretRangeFromPoint(int x, int y);
  Element* scrollingElement();
  // When calling from C++ code, use this method. scrollingElement() is
  // just for the web IDL implementation.
  Element* ScrollingElementNoLayout();

  String readyState() const;

  AtomicString characterSet() const { return Document::EncodingName(); }

  AtomicString EncodingName() const;

  void SetContent(const String&);

  String SuggestedMIMEType() const;
  void SetMimeType(const AtomicString&);
  AtomicString contentType() const;  // DOM 4 document.contentType

  const AtomicString& ContentLanguage() const { return content_language_; }
  void SetContentLanguage(const AtomicString&);

  String xmlEncoding() const { return xml_encoding_; }
  String xmlVersion() const { return xml_version_; }
  enum StandaloneStatus { kStandaloneUnspecified, kStandalone, kNotStandalone };
  bool xmlStandalone() const { return xml_standalone_ == kStandalone; }
  StandaloneStatus XmlStandaloneStatus() const {
    return static_cast<StandaloneStatus>(xml_standalone_);
  }
  bool HasXMLDeclaration() const { return has_xml_declaration_; }

  void SetXMLEncoding(const String& encoding) {
    xml_encoding_ = encoding;
  }  // read-only property, only to be set from XMLDocumentParser
  void setXMLVersion(const String&, ExceptionState&);
  void setXMLStandalone(bool, ExceptionState&);
  void SetHasXMLDeclaration(bool has_xml_declaration) {
    has_xml_declaration_ = has_xml_declaration ? 1 : 0;
  }

  String origin() const;
  String suborigin() const;

  String visibilityState() const;
  PageVisibilityState GetPageVisibilityState() const;
  bool hidden() const;
  void DidChangeVisibilityState();

  // If the document is "prefetch only", it will not be fully contstructed,
  // and should never be displayed. Only a few resources will be loaded and
  // scanned, in order to warm up caches.
  bool IsPrefetchOnly() const;

  Node* adoptNode(Node* source, ExceptionState&);

  HTMLCollection* images();
  HTMLCollection* embeds();
  HTMLCollection* applets();
  HTMLCollection* links();
  HTMLCollection* forms();
  HTMLCollection* anchors();
  HTMLCollection* scripts();
  HTMLAllCollection* all();

  HTMLCollection* WindowNamedItems(const AtomicString& name);
  DocumentNameCollection* DocumentNamedItems(const AtomicString& name);

  // "defaultView" attribute defined in HTML spec.
  LocalDOMWindow* defaultView() const;

  bool IsHTMLDocument() const { return document_classes_ & kHTMLDocumentClass; }
  bool IsXHTMLDocument() const {
    return document_classes_ & kXHTMLDocumentClass;
  }
  bool IsXMLDocument() const { return document_classes_ & kXMLDocumentClass; }
  bool IsImageDocument() const {
    return document_classes_ & kImageDocumentClass;
  }
  bool IsSVGDocument() const { return document_classes_ & kSVGDocumentClass; }
  bool IsPluginDocument() const {
    return document_classes_ & kPluginDocumentClass;
  }
  bool IsMediaDocument() const {
    return document_classes_ & kMediaDocumentClass;
  }

  bool HasSVGRootNode() const;

  bool IsFrameSet() const;

  bool IsSrcdocDocument() const { return is_srcdoc_document_; }
  bool IsMobileDocument() const { return is_mobile_document_; }

  StyleResolver* GetStyleResolver() const;
  StyleResolver& EnsureStyleResolver() const;

  bool IsViewSource() const { return is_view_source_; }
  void SetIsViewSource(bool);

  bool SawElementsInKnownNamespaces() const {
    return saw_elements_in_known_namespaces_;
  }

  bool CanExecuteScripts(ReasonForCallingCanExecuteScripts) override;
  bool IsRenderingReady() const;
  bool IsScriptExecutionReady() const {
    return HaveImportsLoaded() && HaveScriptBlockingStylesheetsLoaded();
  }

  // This is a DOM function.
  StyleSheetList& StyleSheets();

  StyleEngine& GetStyleEngine() {
    DCHECK(style_engine_.Get());
    return *style_engine_.Get();
  }

  bool GotoAnchorNeededAfterStylesheetsLoad() {
    return goto_anchor_needed_after_stylesheets_load_;
  }
  void SetGotoAnchorNeededAfterStylesheetsLoad(bool b) {
    goto_anchor_needed_after_stylesheets_load_ = b;
  }

  void ScheduleUseShadowTreeUpdate(SVGUseElement&);
  void UnscheduleUseShadowTreeUpdate(SVGUseElement&);

  void EvaluateMediaQueryList();

  FormController& GetFormController();
  DocumentState* FormElementsState() const;
  void SetStateForNewFormElements(const Vector<String>&);

  LocalFrameView* View() const;                    // can be null
  LocalFrame* GetFrame() const { return frame_; }  // can be null
  Page* GetPage() const;                           // can be null
  Settings* GetSettings() const;                   // can be null

  float DevicePixelRatio() const;

  Range* createRange();

  NodeIterator* createNodeIterator(Node* root,
                                   unsigned what_to_show,
                                   V8NodeFilterCondition*);
  TreeWalker* createTreeWalker(Node* root,
                               unsigned what_to_show,
                               V8NodeFilterCondition*);

  // Special support for editing
  Text* CreateEditingTextNode(const String&);

  void SetupFontBuilder(ComputedStyle& document_style);

  bool NeedsLayoutTreeUpdate() const;
  bool NeedsLayoutTreeUpdateForNode(const Node&) const;
  // Update ComputedStyles and attach LayoutObjects if necessary, but don't
  // lay out.
  void UpdateStyleAndLayoutTree();
  // Same as updateStyleAndLayoutTree() except ignoring pending stylesheets.
  void UpdateStyleAndLayoutTreeIgnorePendingStylesheets();
  void UpdateStyleAndLayoutTreeForNode(const Node*);
  void UpdateStyleAndLayout();
  void LayoutUpdated();
  enum RunPostLayoutTasks {
    kRunPostLayoutTasksAsyhnchronously,
    kRunPostLayoutTasksSynchronously,
  };
  void UpdateStyleAndLayoutIgnorePendingStylesheets(
      RunPostLayoutTasks = kRunPostLayoutTasksAsyhnchronously);
  void UpdateStyleAndLayoutIgnorePendingStylesheetsForNode(Node*);
  RefPtr<ComputedStyle> StyleForElementIgnoringPendingStylesheets(Element*);
  RefPtr<ComputedStyle> StyleForPage(int page_index);

  // Ensures that location-based data will be valid for a given node.
  //
  // This will run style and layout if they are currently dirty, and it may also
  // run compositing inputs if the node is in a sticky subtree (as the sticky
  // offset may change the node's position).
  //
  // Due to this you should only call this if you definitely need valid location
  // data, otherwise use one of the |UpdateStyleAndLayout...| methods above.
  void EnsurePaintLocationDataValidForNode(const Node*);

  // Returns true if page box (margin boxes and page borders) is visible.
  bool IsPageBoxVisible(int page_index);

  // Returns the preferred page size and margins in pixels, assuming 96
  // pixels per inch. pageSize, marginTop, marginRight, marginBottom,
  // marginLeft must be initialized to the default values that are used if
  // auto is specified.
  void PageSizeAndMarginsInPixels(int page_index,
                                  DoubleSize& page_size,
                                  int& margin_top,
                                  int& margin_right,
                                  int& margin_bottom,
                                  int& margin_left);

  ResourceFetcher* Fetcher() const { return fetcher_.Get(); }

  void Initialize();
  virtual void Shutdown();

  void AttachLayoutTree(AttachContext&) override { NOTREACHED(); }
  void DetachLayoutTree(const AttachContext& = AttachContext()) override {
    NOTREACHED();
  }

  // If you have a Document, use layoutView() instead which is faster.
  void GetLayoutObject() const = delete;

  LayoutView* GetLayoutView() const { return layout_view_; }
  LayoutViewItem GetLayoutViewItem() const;

  Document& AxObjectCacheOwner() const;
  AXObjectCache* ExistingAXObjectCache() const;
  AXObjectCache* AxObjectCache() const;
  void ClearAXObjectCache();

  // to get visually ordered hebrew and arabic pages right
  bool VisuallyOrdered() const { return visually_ordered_; }

  DocumentLoader* Loader() const;

  // This is the DOM API document.open(). enteredDocument is the responsible
  // document of the entry settings object.
  void open(Document* entered_document, ExceptionState&);
  // This is used internally and does not handle exceptions.
  void open();
  DocumentParser* ImplicitOpen(ParserSynchronizationPolicy);

  // This is the DOM API document.close()
  void close(ExceptionState&);
  // This is used internally and does not handle exceptions.
  void close();

  void CheckCompleted();

  bool DispatchBeforeUnloadEvent(ChromeClient&,
                                 bool is_reload,
                                 bool& did_allow_navigation);
  void DispatchUnloadEvents();

  enum PageDismissalType {
    kNoDismissal,
    kBeforeUnloadDismissal,
    kPageHideDismissal,
    kUnloadVisibilityChangeDismissal,
    kUnloadDismissal
  };
  PageDismissalType PageDismissalEventBeingDispatched() const;

  void CancelParsing();

  void write(const SegmentedString& text,
             Document* entered_document = nullptr,
             ExceptionState& = ASSERT_NO_EXCEPTION);
  void write(const String& text,
             Document* entered_document = nullptr,
             ExceptionState& = ASSERT_NO_EXCEPTION);
  void writeln(const String& text,
               Document* entered_document = nullptr,
               ExceptionState& = ASSERT_NO_EXCEPTION);
  void write(LocalDOMWindow*, const Vector<String>& text, ExceptionState&);
  void writeln(LocalDOMWindow*, const Vector<String>& text, ExceptionState&);

  bool WellFormed() const { return well_formed_; }

  // Return the document URL, or an empty URL if it's unavailable.
  // This is not an implementation of web-exposed Document.prototype.URL.
  const KURL& Url() const { return url_; }
  void SetURL(const KURL&);

  // Bind the url to document.url, if unavailable bind to about:blank.
  KURL urlForBinding() const;

  // To understand how these concepts relate to one another, please see the
  // comments surrounding their declaration.

  // Document base URL.
  // https://html.spec.whatwg.org/multipage/urls-and-fetching.html#document-base-url
  const KURL& BaseURL() const;
  void SetBaseURLOverride(const KURL&);
  const KURL& BaseURLOverride() const { return base_url_override_; }
  KURL ValidBaseElementURL() const;
  const AtomicString& BaseTarget() const { return base_target_; }
  void ProcessBaseElement();

  // Fallback base URL.
  // https://html.spec.whatwg.org/multipage/urls-and-fetching.html#fallback-base-url
  KURL FallbackBaseURL() const;

  // Creates URL based on passed relative url and this documents base URL.
  // Depending on base URL value it is possible that parent document
  // base URL will be used instead. Uses CompleteURLWithOverride internally.
  KURL CompleteURL(const String&) const;
  // Creates URL based on passed relative url and passed base URL override.
  KURL CompleteURLWithOverride(const String&,
                               const KURL& base_url_override) const;

  // Determines whether a new document should take on the same origin as that of
  // the document which created it.
  static bool ShouldInheritSecurityOriginFromOwner(const KURL&);

  String UserAgent() const final;
  void DisableEval(const String& error_message) final;

  CSSStyleSheet& ElementSheet();

  virtual DocumentParser* CreateParser();
  DocumentParser* Parser() const { return parser_.Get(); }
  ScriptableDocumentParser* GetScriptableDocumentParser() const;

  // FinishingPrinting denotes that the non-printing layout state is being
  // restored.
  enum PrintingState { kNotPrinting, kPrinting, kFinishingPrinting };
  bool Printing() const { return printing_ == kPrinting; }
  bool FinishingOrIsPrinting() {
    return printing_ == kPrinting || printing_ == kFinishingPrinting;
  }
  void SetPrinting(PrintingState state) { printing_ = state; }

  bool PaginatedForScreen() const { return paginated_for_screen_; }
  void SetPaginatedForScreen(bool p) { paginated_for_screen_ = p; }

  bool Paginated() const { return Printing() || PaginatedForScreen(); }

  enum CompatibilityMode { kQuirksMode, kLimitedQuirksMode, kNoQuirksMode };

  void SetCompatibilityMode(CompatibilityMode);
  CompatibilityMode GetCompatibilityMode() const { return compatibility_mode_; }

  String compatMode() const;

  bool InQuirksMode() const { return compatibility_mode_ == kQuirksMode; }
  bool InLimitedQuirksMode() const {
    return compatibility_mode_ == kLimitedQuirksMode;
  }
  bool InNoQuirksMode() const { return compatibility_mode_ == kNoQuirksMode; }

  // https://html.spec.whatwg.org/multipage/dom.html#documentreadystate
  enum DocumentReadyState { kLoading, kInteractive, kComplete };

  void SetReadyState(DocumentReadyState);
  bool IsLoadCompleted();

  enum ParsingState { kParsing, kInDOMContentLoaded, kFinishedParsing };
  void SetParsingState(ParsingState);
  bool Parsing() const { return parsing_state_ == kParsing; }
  bool HasFinishedParsing() const { return parsing_state_ == kFinishedParsing; }

  bool ShouldScheduleLayout() const;
  int ElapsedTime() const;

  bool CanCreateHistoryEntry() const;

  TextLinkColors& GetTextLinkColors() { return text_link_colors_; }
  const TextLinkColors& GetTextLinkColors() const { return text_link_colors_; }
  VisitedLinkState& GetVisitedLinkState() const { return *visited_link_state_; }

  MouseEventWithHitTestResults PerformMouseEventHitTest(const HitTestRequest&,
                                                        const LayoutPoint&,
                                                        const WebMouseEvent&);

  /* Newly proposed CSS3 mechanism for selecting alternate
       stylesheets using the DOM. May be subject to change as
       spec matures. - dwh
    */
  String preferredStylesheetSet() const;
  String selectedStylesheetSet() const;
  void setSelectedStylesheetSet(const String&);

  bool SetFocusedElement(Element*, const FocusParams&);
  void ClearFocusedElement();
  Element* FocusedElement() const { return focused_element_.Get(); }
  UserActionElementSet& UserActionElements() { return user_action_elements_; }
  const UserActionElementSet& UserActionElements() const {
    return user_action_elements_;
  }
  void SetAutofocusElement(Element*);
  Element* AutofocusElement() const { return autofocus_element_.Get(); }
  void SetSequentialFocusNavigationStartingPoint(Node*);
  Element* SequentialFocusNavigationStartingPoint(WebFocusType) const;

  void SetActiveElement(Element*);
  Element* GetActiveElement() const { return active_element_.Get(); }

  Element* HoverElement() const { return hover_element_.Get(); }

  void RemoveFocusedElementOfSubtree(Node*, bool among_children_only = false);
  void HoveredElementDetached(Element&);
  void ActiveChainNodeDetached(Element&);

  void UpdateHoverActiveState(const HitTestRequest&, Element*);

  // Updates for :target (CSS3 selector).
  void SetCSSTarget(Element*);
  Element* CssTarget() const { return css_target_; }

  void ScheduleLayoutTreeUpdateIfNeeded();
  bool HasPendingForcedStyleRecalc() const;

  void RegisterNodeList(const LiveNodeListBase*);
  void UnregisterNodeList(const LiveNodeListBase*);
  void RegisterNodeListWithIdNameCache(const LiveNodeListBase*);
  void UnregisterNodeListWithIdNameCache(const LiveNodeListBase*);
  bool ShouldInvalidateNodeListCaches(
      const QualifiedName* attr_name = nullptr) const;
  void InvalidateNodeListCaches(const QualifiedName* attr_name);

  void AttachNodeIterator(NodeIterator*);
  void DetachNodeIterator(NodeIterator*);
  void MoveNodeIteratorsToNewDocument(Node&, Document&);

  void AttachRange(Range*);
  void DetachRange(Range*);

  void DidMoveTreeToNewDocument(const Node& root);
  // nodeChildrenWillBeRemoved is used when removing all node children at once.
  void NodeChildrenWillBeRemoved(ContainerNode&);
  // nodeWillBeRemoved is only safe when removing one node at a time.
  void NodeWillBeRemoved(Node&);
  bool CanAcceptChild(const Node& new_child,
                      const Node* next,
                      const Node* old_child,
                      ExceptionState&) const;

  void DidInsertText(const CharacterData&, unsigned offset, unsigned length);
  void DidRemoveText(const CharacterData&, unsigned offset, unsigned length);
  void DidMergeTextNodes(const Text& merged_node,
                         const Text& node_to_be_removed,
                         unsigned old_length);
  void DidSplitTextNode(const Text& old_node);

  void ClearDOMWindow() { dom_window_ = nullptr; }
  LocalDOMWindow* domWindow() const { return dom_window_; }

  // Helper functions for forwarding LocalDOMWindow event related tasks to the
  // LocalDOMWindow if it exists.
  void SetWindowAttributeEventListener(const AtomicString& event_type,
                                       EventListener*);
  EventListener* GetWindowAttributeEventListener(
      const AtomicString& event_type);

  static void RegisterEventFactory(std::unique_ptr<EventFactoryBase>);
  static Event* createEvent(ScriptState*,
                            const String& event_type,
                            ExceptionState&);

  // keep track of what types of event listeners are registered, so we don't
  // dispatch events unnecessarily
  enum ListenerType {
    kDOMSubtreeModifiedListener = 1,
    kDOMNodeInsertedListener = 1 << 1,
    kDOMNodeRemovedListener = 1 << 2,
    kDOMNodeRemovedFromDocumentListener = 1 << 3,
    kDOMNodeInsertedIntoDocumentListener = 1 << 4,
    kDOMCharacterDataModifiedListener = 1 << 5,
    kAnimationEndListener = 1 << 6,
    kAnimationStartListener = 1 << 7,
    kAnimationIterationListener = 1 << 8,
    kTransitionEndListener = 1 << 9,
    kScrollListener = 1 << 10,
    kLoadListenerAtCapturePhaseOrAtStyleElement = 1 << 11
    // 4 bits remaining
  };

  bool HasListenerType(ListenerType listener_type) const {
    return (listener_types_ & listener_type);
  }
  void AddListenerTypeIfNeeded(const AtomicString& event_type, EventTarget&);

  bool HasMutationObserversOfType(MutationObserver::MutationType type) const {
    return mutation_observer_types_ & type;
  }
  bool HasMutationObservers() const { return mutation_observer_types_; }
  void AddMutationObserverTypes(MutationObserverOptions types) {
    mutation_observer_types_ |= types;
  }

  IntersectionObserverController* GetIntersectionObserverController();
  IntersectionObserverController& EnsureIntersectionObserverController();

  ResizeObserverController* GetResizeObserverController() const {
    return resize_observer_controller_;
  }
  ResizeObserverController& EnsureResizeObserverController();

  void UpdateViewportDescription();

  // Returns the owning element in the parent document. Returns nullptr if
  // this is the top level document or the owner is remote.
  HTMLFrameOwnerElement* LocalOwner() const;

  void WillChangeFrameOwnerProperties(int margin_width,
                                      int margin_height,
                                      ScrollbarMode,
                                      bool is_display_none);

  // Returns true if this document belongs to a frame that the parent document
  // made invisible (for instance by setting as style display:none).
  bool IsInInvisibleSubframe() const;

  String title() const { return title_; }
  void setTitle(const String&);

  Element* TitleElement() const { return title_element_.Get(); }
  void SetTitleElement(Element*);
  void RemoveTitle(Element* title_element);

  const AtomicString& dir();
  void setDir(const AtomicString&);

  String cookie(ExceptionState&) const;
  void setCookie(const String&, ExceptionState&);

  const AtomicString& referrer() const;

  String domain() const;
  void setDomain(const String& new_domain, ExceptionState&);

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
  const KURL& CookieURL() const { return cookie_url_; }
  void SetCookieURL(const KURL& url) { cookie_url_ = url; }

  const KURL FirstPartyForCookies() const;

  // The following implements the rule from HTML 4 for what valid names are.
  // To get this right for all the XML cases, we probably have to improve this
  // or move it and make it sensitive to the type of document.
  static bool IsValidName(const String&);

  // The following breaks a qualified name into a prefix and a local name.
  // It also does a validity check, and returns false if the qualified name
  // is invalid.  It also sets ExceptionCode when name is invalid.
  static bool ParseQualifiedName(const AtomicString& qualified_name,
                                 AtomicString& prefix,
                                 AtomicString& local_name,
                                 ExceptionState&);

  // Checks to make sure prefix and namespace do not conflict (per DOM Core 3)
  static bool HasValidNamespaceForElements(const QualifiedName&);
  static bool HasValidNamespaceForAttributes(const QualifiedName&);

  // "body element" as defined by HTML5
  // (https://html.spec.whatwg.org/multipage/dom.html#the-body-element-2).
  // That is, the first body or frameset child of the document element.
  HTMLElement* body() const;

  // "HTML body element" as defined by CSSOM View spec
  // (http://dev.w3.org/csswg/cssom-view/#the-html-body-element).
  // That is, the first body child of the document element.
  HTMLBodyElement* FirstBodyElement() const;

  void setBody(HTMLElement*, ExceptionState&);
  void WillInsertBody();

  HTMLHeadElement* head() const;

  // Decide which element is to define the viewport's overflow policy. If
  // |rootStyle| is set, use that as the style for the root element, rather than
  // obtaining it on our own. The reason for this is that style may not have
  // been associated with the elements yet - in which case it may have been
  // calculated on the fly (without associating it with the actual element)
  // somewhere.
  Element* ViewportDefiningElement(
      const ComputedStyle* root_style = nullptr) const;

  DocumentMarkerController& Markers() const { return *markers_; }

  // Support for Javascript execCommand, and related methods
  // See "core/editing/commands/DocumentExecCommand.cpp" for implementations.
  bool execCommand(const String& command,
                   bool show_ui,
                   const String& value,
                   ExceptionState&);
  bool IsRunningExecCommand() const { return is_running_exec_command_; }
  bool queryCommandEnabled(const String& command, ExceptionState&);
  bool queryCommandIndeterm(const String& command, ExceptionState&);
  bool queryCommandState(const String& command, ExceptionState&);
  bool queryCommandSupported(const String& command, ExceptionState&);
  String queryCommandValue(const String& command, ExceptionState&);

  KURL OpenSearchDescriptionURL();

  // designMode support
  bool InDesignMode() const { return design_mode_; }
  String designMode() const;
  void setDesignMode(const String&);

  // The document of the parent frame.
  Document* ParentDocument() const;
  Document& TopDocument() const;
  Document* ContextDocument();

  ScriptRunner* GetScriptRunner() { return script_runner_.Get(); }

  void currentScriptForBinding(HTMLScriptElementOrSVGScriptElement&) const;
  void PushCurrentScript(ScriptElementBase*);
  void PopCurrentScript(ScriptElementBase*);

  void SetTransformSource(std::unique_ptr<TransformSource>);
  TransformSource* GetTransformSource() const {
    return transform_source_.get();
  }

  void IncDOMTreeVersion() {
    DCHECK(lifecycle_.StateAllowsTreeMutations());
    dom_tree_version_ = ++global_tree_version_;
  }
  uint64_t DomTreeVersion() const { return dom_tree_version_; }

  uint64_t StyleVersion() const { return style_version_; }
  unsigned ForceLayoutCountForTesting() const { return force_layout_count_; }

  enum PendingSheetLayout {
    kNoLayoutWithPendingSheets,
    kDidLayoutWithPendingSheets,
    kIgnoreLayoutWithPendingSheets
  };

  bool DidLayoutWithPendingStylesheets() const {
    return pending_sheet_layout_ == kDidLayoutWithPendingSheets;
  }
  bool IgnoreLayoutWithPendingStylesheets() const {
    return pending_sheet_layout_ == kIgnoreLayoutWithPendingSheets;
  }

  bool HasNodesWithPlaceholderStyle() const {
    return has_nodes_with_placeholder_style_;
  }
  void SetHasNodesWithPlaceholderStyle() {
    has_nodes_with_placeholder_style_ = true;
  }

  Vector<IconURL> IconURLs(int icon_types_mask);

  Color ThemeColor() const;

  // Returns the HTMLLinkElement currently in use for the Web Manifest.
  // Returns null if there is no such element.
  HTMLLinkElement* LinkManifest() const;

  void UpdateFocusAppearanceLater();
  void CancelFocusAppearanceUpdate();

  bool IsDNSPrefetchEnabled() const { return is_dns_prefetch_enabled_; }
  void ParseDNSPrefetchControlHeader(const String&);

  void TasksWereSuspended() final;
  void TasksWereResumed() final;
  bool TasksNeedSuspension() final;

  void FinishedParsing();

  void SetEncodingData(const DocumentEncodingData& new_data);
  const WTF::TextEncoding& Encoding() const {
    return encoding_data_.Encoding();
  }

  bool EncodingWasDetectedHeuristically() const {
    return encoding_data_.WasDetectedHeuristically();
  }
  bool SawDecodingError() const { return encoding_data_.SawDecodingError(); }

  void SetAnnotatedRegionsDirty(bool f) { annotated_regions_dirty_ = f; }
  bool AnnotatedRegionsDirty() const { return annotated_regions_dirty_; }
  bool HasAnnotatedRegions() const { return has_annotated_regions_; }
  void SetHasAnnotatedRegions(bool f) { has_annotated_regions_ = f; }
  const Vector<AnnotatedRegionValue>& AnnotatedRegions() const;
  void SetAnnotatedRegions(const Vector<AnnotatedRegionValue>&);

  void RemoveAllEventListeners() final;

  const SVGDocumentExtensions* SvgExtensions();
  SVGDocumentExtensions& AccessSVGExtensions();

  // the first parameter specifies a policy to use as the document csp meaning
  // the document will take ownership of the policy
  // the second parameter specifies a policy to inherit meaning the document
  // will attempt to copy over the policy
  void InitContentSecurityPolicy(
      ContentSecurityPolicy* = nullptr,
      const ContentSecurityPolicy* policy_to_inherit = nullptr);

  bool IsSecureTransitionTo(const KURL&) const;

  bool AllowInlineEventHandler(Node*,
                               EventListener*,
                               const String& context_url,
                               const WTF::OrdinalNumber& context_line);

  void EnforceSandboxFlags(SandboxFlags mask) override;

  void StatePopped(PassRefPtr<SerializedScriptValue>);

  enum LoadEventProgress {
    kLoadEventNotRun,
    kLoadEventInProgress,
    kLoadEventCompleted,
    kBeforeUnloadEventInProgress,
    kBeforeUnloadEventCompleted,
    kPageHideInProgress,
    kUnloadVisibilityChangeInProgress,
    kUnloadEventInProgress,
    kUnloadEventHandled
  };
  bool LoadEventStillNeeded() const {
    return load_event_progress_ == kLoadEventNotRun;
  }
  bool LoadEventFinished() const {
    return load_event_progress_ >= kLoadEventCompleted;
  }
  bool UnloadStarted() const {
    return load_event_progress_ >= kPageHideInProgress;
  }
  bool ProcessingBeforeUnload() const {
    return load_event_progress_ == kBeforeUnloadEventInProgress;
  }
  void SuppressLoadEvent();

  void SetContainsPlugins() { contains_plugins_ = true; }
  bool ContainsPlugins() const { return contains_plugins_; }

  bool IsContextThread() const final;
  bool IsJSExecutionForbidden() const final { return false; }

  bool ContainsValidityStyleRules() const {
    return contains_validity_style_rules_;
  }
  void SetContainsValidityStyleRules() {
    contains_validity_style_rules_ = true;
  }

  void EnqueueResizeEvent();
  void EnqueueScrollEventForNode(Node*);
  void EnqueueAnimationFrameTask(WTF::Closure);
  void EnqueueAnimationFrameEvent(Event*);
  // Only one event for a target/event type combination will be dispatched per
  // frame.
  void EnqueueUniqueAnimationFrameEvent(Event*);
  void EnqueueMediaQueryChangeListeners(
      HeapVector<Member<MediaQueryListListener>>&);
  void EnqueueVisualViewportScrollEvent();
  void EnqueueVisualViewportResizeEvent();

  void DispatchEventsForPrinting();

  bool HasFullscreenSupplement() const { return has_fullscreen_supplement_; }
  void SetHasFullscreenSupplement() { has_fullscreen_supplement_ = true; }

  void exitPointerLock();
  Element* PointerLockElement() const;

  // Used to allow element that loads data without going through a FrameLoader
  // to delay the 'load' event.
  void IncrementLoadEventDelayCount() { ++load_event_delay_count_; }
  void DecrementLoadEventDelayCount();
  void CheckLoadEventSoon();
  bool IsDelayingLoadEvent();
  void LoadPluginsSoon();
  // This calls checkCompleted() sync and thus can cause JavaScript execution.
  void DecrementLoadEventDelayCountAndCheckLoadEvent();

  Touch* createTouch(DOMWindow*,
                     EventTarget*,
                     int identifier,
                     double page_x,
                     double page_y,
                     double screen_x,
                     double screen_y,
                     double radius_x,
                     double radius_y,
                     float rotation_angle,
                     float force) const;
  TouchList* createTouchList(HeapVector<Member<Touch>>&) const;

  const DocumentTiming& GetTiming() const { return document_timing_; }

  int RequestAnimationFrame(FrameRequestCallback*);
  void CancelAnimationFrame(int id);
  void ServiceScriptedAnimations(double monotonic_animation_start_time);

  int RequestIdleCallback(IdleRequestCallback*, const IdleRequestOptions&);
  void CancelIdleCallback(int id);

  EventTarget* ErrorEventTarget() final;
  void ExceptionThrown(ErrorEvent*) final;

  void InitDNSPrefetch();

  bool IsInDocumentWrite() const { return write_recursion_depth_ > 0; }

  TextAutosizer* GetTextAutosizer();

  Element* createElement(const AtomicString& local_name,
                         ExceptionState& = ASSERT_NO_EXCEPTION);
  Element* createElement(const AtomicString& local_name,
                         const StringOrDictionary&,
                         ExceptionState& = ASSERT_NO_EXCEPTION);
  Element* createElementNS(const AtomicString& namespace_uri,
                           const AtomicString& qualified_name,
                           const StringOrDictionary&,
                           ExceptionState&);
  ScriptValue registerElement(
      ScriptState*,
      const AtomicString& name,
      const ElementRegistrationOptions&,
      ExceptionState&,
      V0CustomElement::NameSet valid_names = V0CustomElement::kStandardNames);
  V0CustomElementRegistrationContext* RegistrationContext() {
    return registration_context_.Get();
  }
  V0CustomElementMicrotaskRunQueue* CustomElementMicrotaskRunQueue();

  void ClearImportsController();
  void CreateImportsController();
  HTMLImportsController* ImportsController() const {
    return imports_controller_;
  }
  HTMLImportLoader* ImportLoader() const;

  void DidLoadAllImports();

  void AdjustFloatQuadsForScrollAndAbsoluteZoom(Vector<FloatQuad>&,
                                                LayoutObject&);
  void AdjustFloatRectForScrollAndAbsoluteZoom(FloatRect&, LayoutObject&);

  void SetContextFeatures(ContextFeatures&);
  ContextFeatures& GetContextFeatures() const { return *context_features_; }

  ElementDataCache* GetElementDataCache() { return element_data_cache_.Get(); }

  void DidLoadAllScriptBlockingResources();
  void DidAddPendingStylesheetInBody();
  void DidRemoveAllPendingStylesheet();
  void DidRemoveAllPendingBodyStylesheets();

  bool InStyleRecalc() const {
    return lifecycle_.GetState() == DocumentLifecycle::kInStyleRecalc;
  }

  // Return a Locale for the default locale if the argument is null or empty.
  Locale& GetCachedLocale(const AtomicString& locale = g_null_atom);

  AnimationClock& GetAnimationClock();
  DocumentTimeline& Timeline() const { return *timeline_; }
  CompositorPendingAnimations& GetCompositorPendingAnimations() {
    return *compositor_pending_animations_;
  }

  void AddToTopLayer(Element*, const Element* before = nullptr);
  void RemoveFromTopLayer(Element*);
  const HeapVector<Member<Element>>& TopLayerElements() const {
    return top_layer_elements_;
  }
  HTMLDialogElement* ActiveModalDialog() const;

  // A non-null m_templateDocumentHost implies that |this| was created by
  // ensureTemplateDocument().
  bool IsTemplateDocument() const { return !!template_document_host_; }
  Document& EnsureTemplateDocument();
  Document* TemplateDocumentHost() { return template_document_host_; }

  mojom::EngagementLevel GetEngagementLevel() const {
    return engagement_level_;
  }
  void SetEngagementLevel(mojom::EngagementLevel level) {
    engagement_level_ = level;
  }

  void SetHasHighMediaEngagement(bool has_high_media_engagement) {
    has_high_media_engagement_ = has_high_media_engagement;
  }
  bool HasHighMediaEngagement() const { return has_high_media_engagement_; }

  // TODO(thestig): Rename these and related functions, since we can call them
  // for controls outside of forms as well.
  void DidAssociateFormControl(Element*);

  void AddConsoleMessage(ConsoleMessage*) final;

  LocalDOMWindow* ExecutingWindow() const final;
  LocalFrame* ExecutingFrame();

  DocumentLifecycle& Lifecycle() { return lifecycle_; }
  bool IsActive() const { return lifecycle_.IsActive(); }
  bool IsDetached() const {
    return lifecycle_.GetState() >= DocumentLifecycle::kStopping;
  }
  bool IsStopped() const {
    return lifecycle_.GetState() == DocumentLifecycle::kStopped;
  }

  enum HttpRefreshType { kHttpRefreshFromHeader, kHttpRefreshFromMetaTag };
  void MaybeHandleHttpRefresh(const String&, HttpRefreshType);

  void UpdateSecurityOrigin(RefPtr<SecurityOrigin>);

  void SetHasViewportUnits() { has_viewport_units_ = true; }
  bool HasViewportUnits() const { return has_viewport_units_; }
  void SetResizedForViewportUnits();
  void ClearResizedForViewportUnits();

  void UpdateActiveStyle();

  DECLARE_VIRTUAL_TRACE();

  DECLARE_VIRTUAL_TRACE_WRAPPERS();

  AtomicString ConvertLocalName(const AtomicString&);

  void PlatformColorsChanged();

  DOMTimerCoordinator* Timers() final;

  HostsUsingFeatures::Value& HostsUsingFeaturesValue() {
    return hosts_using_features_value_;
  }

  NthIndexCache* GetNthIndexCache() const { return nth_index_cache_; }

  bool IsSecureContext(String& error_message) const override;
  bool IsSecureContext() const override;

  ClientHintsPreferences& GetClientHintsPreferences() {
    return client_hints_preferences_;
  }

  CanvasFontCache* GetCanvasFontCache();

  // Used by unit tests so that all parsing will be main thread for
  // controlling parsing and chunking precisely.
  static void SetThreadedParsingEnabledForTesting(bool);
  static bool ThreadedParsingEnabledForTesting();

  void IncrementNodeCount() { node_count_++; }
  void DecrementNodeCount() {
    DCHECK_GT(node_count_, 0);
    node_count_--;
  }
  int NodeCount() const { return node_count_; }

  SnapCoordinator* GetSnapCoordinator();

  void EnforceInsecureRequestPolicy(WebInsecureRequestPolicy);

  bool MayContainV0Shadow() const { return may_contain_v0_shadow_; }

  ShadowCascadeOrder GetShadowCascadeOrder() const {
    return shadow_cascade_order_;
  }
  void SetShadowCascadeOrder(ShadowCascadeOrder);

  bool ContainsV1ShadowTree() const {
    return shadow_cascade_order_ == ShadowCascadeOrder::kShadowCascadeV1;
  }

  Element* rootScroller() const;
  void setRootScroller(Element*, ExceptionState& = ASSERT_NO_EXCEPTION);
  RootScrollerController& GetRootScrollerController() const {
    DCHECK(root_scroller_controller_);
    return *root_scroller_controller_;
  }

  bool IsInMainFrame() const;

  void RecordDeferredLoadReason(WouldLoadReason);
  WouldLoadReason DeferredLoadReason() { return would_load_reason_; }

  const PropertyRegistry* GetPropertyRegistry() const;
  PropertyRegistry* GetPropertyRegistry();

  // Document maintains a counter of visible non-secure password
  // fields in the page. Used to notify the embedder when all visible
  // non-secure password fields are no longer visible.
  void IncrementPasswordCount();
  void DecrementPasswordCount();
  // Used to notify the embedder when the user edits the value of a
  // text field in a non-secure context.
  void MaybeQueueSendDidEditFieldInInsecureContext();

  CoreProbeSink* GetProbeSink() final;

  void SetFeaturePolicy(const String& feature_policy_header);

 protected:
  Document(const DocumentInit&, DocumentClassFlags = kDefaultDocumentClass);

  void DidUpdateSecurityOrigin() final;

  void ClearXMLVersion() { xml_version_ = String(); }

  virtual Document* CloneDocumentWithoutChildren();

  bool ImportContainerNodeChildren(ContainerNode* old_container_node,
                                   ContainerNode* new_container_node,
                                   ExceptionState&);
  void LockCompatibilityMode() { compatibility_mode_locked_ = true; }
  ParserSynchronizationPolicy GetParserSynchronizationPolicy() const {
    return parser_sync_policy_;
  }

 private:
  friend class IgnoreDestructiveWriteCountIncrementer;
  friend class ThrowOnDynamicMarkupInsertionCountIncrementer;
  friend class NthIndexCache;
  class NetworkStateObserver;

  bool IsDocumentFragment() const =
      delete;  // This will catch anyone doing an unnecessary check.
  bool IsDocumentNode() const =
      delete;  // This will catch anyone doing an unnecessary check.
  bool IsElementNode() const =
      delete;  // This will catch anyone doing an unnecessary check.

  ScriptedAnimationController& EnsureScriptedAnimationController();
  ScriptedIdleTaskController& EnsureScriptedIdleTaskController();
  void InitSecurityContext(const DocumentInit&);
  SecurityContext& GetSecurityContext() final { return *this; }
  EventQueue* GetEventQueue() const final;

  bool HasPendingVisualUpdate() const {
    return lifecycle_.GetState() == DocumentLifecycle::kVisualUpdatePending;
  }

  bool ShouldScheduleLayoutTreeUpdate() const;
  void ScheduleLayoutTreeUpdate();

  bool NeedsFullLayoutTreeUpdate() const;

  void InheritHtmlAndBodyElementStyles(StyleRecalcChange);

  void UpdateUseShadowTreesIfNeeded();
  void EvaluateMediaQueryListIfNeeded();

  void UpdateStyleInvalidationIfNeeded();
  void UpdateStyle();
  void NotifyLayoutTreeOfSubtreeChanges();

  // ImplicitClose() actually does the work of closing the input stream.
  void ImplicitClose();
  bool ShouldComplete();

  void DetachParser();

  void BeginLifecycleUpdatesIfRenderingReady();

  bool IsDocument() const final { return true; }

  void ChildrenChanged(const ChildrenChange&) override;

  String nodeName() const final;
  NodeType getNodeType() const final;
  bool ChildTypeAllowed(NodeType) const final;
  Node* cloneNode(bool deep, ExceptionState&) final;
  void CloneDataFromDocument(const Document&);
  bool IsSecureContextImpl() const;

  ShadowCascadeOrder shadow_cascade_order_ = kShadowCascadeNone;

  // Same as url(), but needed for ExecutionContext to implement it without a
  // performance loss for direct calls.
  const KURL& VirtualURL() const final;
  // Same as completeURL() for the same reason as above.
  KURL VirtualCompleteURL(const String&) const final;

  void UpdateTitle(const String&);
  void UpdateFocusAppearanceTimerFired(TimerBase*);
  void UpdateBaseURL();

  void ExecuteScriptsWaitingForResources();

  void LoadEventDelayTimerFired(TimerBase*);
  void PluginLoadingTimerFired(TimerBase*);

  void AddListenerType(ListenerType listener_type) {
    listener_types_ |= listener_type;
  }
  void AddMutationEventListenerTypeIfEnabled(ListenerType);

  void DidAssociateFormControlsTimerFired(TimerBase*);

  void ClearFocusedElementSoon();
  void ClearFocusedElementTimerFired(TimerBase*);

  bool HaveScriptBlockingStylesheetsLoaded() const;
  bool HaveRenderBlockingStylesheetsLoaded() const;
  void StyleResolverMayHaveChanged();

  void SetHoverElement(Element*);

  using EventFactorySet = HashSet<std::unique_ptr<EventFactoryBase>>;
  static EventFactorySet& EventFactories();

  void SetNthIndexCache(NthIndexCache* nth_index_cache) {
    DCHECK(!nth_index_cache_ || !nth_index_cache);
    nth_index_cache_ = nth_index_cache;
  }

  const OriginAccessEntry& AccessEntryFromURL();

  void SendSensitiveInputVisibility();
  void SendSensitiveInputVisibilityInternal();
  void SendDidEditFieldInInsecureContext();

  bool HaveImportsLoaded() const;
  void ViewportDefiningElementDidChange();

  void UpdateActiveState(const HitTestRequest&, Element*);
  void UpdateHoverState(const HitTestRequest&, Element*);

  DocumentLifecycle lifecycle_;

  bool has_nodes_with_placeholder_style_;
  bool evaluate_media_queries_on_style_recalc_;

  // If we do ignore the pending stylesheet count, then we need to add a boolean
  // to track that this happened so that we can do a full repaint when the
  // stylesheets do eventually load.
  PendingSheetLayout pending_sheet_layout_;

  Member<LocalFrame> frame_;
  Member<LocalDOMWindow> dom_window_;
  TraceWrapperMember<HTMLImportsController> imports_controller_;

  Member<ResourceFetcher> fetcher_;
  TraceWrapperMember<DocumentParser> parser_;
  Member<ContextFeatures> context_features_;

  bool well_formed_;

  // Document URLs.
  KURL url_;  // Document.URL: The URL from which this document was retrieved.
  KURL base_url_;  // Node.baseURI: The URL to use when resolving relative URLs.
  KURL
      base_url_override_;  // An alternative base URL that takes precedence over
                           // m_baseURL (but not m_baseElementURL).
  KURL base_element_url_;  // The URL set by the <base> element.
  KURL cookie_url_;        // The URL to use for cookie access.
  std::unique_ptr<OriginAccessEntry> access_entry_from_url_;

  AtomicString base_target_;

  // Mime-type of the document in case it was cloned or created by XHR.
  AtomicString mime_type_;

  Member<DocumentType> doc_type_;
  TraceWrapperMember<DOMImplementation> implementation_;

  Member<CSSStyleSheet> elem_sheet_;

  PrintingState printing_;
  bool paginated_for_screen_;

  CompatibilityMode compatibility_mode_;
  // This is cheaper than making setCompatibilityMode virtual.
  bool compatibility_mode_locked_;

  TaskHandle execute_scripts_waiting_for_resources_task_handle_;

  bool has_autofocused_;
  TaskRunnerTimer<Document> clear_focused_element_timer_;
  Member<Element> autofocus_element_;
  Member<Element> focused_element_;
  Member<Range> sequential_focus_navigation_starting_point_;
  Member<Element> hover_element_;
  Member<Element> active_element_;
  Member<Element> document_element_;
  UserActionElementSet user_action_elements_;
  Member<RootScrollerController> root_scroller_controller_;

  uint64_t dom_tree_version_;
  static uint64_t global_tree_version_;

  uint64_t style_version_;
  unsigned force_layout_count_ = 0;

  HeapHashSet<WeakMember<NodeIterator>> node_iterators_;
  using AttachedRangeSet = HeapHashSet<WeakMember<Range>>;
  AttachedRangeSet ranges_;

  unsigned short listener_types_;

  MutationObserverOptions mutation_observer_types_;

  TraceWrapperMember<StyleEngine> style_engine_;
  TraceWrapperMember<StyleSheetList> style_sheet_list_;

  Member<FormController> form_controller_;

  TextLinkColors text_link_colors_;
  const Member<VisitedLinkState> visited_link_state_;

  bool visually_ordered_;

  DocumentReadyState ready_state_;
  ParsingState parsing_state_;

  bool goto_anchor_needed_after_stylesheets_load_;
  bool is_dns_prefetch_enabled_;
  bool have_explicitly_disabled_dns_prefetch_;
  bool contains_validity_style_rules_;
  bool contains_plugins_;

  // http://www.whatwg.org/specs/web-apps/current-work/#ignore-destructive-writes-counter
  unsigned ignore_destructive_write_count_;
  // https://html.spec.whatwg.org/#throw-on-dynamic-markup-insertion-counter
  unsigned throw_on_dynamic_markup_insertion_count_;

  String title_;
  String raw_title_;
  Member<Element> title_element_;

  Member<AXObjectCache> ax_object_cache_;
  Member<DocumentMarkerController> markers_;

  TaskRunnerTimer<Document> update_focus_appearance_timer_;

  Member<Element> css_target_;

  LoadEventProgress load_event_progress_;

  double start_time_;

  Member<ScriptRunner> script_runner_;

  HeapVector<Member<ScriptElementBase>> current_script_stack_;

  std::unique_ptr<TransformSource> transform_source_;

  String xml_encoding_;
  String xml_version_;
  unsigned xml_standalone_ : 2;
  unsigned has_xml_declaration_ : 1;

  AtomicString content_language_;

  DocumentEncodingData encoding_data_;

  bool design_mode_;
  bool is_running_exec_command_;

  HeapHashSet<WeakMember<const LiveNodeListBase>>
      lists_invalidated_at_document_;
  // Oilpan keeps track of all registered NodeLists.
  // TODO(Oilpan): improve - only need to know if a NodeList
  // is currently alive or not for the different types.
  HeapHashSet<WeakMember<const LiveNodeListBase>>
      node_lists_[kNumNodeListInvalidationTypes];

  Member<SVGDocumentExtensions> svg_extensions_;

  Vector<AnnotatedRegionValue> annotated_regions_;
  bool has_annotated_regions_;
  bool annotated_regions_dirty_;

  std::unique_ptr<SelectorQueryCache> selector_query_cache_;

  // It is safe to keep a raw, untraced pointer to this stack-allocated
  // cache object: it is set upon the cache object being allocated on
  // the stack and cleared upon leaving its allocated scope. Hence it
  // is acceptable not to trace it -- should a conservative GC occur,
  // the cache object's references will be traced by a stack walk.
  GC_PLUGIN_IGNORE("461878")
  NthIndexCache* nth_index_cache_ = nullptr;

  DocumentClassFlags document_classes_;

  bool is_view_source_;
  bool saw_elements_in_known_namespaces_;
  bool is_srcdoc_document_;
  bool is_mobile_document_;

  LayoutView* layout_view_;

  // The document of creator browsing context for frame-less documents such as
  // documents created by DOMParser and DOMImplementation.
  WeakMember<Document> context_document_;

  // For early return in Fullscreen::fromIfExists()
  bool has_fullscreen_supplement_;

  HeapVector<Member<Element>> top_layer_elements_;

  int load_event_delay_count_;
  TaskRunnerTimer<Document> load_event_delay_timer_;
  TaskRunnerTimer<Document> plugin_loading_timer_;

  ViewportDescription viewport_description_;
  ViewportDescription legacy_viewport_description_;
  Length viewport_default_min_width_;

  DocumentTiming document_timing_;
  Member<MediaQueryMatcher> media_query_matcher_;
  bool write_recursion_is_too_deep_;
  unsigned write_recursion_depth_;

  Member<ScriptedAnimationController> scripted_animation_controller_;
  Member<ScriptedIdleTaskController> scripted_idle_task_controller_;
  Member<TextAutosizer> text_autosizer_;

  Member<V0CustomElementRegistrationContext> registration_context_;
  Member<V0CustomElementMicrotaskRunQueue> custom_element_microtask_run_queue_;

  void ElementDataCacheClearTimerFired(TimerBase*);
  TaskRunnerTimer<Document> element_data_cache_clear_timer_;

  Member<ElementDataCache> element_data_cache_;

  using LocaleIdentifierToLocaleMap =
      HashMap<AtomicString, std::unique_ptr<Locale>>;
  LocaleIdentifierToLocaleMap locale_cache_;

  Member<DocumentTimeline> timeline_;
  Member<CompositorPendingAnimations> compositor_pending_animations_;

  Member<Document> template_document_;
  Member<Document> template_document_host_;

  TaskRunnerTimer<Document> did_associate_form_controls_timer_;

  HeapHashSet<Member<SVGUseElement>> use_elements_needing_update_;

  DOMTimerCoordinator timers_;

  bool has_viewport_units_;

  ParserSynchronizationPolicy parser_sync_policy_;

  HostsUsingFeatures::Value hosts_using_features_value_;

  ClientHintsPreferences client_hints_preferences_;

  Member<CanvasFontCache> canvas_font_cache_;

  Member<IntersectionObserverController> intersection_observer_controller_;
  Member<ResizeObserverController> resize_observer_controller_;

  int node_count_;

  bool may_contain_v0_shadow_ = false;

  Member<SnapCoordinator> snap_coordinator_;

  WouldLoadReason would_load_reason_;

  Member<PropertyRegistry> property_registry_;

  unsigned password_count_;

  bool logged_field_edit_;

  TaskHandle sensitive_input_visibility_task_;

  TaskHandle sensitive_input_edited_task_;

  mojom::EngagementLevel engagement_level_;

  Member<NetworkStateObserver> network_state_observer_;

  bool has_high_media_engagement_;
};

extern template class CORE_EXTERN_TEMPLATE_EXPORT Supplement<Document>;

inline bool Document::ShouldOverrideLegacyDescription(
    ViewportDescription::Type origin) const {
  // The different (legacy) meta tags have different priorities based on the
  // type regardless of which order they appear in the DOM. The priority is
  // given by the ViewportDescription::Type enum.
  return origin >= legacy_viewport_description_.type;
}

inline void Document::ScheduleLayoutTreeUpdateIfNeeded() {
  // Inline early out to avoid the function calls below.
  if (HasPendingVisualUpdate())
    return;
  if (ShouldScheduleLayoutTreeUpdate() && NeedsLayoutTreeUpdate())
    ScheduleLayoutTreeUpdate();
}

DEFINE_TYPE_CASTS(Document,
                  ExecutionContext,
                  context,
                  context->IsDocument(),
                  context.IsDocument());
DEFINE_NODE_TYPE_CASTS(Document, IsDocumentNode());

#define DEFINE_DOCUMENT_TYPE_CASTS(thisType)                                \
  DEFINE_TYPE_CASTS(thisType, Document, document, document->Is##thisType(), \
                    document.Is##thisType())

// This is needed to avoid ambiguous overloads with the Node and TreeScope
// versions.
DEFINE_COMPARISON_OPERATORS_WITH_REFERENCES(Document)

// Put these methods here, because they require the Document definition, but we
// really want to inline them.

inline bool Node::IsDocumentNode() const {
  return this == GetDocument();
}

Node* EventTargetNodeForDocument(Document*);

DEFINE_TYPE_CASTS(TreeScope, Document, document, true, true);

}  // namespace blink

#ifndef NDEBUG
// Outside the WebCore namespace for ease of invocation from gdb.
CORE_EXPORT void showLiveDocumentInstances();
#endif

#endif  // Document_h
