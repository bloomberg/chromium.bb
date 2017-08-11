/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Peter Kelly (pmk@post.com)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2011, 2013, 2014 Apple Inc. All rights reserved.
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

#ifndef Element_h
#define Element_h

#include "core/CoreExport.h"
#include "core/HTMLNames.h"
#include "core/css/CSSPrimitiveValue.h"
#include "core/css/CSSSelector.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/Attribute.h"
#include "core/dom/ContainerNode.h"
#include "core/dom/Document.h"
#include "core/dom/ElementData.h"
#include "core/dom/SpaceSplitString.h"
#include "core/dom/WhitespaceAttacher.h"
#include "core/resize_observer/ResizeObserver.h"
#include "platform/bindings/TraceWrapperMember.h"
#include "platform/heap/Handle.h"
#include "platform/scroll/ScrollTypes.h"
#include "public/platform/WebFocusType.h"

namespace blink {

class ElementAnimations;
class AccessibleNode;
class Attr;
class Attribute;
class CSSStyleDeclaration;
class CompositorMutation;
class CustomElementDefinition;
class DOMRect;
class DOMRectList;
class DOMStringMap;
class DOMTokenList;
class ElementRareData;
class ElementShadow;
class ExceptionState;
class Image;
class InputDeviceCapabilities;
class Locale;
class MutableStylePropertySet;
class NamedNodeMap;
class ElementIntersectionObserverData;
class PseudoElement;
class PseudoStyleRequest;
class ResizeObservation;
class ScrollIntoViewOptions;
class ScrollIntoViewOptionsOrBoolean;
class ScrollState;
class ScrollStateCallback;
class ScrollToOptions;
class ShadowRoot;
class ShadowRootInit;
class StylePropertySet;
class StylePropertyMap;
class V0CustomElementDefinition;

enum SpellcheckAttributeState {
  kSpellcheckAttributeTrue,
  kSpellcheckAttributeFalse,
  kSpellcheckAttributeDefault
};

enum ElementFlags {
  kTabIndexWasSetExplicitly = 1 << 0,
  kStyleAffectedByEmpty = 1 << 1,
  kIsInCanvasSubtree = 1 << 2,
  kContainsFullScreenElement = 1 << 3,
  kIsInTopLayer = 1 << 4,
  kHasPendingResources = 1 << 5,
  kAlreadySpellChecked = 1 << 6,
  kContainsPersistentVideo = 1 << 7,

  kNumberOfElementFlags = 8,  // Size of bitfield used to store the flags.
};

enum class ShadowRootType;

enum class SelectionBehaviorOnFocus {
  kReset,
  kRestore,
  kNone,
};

struct FocusParams {
  STACK_ALLOCATED();

  FocusParams() {}
  FocusParams(SelectionBehaviorOnFocus selection,
              WebFocusType focus_type,
              InputDeviceCapabilities* capabilities)
      : selection_behavior(selection),
        type(focus_type),
        source_capabilities(capabilities) {}

  SelectionBehaviorOnFocus selection_behavior =
      SelectionBehaviorOnFocus::kRestore;
  WebFocusType type = kWebFocusTypeNone;
  Member<InputDeviceCapabilities> source_capabilities = nullptr;
};

typedef HeapVector<TraceWrapperMember<Attr>> AttrNodeList;

class CORE_EXPORT Element : public ContainerNode {
  DEFINE_WRAPPERTYPEINFO();

 public:
  static Element* Create(const QualifiedName&, Document*);
  ~Element() override;

  DEFINE_ATTRIBUTE_EVENT_LISTENER(beforecopy);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(beforecut);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(beforepaste);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(copy);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(cut);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(gotpointercapture);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(lostpointercapture);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(paste);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(search);
  DEFINE_ATTRIBUTE_EVENT_LISTENER(selectstart);

  bool hasAttribute(const QualifiedName&) const;
  const AtomicString& getAttribute(const QualifiedName&) const;

  // Passing nullAtom as the second parameter removes the attribute when calling
  // either of these set methods.
  void setAttribute(const QualifiedName&, const AtomicString& value);
  void SetSynchronizedLazyAttribute(const QualifiedName&,
                                    const AtomicString& value);

  void removeAttribute(const QualifiedName&);

  // Typed getters and setters for language bindings.
  int GetIntegralAttribute(const QualifiedName& attribute_name) const;
  void SetIntegralAttribute(const QualifiedName& attribute_name, int value);
  void SetUnsignedIntegralAttribute(const QualifiedName& attribute_name,
                                    unsigned value,
                                    unsigned default_value = 0);
  double GetFloatingPointAttribute(
      const QualifiedName& attribute_name,
      double fallback_value = std::numeric_limits<double>::quiet_NaN()) const;
  void SetFloatingPointAttribute(const QualifiedName& attribute_name,
                                 double value);

  // Call this to get the value of an attribute that is known not to be the
  // style attribute or one of the SVG animatable attributes.
  bool FastHasAttribute(const QualifiedName&) const;
  const AtomicString& FastGetAttribute(const QualifiedName&) const;
#if DCHECK_IS_ON()
  bool FastAttributeLookupAllowed(const QualifiedName&) const;
#endif

#ifdef DUMP_NODE_STATISTICS
  bool HasNamedNodeMap() const;
#endif
  bool hasAttributes() const;

  bool hasAttribute(const AtomicString& name) const;
  bool hasAttributeNS(const AtomicString& namespace_uri,
                      const AtomicString& local_name) const;

  const AtomicString& getAttribute(const AtomicString& name) const;
  const AtomicString& getAttributeNS(const AtomicString& namespace_uri,
                                     const AtomicString& local_name) const;

  void setAttribute(const AtomicString& name,
                    const AtomicString& value,
                    ExceptionState& = ASSERT_NO_EXCEPTION);
  static bool ParseAttributeName(QualifiedName&,
                                 const AtomicString& namespace_uri,
                                 const AtomicString& qualified_name,
                                 ExceptionState&);
  void setAttributeNS(const AtomicString& namespace_uri,
                      const AtomicString& qualified_name,
                      const AtomicString& value,
                      ExceptionState&);

  const AtomicString& GetIdAttribute() const;
  void SetIdAttribute(const AtomicString&);

  const AtomicString& GetNameAttribute() const;
  const AtomicString& GetClassAttribute() const;

  // This is an operation defined in the DOM standard like:
  //   If element is in the HTML namespace and its node document is an HTML
  //   document, then set qualifiedName to qualifiedName in ASCII lowercase.
  //   https://dom.spec.whatwg.org/#concept-element-attributes-get-by-name
  AtomicString LowercaseIfNecessary(const AtomicString&) const;

  // NoncedElement implementation: this is only used by HTMLElement and
  // SVGElement, but putting the implementation here allows us to use
  // ElementRareData to hold the data.
  const AtomicString& nonce() const;
  void setNonce(const AtomicString&);

  // Call this to get the value of the id attribute for style resolution
  // purposes.  The value will already be lowercased if the document is in
  // compatibility mode, so this function is not suitable for non-style uses.
  const AtomicString& IdForStyleResolution() const;

  // This getter takes care of synchronizing all attributes before returning the
  // AttributeCollection. If the Element has no attributes, an empty
  // AttributeCollection will be returned. This is not a trivial getter and its
  // return value should be cached for performance.
  AttributeCollection Attributes() const;
  // This variant will not update the potentially invalid attributes. To be used
  // when not interested in style attribute or one of the SVG animation
  // attributes.
  AttributeCollection AttributesWithoutUpdate() const;

  void scrollIntoView(ScrollIntoViewOptionsOrBoolean);
  void scrollIntoView(bool align_to_top = true);
  void scrollIntoViewWithOptions(const ScrollIntoViewOptions&);
  void scrollIntoViewIfNeeded(bool center_if_needed = true);

  int OffsetLeft();
  int OffsetTop();
  int OffsetWidth();
  int OffsetHeight();

  Element* OffsetParent();
  int clientLeft();
  int clientTop();
  int clientWidth();
  int clientHeight();
  double scrollLeft();
  double scrollTop();
  void setScrollLeft(double);
  void setScrollTop(double);
  int scrollWidth();
  int scrollHeight();

  void scrollBy(double x, double y);
  virtual void scrollBy(const ScrollToOptions&);
  void scrollTo(double x, double y);
  virtual void scrollTo(const ScrollToOptions&);

  IntRect BoundsInViewport() const;
  // Returns an intersection rectangle of the bounds rectangle and the
  // viewport rectangle, in the visual viewport coordinate. This function is
  // used to show popups beside this element.
  IntRect VisibleBoundsInVisualViewport() const;

  DOMRectList* getClientRects();
  DOMRect* getBoundingClientRect();

  bool HasNonEmptyLayoutSize() const;

  const AtomicString& computedRole();
  String computedName();

  AccessibleNode* ExistingAccessibleNode() const;
  AccessibleNode* accessibleNode();

  void DidMoveToNewDocument(Document&) override;

  void removeAttribute(const AtomicString& name);
  void removeAttributeNS(const AtomicString& namespace_uri,
                         const AtomicString& local_name);

  Attr* DetachAttribute(size_t index);

  Attr* getAttributeNode(const AtomicString& name);
  Attr* getAttributeNodeNS(const AtomicString& namespace_uri,
                           const AtomicString& local_name);
  Attr* setAttributeNode(Attr*, ExceptionState&);
  Attr* setAttributeNodeNS(Attr*, ExceptionState&);
  Attr* removeAttributeNode(Attr*, ExceptionState&);

  Attr* AttrIfExists(const QualifiedName&);
  Attr* EnsureAttr(const QualifiedName&);

  AttrNodeList* GetAttrNodeList();

  CSSStyleDeclaration* style();
  StylePropertyMap* styleMap();

  const QualifiedName& TagQName() const { return tag_name_; }
  String tagName() const { return nodeName(); }

  bool HasTagName(const QualifiedName& tag_name) const {
    return tag_name_.Matches(tag_name);
  }
  bool HasTagName(const HTMLQualifiedName& tag_name) const {
    return ContainerNode::HasTagName(tag_name);
  }
  bool HasTagName(const SVGQualifiedName& tag_name) const {
    return ContainerNode::HasTagName(tag_name);
  }

  // Should be called only by Document::createElementNS to fix up m_tagName
  // immediately after construction.
  void SetTagNameForCreateElementNS(const QualifiedName&);

  // A fast function for checking the local name against another atomic string.
  bool HasLocalName(const AtomicString& other) const {
    return tag_name_.LocalName() == other;
  }

  const AtomicString& localName() const { return tag_name_.LocalName(); }
  AtomicString LocalNameForSelectorMatching() const;
  const AtomicString& prefix() const { return tag_name_.Prefix(); }
  const AtomicString& namespaceURI() const { return tag_name_.NamespaceURI(); }

  const AtomicString& LocateNamespacePrefix(
      const AtomicString& namespace_uri) const;

  String nodeName() const override;

  Element* CloneElementWithChildren();
  Element* CloneElementWithoutChildren();

  void SetBooleanAttribute(const QualifiedName&, bool);

  virtual const StylePropertySet* AdditionalPresentationAttributeStyle() {
    return nullptr;
  }
  void InvalidateStyleAttribute();

  const StylePropertySet* InlineStyle() const {
    return GetElementData() ? GetElementData()->inline_style_.Get() : nullptr;
  }

  void SetInlineStyleProperty(CSSPropertyID,
                              CSSValueID identifier,
                              bool important = false);
  void SetInlineStyleProperty(CSSPropertyID,
                              double value,
                              CSSPrimitiveValue::UnitType,
                              bool important = false);
  // TODO(sashab): Make this take a const CSSValue&.
  void SetInlineStyleProperty(CSSPropertyID,
                              const CSSValue*,
                              bool important = false);
  bool SetInlineStyleProperty(CSSPropertyID,
                              const String& value,
                              bool important = false);

  bool RemoveInlineStyleProperty(CSSPropertyID);
  void RemoveAllInlineStyleProperties();

  void SynchronizeStyleAttributeInternal() const;

  const StylePropertySet* PresentationAttributeStyle();
  virtual bool IsPresentationAttribute(const QualifiedName&) const {
    return false;
  }
  virtual void CollectStyleForPresentationAttribute(const QualifiedName&,
                                                    const AtomicString&,
                                                    MutableStylePropertySet*) {}

  // For exposing to DOM only.
  NamedNodeMap* attributesForBindings() const;
  Vector<AtomicString> getAttributeNames() const;

  enum class AttributeModificationReason { kDirectly, kByParser, kByCloning };
  struct AttributeModificationParams {
    STACK_ALLOCATED();
    AttributeModificationParams(const QualifiedName& qname,
                                const AtomicString& old_value,
                                const AtomicString& new_value,
                                AttributeModificationReason reason)
        : name(qname),
          old_value(old_value),
          new_value(new_value),
          reason(reason) {}

    const QualifiedName& name;
    const AtomicString& old_value;
    const AtomicString& new_value;
    const AttributeModificationReason reason;
  };

  // |attributeChanged| is called whenever an attribute is added, changed or
  // removed. It handles very common attributes such as id, class, name, style,
  // and slot.
  //
  // While the owner document is parsed, this function is called after all
  // attributes in a start tag were added to the element.
  virtual void AttributeChanged(const AttributeModificationParams&);

  // |parseAttribute| is called by |attributeChanged|. If an element
  // implementation needs to check an attribute update, override this function.
  //
  // While the owner document is parsed, this function is called after all
  // attributes in a start tag were added to the element.
  virtual void ParseAttribute(const AttributeModificationParams&);

  virtual bool HasLegalLinkAttribute(const QualifiedName&) const;
  virtual const QualifiedName& SubResourceAttributeName() const;

  // Only called by the parser immediately after element construction.
  void ParserSetAttributes(const Vector<Attribute>&);

  // Remove attributes that might introduce scripting from the vector leaving
  // the element unchanged.
  void StripScriptingAttributes(Vector<Attribute>&) const;

  bool SharesSameElementData(const Element& other) const {
    return GetElementData() == other.GetElementData();
  }

  // Clones attributes only.
  void CloneAttributesFromElement(const Element&);

  // Clones all attribute-derived data, including subclass specifics (through
  // copyNonAttributeProperties.)
  void CloneDataFromElement(const Element&);

  bool HasEquivalentAttributes(const Element* other) const;

  virtual void CopyNonAttributePropertiesFromElement(const Element&) {}

  void AttachLayoutTree(AttachContext&) override;
  void DetachLayoutTree(const AttachContext& = AttachContext()) override;

  virtual LayoutObject* CreateLayoutObject(const ComputedStyle&);
  virtual bool LayoutObjectIsNeeded(const ComputedStyle&);
  void RecalcStyle(StyleRecalcChange);
  bool NeedsRebuildLayoutTree(
      const WhitespaceAttacher& whitespace_attacher) const {
    return NeedsReattachLayoutTree() || ChildNeedsReattachLayoutTree() ||
           IsActiveSlotOrActiveV0InsertionPoint() ||
           (whitespace_attacher.LastTextNodeNeedsReattach() &&
            HasDisplayContentsStyle());
  }
  void RebuildLayoutTree(WhitespaceAttacher&);
  void PseudoStateChanged(CSSSelector::PseudoType);
  void SetAnimationStyleChange(bool);
  void ClearAnimationStyleChange();
  void SetNeedsAnimationStyleRecalc();

  void SetNeedsCompositingUpdate();

  ElementShadow* Shadow() const;
  ElementShadow& EnsureShadow();
  // If type of ShadowRoot (either closed or open) is explicitly specified,
  // creation of multiple shadow roots is prohibited in any combination and
  // throws an exception.  Multiple shadow roots are allowed only when
  // createShadowRoot() is used without any parameters from JavaScript.
  //
  // TODO(esprehn): These take a ScriptState only for calling
  // HostsUsingFeatures::countMainWorldOnly, which should be handled in the
  // bindings instead so adding a ShadowRoot from C++ doesn't need one.
  ShadowRoot* createShadowRoot(const ScriptState* = nullptr,
                               ExceptionState& = ASSERT_NO_EXCEPTION);
  ShadowRoot* attachShadow(const ScriptState*,
                           const ShadowRootInit&,
                           ExceptionState&);
  ShadowRoot* CreateShadowRootInternal(ShadowRootType, ExceptionState&);

  ShadowRoot* openShadowRoot() const;
  ShadowRoot* ClosedShadowRoot() const;
  ShadowRoot* AuthorShadowRoot() const;
  ShadowRoot* UserAgentShadowRoot() const;

  ShadowRoot* YoungestShadowRoot() const;

  ShadowRoot* ShadowRootIfV1() const;

  ShadowRoot& EnsureUserAgentShadowRoot();

  bool IsInDescendantTreeOf(const Element* shadow_host) const;

  // Returns the Element’s ComputedStyle. If the ComputedStyle is not already
  // stored on the Element, computes the ComputedStyle and stores it on the
  // Element’s ElementRareData.  Used for getComputedStyle when Element is
  // display none.
  const ComputedStyle* EnsureComputedStyle(PseudoId = kPseudoIdNone);

  const ComputedStyle* NonLayoutObjectComputedStyle() const;

  bool HasDisplayContentsStyle() const;

  ComputedStyle* MutableNonLayoutObjectComputedStyle() const {
    return const_cast<ComputedStyle*>(NonLayoutObjectComputedStyle());
  }

  bool ShouldStoreNonLayoutObjectComputedStyle(const ComputedStyle&) const;
  void StoreNonLayoutObjectComputedStyle(RefPtr<ComputedStyle>);

  // Methods for indicating the style is affected by dynamic updates (e.g.,
  // children changing, our position changing in our sibling list, etc.)
  bool StyleAffectedByEmpty() const {
    return HasElementFlag(kStyleAffectedByEmpty);
  }
  void SetStyleAffectedByEmpty() { SetElementFlag(kStyleAffectedByEmpty); }

  void SetIsInCanvasSubtree(bool value) {
    SetElementFlag(kIsInCanvasSubtree, value);
  }
  bool IsInCanvasSubtree() const { return HasElementFlag(kIsInCanvasSubtree); }

  bool IsDefined() const {
    return !(static_cast<int>(GetCustomElementState()) &
             static_cast<int>(CustomElementState::kNotDefinedFlag));
  }
  bool IsUpgradedV0CustomElement() {
    return GetV0CustomElementState() == kV0Upgraded;
  }
  bool IsUnresolvedV0CustomElement() {
    return GetV0CustomElementState() == kV0WaitingForUpgrade;
  }

  AtomicString ComputeInheritedLanguage() const;
  Locale& GetLocale() const;

  virtual void AccessKeyAction(bool /*sendToAnyEvent*/) {}

  virtual bool IsURLAttribute(const Attribute&) const { return false; }
  virtual bool IsHTMLContentAttribute(const Attribute&) const { return false; }
  bool IsJavaScriptURLAttribute(const Attribute&) const;
  virtual bool IsSVGAnimationAttributeSettingJavaScriptURL(
      const Attribute&) const {
    return false;
  }
  bool IsScriptingAttribute(const Attribute&) const;

  virtual bool IsLiveLink() const { return false; }
  KURL HrefURL() const;

  KURL GetURLAttribute(const QualifiedName&) const;
  KURL GetNonEmptyURLAttribute(const QualifiedName&) const;

  virtual const AtomicString ImageSourceURL() const;
  virtual Image* ImageContents() { return nullptr; }

  virtual void focus(const FocusParams& = FocusParams());
  virtual void UpdateFocusAppearance(SelectionBehaviorOnFocus);
  virtual void blur();

  void setDistributeScroll(ScrollStateCallback*, String native_scroll_behavior);
  void NativeDistributeScroll(ScrollState&);
  void setApplyScroll(ScrollStateCallback*, String native_scroll_behavior);
  void RemoveApplyScroll();
  void NativeApplyScroll(ScrollState&);

  void CallDistributeScroll(ScrollState&);
  void CallApplyScroll(ScrollState&);

  ScrollStateCallback* GetApplyScroll();

  // Whether this element can receive focus at all. Most elements are not
  // focusable but some elements, such as form controls and links, are. Unlike
  // layoutObjectIsFocusable(), this method may be called when layout is not up
  // to date, so it must not use the layoutObject to determine focusability.
  virtual bool SupportsFocus() const;
  // isFocusable(), isKeyboardFocusable(), and isMouseFocusable() check
  // whether the element can actually be focused. Callers should ensure
  // ComputedStyle is up to date;
  // e.g. by calling Document::updateLayoutTreeIgnorePendingStylesheets().
  bool IsFocusable() const;
  virtual bool IsKeyboardFocusable() const;
  virtual bool IsMouseFocusable() const;
  bool IsFocusedElementInDocument() const;
  Element* AdjustedFocusedElementInTreeScope() const;

  virtual void DispatchFocusEvent(
      Element* old_focused_element,
      WebFocusType,
      InputDeviceCapabilities* source_capabilities = nullptr);
  virtual void DispatchBlurEvent(
      Element* new_focused_element,
      WebFocusType,
      InputDeviceCapabilities* source_capabilities = nullptr);
  virtual void DispatchFocusInEvent(
      const AtomicString& event_type,
      Element* old_focused_element,
      WebFocusType,
      InputDeviceCapabilities* source_capabilities = nullptr);
  void DispatchFocusOutEvent(
      const AtomicString& event_type,
      Element* new_focused_element,
      InputDeviceCapabilities* source_capabilities = nullptr);

  virtual String innerText();
  String outerText();
  String innerHTML() const;
  String outerHTML() const;
  void setInnerHTML(const String&, ExceptionState& = ASSERT_NO_EXCEPTION);
  void setOuterHTML(const String&, ExceptionState&);

  Element* insertAdjacentElement(const String& where,
                                 Element* new_child,
                                 ExceptionState&);
  void insertAdjacentText(const String& where,
                          const String& text,
                          ExceptionState&);
  void insertAdjacentHTML(const String& where,
                          const String& html,
                          ExceptionState&);

  void setPointerCapture(int pointer_id, ExceptionState&);
  void releasePointerCapture(int pointer_id, ExceptionState&);

  // Returns true iff the element would capture the next pointer event. This
  // is true between a setPointerCapture call and a releasePointerCapture (or
  // implicit release) call:
  // https://w3c.github.io/pointerevents/#dom-element-haspointercapture
  bool hasPointerCapture(int pointer_id) const;

  // Returns true iff the element has received a gotpointercapture event for
  // the |pointerId| but hasn't yet received a lostpointercapture event for
  // the same id. The time window during which this is true is "delayed" from
  // (but overlapping with) the time window for hasPointerCapture():
  // https://w3c.github.io/pointerevents/#process-pending-pointer-capture
  bool HasProcessedPointerCapture(int pointer_id) const;

  String TextFromChildren();

  virtual String title() const { return String(); }
  virtual String DefaultToolTip() const { return String(); }

  virtual const AtomicString& ShadowPseudoId() const;
  // The specified string must start with "-webkit-" or "-internal-". The
  // former can be used as a selector in any places, and the latter can be
  // used only in UA stylesheet.
  void SetShadowPseudoId(const AtomicString&);

  // Called by the parser when this element's close tag is reached, signaling
  // that all child tags have been parsed and added.  This is needed for
  // <applet> and <object> elements, which can't lay themselves out until they
  // know all of their nested <param>s. [Radar 3603191, 4040848].  Also used for
  // script elements and some SVG elements for similar purposes, but making
  // parsing a special case in this respect should be avoided if possible.
  virtual void FinishParsingChildren();

  void BeginParsingChildren() { SetIsFinishedParsingChildren(false); }

  PseudoElement* GetPseudoElement(PseudoId) const;
  LayoutObject* PseudoElementLayoutObject(PseudoId) const;

  ComputedStyle* PseudoStyle(const PseudoStyleRequest&,
                             const ComputedStyle* parent_style = nullptr);
  RefPtr<ComputedStyle> GetUncachedPseudoStyle(
      const PseudoStyleRequest&,
      const ComputedStyle* parent_style = nullptr);
  bool CanGeneratePseudoElement(PseudoId) const;

  virtual bool MatchesDefaultPseudoClass() const { return false; }
  virtual bool MatchesEnabledPseudoClass() const { return false; }
  virtual bool MatchesReadOnlyPseudoClass() const { return false; }
  virtual bool MatchesReadWritePseudoClass() const { return false; }
  virtual bool MatchesValidityPseudoClasses() const { return false; }

  // https://dom.spec.whatwg.org/#dom-element-matches
  bool matches(const AtomicString& selectors,
               ExceptionState& = ASSERT_NO_EXCEPTION);

  // https://dom.spec.whatwg.org/#dom-element-closest
  Element* closest(const AtomicString& selectors,
                   ExceptionState& = ASSERT_NO_EXCEPTION);

  virtual bool ShouldAppearIndeterminate() const { return false; }

  DOMTokenList& classList();

  DOMStringMap& dataset();

  virtual bool IsDateTimeEditElement() const { return false; }
  virtual bool IsDateTimeFieldElement() const { return false; }
  virtual bool IsPickerIndicatorElement() const { return false; }

  virtual bool IsFormControlElement() const { return false; }
  virtual bool IsSpinButtonElement() const { return false; }
  virtual bool IsTextControl() const { return false; }
  virtual bool IsOptionalFormControl() const { return false; }
  virtual bool IsRequiredFormControl() const { return false; }
  virtual bool willValidate() const { return false; }
  virtual bool IsValidElement() { return false; }
  virtual bool IsInRange() const { return false; }
  virtual bool IsOutOfRange() const { return false; }
  virtual bool IsClearButtonElement() const { return false; }
  virtual bool IsScriptElement() const { return false; }

  bool CanContainRangeEndPoint() const override { return true; }

  // Used for disabled form elements; if true, prevents mouse events from being
  // dispatched to event listeners, and prevents DOMActivate events from being
  // sent at all.
  virtual bool IsDisabledFormControl() const { return false; }

  bool HasPendingResources() const {
    return HasElementFlag(kHasPendingResources);
  }
  void SetHasPendingResources() { SetElementFlag(kHasPendingResources); }
  void ClearHasPendingResources() { ClearElementFlag(kHasPendingResources); }
  virtual void BuildPendingResource() {}

  bool IsAlreadySpellChecked() const {
    return HasElementFlag(kAlreadySpellChecked);
  }
  void SetAlreadySpellChecked(bool value) {
    SetElementFlag(kAlreadySpellChecked, value);
  }

  void V0SetCustomElementDefinition(V0CustomElementDefinition*);
  V0CustomElementDefinition* GetV0CustomElementDefinition() const;

  void SetCustomElementDefinition(CustomElementDefinition*);
  CustomElementDefinition* GetCustomElementDefinition() const;

  bool ContainsFullScreenElement() const {
    return HasElementFlag(kContainsFullScreenElement);
  }
  void SetContainsFullScreenElement(bool);
  void SetContainsFullScreenElementOnAncestorsCrossingFrameBoundaries(bool);

  bool ContainsPersistentVideo() const {
    return HasElementFlag(kContainsPersistentVideo);
  }
  void SetContainsPersistentVideo(bool);

  bool IsInTopLayer() const { return HasElementFlag(kIsInTopLayer); }
  void SetIsInTopLayer(bool);

  void requestPointerLock();

  bool IsSpellCheckingEnabled() const;

  // FIXME: public for LayoutTreeBuilder, we shouldn't expose this though.
  RefPtr<ComputedStyle> StyleForLayoutObject();

  bool HasID() const;
  bool HasClass() const;
  const SpaceSplitString& ClassNames() const;

  ScrollOffset SavedLayerScrollOffset() const;
  void SetSavedLayerScrollOffset(const ScrollOffset&);

  ElementAnimations* GetElementAnimations() const;
  ElementAnimations& EnsureElementAnimations();
  bool HasAnimations() const;

  void SynchronizeAttribute(const AtomicString& local_name) const;

  MutableStylePropertySet& EnsureMutableInlineStyle();
  void ClearMutableInlineStyleIfEmpty();

  void setTabIndex(int);
  int tabIndex() const override;

  void UpdateFromCompositorMutation(const CompositorMutation&);

  // Helpers for V8DOMActivityLogger::logEvent.  They call logEvent only if
  // the element is isConnected() and the context is an isolated world.
  void LogAddElementIfIsolatedWorldAndInDocument(const char element[],
                                                 const QualifiedName& attr1);
  void LogAddElementIfIsolatedWorldAndInDocument(const char element[],
                                                 const QualifiedName& attr1,
                                                 const QualifiedName& attr2);
  void LogAddElementIfIsolatedWorldAndInDocument(const char element[],
                                                 const QualifiedName& attr1,
                                                 const QualifiedName& attr2,
                                                 const QualifiedName& attr3);
  void LogUpdateAttributeIfIsolatedWorldAndInDocument(
      const char element[],
      const AttributeModificationParams&);

  DECLARE_VIRTUAL_TRACE();

  DECLARE_VIRTUAL_TRACE_WRAPPERS();

  SpellcheckAttributeState GetSpellcheckAttributeState() const;

  ElementIntersectionObserverData* IntersectionObserverData() const;
  ElementIntersectionObserverData& EnsureIntersectionObserverData();

  HeapHashMap<TraceWrapperMember<ResizeObserver>, Member<ResizeObservation>>*
  ResizeObserverData() const;
  HeapHashMap<TraceWrapperMember<ResizeObserver>, Member<ResizeObservation>>&
  EnsureResizeObserverData();
  void SetNeedsResizeObserverUpdate();

 protected:
  Element(const QualifiedName& tag_name, Document*, ConstructionType);

  const ElementData* GetElementData() const { return element_data_.Get(); }
  UniqueElementData& EnsureUniqueElementData();

  void AddPropertyToPresentationAttributeStyle(MutableStylePropertySet*,
                                               CSSPropertyID,
                                               CSSValueID identifier);
  void AddPropertyToPresentationAttributeStyle(MutableStylePropertySet*,
                                               CSSPropertyID,
                                               double value,
                                               CSSPrimitiveValue::UnitType);
  void AddPropertyToPresentationAttributeStyle(MutableStylePropertySet*,
                                               CSSPropertyID,
                                               const String& value);
  // TODO(sashab): Make this take a const CSSValue&.
  void AddPropertyToPresentationAttributeStyle(MutableStylePropertySet*,
                                               CSSPropertyID,
                                               const CSSValue*);

  InsertionNotificationRequest InsertedInto(ContainerNode*) override;
  void RemovedFrom(ContainerNode*) override;
  void ChildrenChanged(const ChildrenChange&) override;

  virtual void WillRecalcStyle(StyleRecalcChange);
  virtual void DidRecalcStyle();
  virtual RefPtr<ComputedStyle> CustomStyleForLayoutObject();

  virtual bool ShouldRegisterAsNamedItem() const { return false; }
  virtual bool ShouldRegisterAsExtraNamedItem() const { return false; }

  bool SupportsSpatialNavigationFocus() const;

  void ClearTabIndexExplicitlyIfNeeded();
  void SetTabIndexExplicitly();
  // Subclasses may override this method to affect focusability. This method
  // must be called on an up-to-date ComputedStyle, so it may use existence of
  // layoutObject and the LayoutObject::style() to reason about focusability.
  // However, it must not retrieve layout information like position and size.
  // This method cannot be moved to LayoutObject because some focusable nodes
  // don't have layoutObjects. e.g., HTMLOptionElement.
  // TODO(tkent): Rename this to isFocusableStyle.
  virtual bool LayoutObjectIsFocusable() const;

  // classAttributeChanged() exists to share code between
  // parseAttribute (called via setAttribute()) and
  // svgAttributeChanged (called when element.className.baseValue is set)
  void ClassAttributeChanged(const AtomicString& new_class_string);

  static bool AttributeValueIsJavaScriptURL(const Attribute&);

  RefPtr<ComputedStyle> OriginalStyleForLayoutObject();

  Node* InsertAdjacent(const String& where, Node* new_child, ExceptionState&);

  virtual void ParserDidSetAttributes() {}

 private:
  void ScrollLayoutBoxBy(const ScrollToOptions&);
  void ScrollLayoutBoxTo(const ScrollToOptions&);
  void ScrollFrameBy(const ScrollToOptions&);
  void ScrollFrameTo(const ScrollToOptions&);

  bool HasElementFlag(ElementFlags mask) const {
    return HasRareData() && HasElementFlagInternal(mask);
  }
  void SetElementFlag(ElementFlags, bool value = true);
  void ClearElementFlag(ElementFlags);
  bool HasElementFlagInternal(ElementFlags) const;

  bool IsElementNode() const =
      delete;  // This will catch anyone doing an unnecessary check.
  bool IsDocumentFragment() const =
      delete;  // This will catch anyone doing an unnecessary check.
  bool IsDocumentNode() const =
      delete;  // This will catch anyone doing an unnecessary check.

  void StyleAttributeChanged(const AtomicString& new_style_string,
                             AttributeModificationReason);

  void UpdatePresentationAttributeStyle();

  void InlineStyleChanged();
  void SetInlineStyleFromString(const AtomicString&);

  // If the only inherited changes in the parent element are independent,
  // these changes can be directly propagated to this element (the child).
  // If these conditions are met, propagates the changes to the current style
  // and returns the new style. Otherwise, returns null.
  RefPtr<ComputedStyle> PropagateInheritedProperties(StyleRecalcChange);

  StyleRecalcChange RecalcOwnStyle(StyleRecalcChange);
  void RebuildPseudoElementLayoutTree(PseudoId, WhitespaceAttacher&);
  void RebuildShadowRootLayoutTree(WhitespaceAttacher&);
  inline void CheckForEmptyStyleChange();

  void UpdatePseudoElement(PseudoId, StyleRecalcChange);
  bool UpdateFirstLetter(Element*);

  inline void CreatePseudoElementIfNeeded(PseudoId);

  ShadowRoot* GetShadowRoot() const;

  // FIXME: Everyone should allow author shadows.
  virtual bool AreAuthorShadowsAllowed() const { return true; }
  virtual void DidAddUserAgentShadowRoot(ShadowRoot&) {}
  virtual bool AlwaysCreateUserAgentShadowRoot() const { return false; }

  enum SynchronizationOfLazyAttribute {
    kNotInSynchronizationOfLazyAttribute = 0,
    kInSynchronizationOfLazyAttribute
  };

  void DidAddAttribute(const QualifiedName&, const AtomicString&);
  void WillModifyAttribute(const QualifiedName&,
                           const AtomicString& old_value,
                           const AtomicString& new_value);
  void DidModifyAttribute(const QualifiedName&,
                          const AtomicString& old_value,
                          const AtomicString& new_value);
  void DidRemoveAttribute(const QualifiedName&, const AtomicString& old_value);

  void SynchronizeAllAttributes() const;
  void SynchronizeAttribute(const QualifiedName&) const;

  void UpdateId(const AtomicString& old_id, const AtomicString& new_id);
  void UpdateId(TreeScope&,
                const AtomicString& old_id,
                const AtomicString& new_id);
  void UpdateName(const AtomicString& old_name, const AtomicString& new_name);

  void ClientQuads(Vector<FloatQuad>& quads);

  NodeType getNodeType() const final;
  bool ChildTypeAllowed(NodeType) const final;

  void SetAttributeInternal(size_t index,
                            const QualifiedName&,
                            const AtomicString& value,
                            SynchronizationOfLazyAttribute);
  void AppendAttributeInternal(const QualifiedName&,
                               const AtomicString& value,
                               SynchronizationOfLazyAttribute);
  void RemoveAttributeInternal(size_t index, SynchronizationOfLazyAttribute);
  void AttributeChangedFromParserOrByCloning(const QualifiedName&,
                                             const AtomicString&,
                                             AttributeModificationReason);

  void CancelFocusAppearanceUpdate();

  const ComputedStyle* VirtualEnsureComputedStyle(
      PseudoId pseudo_element_specifier = kPseudoIdNone) override {
    return EnsureComputedStyle(pseudo_element_specifier);
  }

  inline void UpdateCallbackSelectors(const ComputedStyle* old_style,
                                      const ComputedStyle* new_style);
  inline void RemoveCallbackSelectors();
  inline void AddCallbackSelectors();

  // cloneNode is private so that non-virtual cloneElementWithChildren and
  // cloneElementWithoutChildren are used instead.
  Node* cloneNode(bool deep, ExceptionState&) override;
  virtual Element* CloneElementWithoutAttributesAndChildren();

  QualifiedName tag_name_;

  void UpdateNamedItemRegistration(const AtomicString& old_name,
                                   const AtomicString& new_name);
  void UpdateExtraNamedItemRegistration(const AtomicString& old_name,
                                        const AtomicString& new_name);

  void CreateUniqueElementData();

  bool ShouldInvalidateDistributionWhenAttributeChanged(ElementShadow*,
                                                        const QualifiedName&,
                                                        const AtomicString&);

  ElementRareData* GetElementRareData() const;
  ElementRareData& EnsureElementRareData();

  void RemoveAttrNodeList();
  void DetachAllAttrNodesFromElement();
  void DetachAttrNodeFromElementWithValue(Attr*, const AtomicString& value);
  void DetachAttrNodeAtIndex(Attr*, size_t index);

  Member<ElementData> element_data_;
};

DEFINE_NODE_TYPE_CASTS(Element, IsElementNode());
template <typename T>
bool IsElementOfType(const Node&);
template <>
inline bool IsElementOfType<const Element>(const Node& node) {
  return node.IsElementNode();
}
template <typename T>
inline bool IsElementOfType(const Element& element) {
  return IsElementOfType<T>(static_cast<const Node&>(element));
}
template <>
inline bool IsElementOfType<const Element>(const Element&) {
  return true;
}

// Type casting.
template <typename T>
inline T& ToElement(Node& node) {
  SECURITY_DCHECK(IsElementOfType<const T>(node));
  return static_cast<T&>(node);
}
template <typename T>
inline T* ToElement(Node* node) {
  SECURITY_DCHECK(!node || IsElementOfType<const T>(*node));
  return static_cast<T*>(node);
}
template <typename T>
inline const T& ToElement(const Node& node) {
  SECURITY_DCHECK(IsElementOfType<const T>(node));
  return static_cast<const T&>(node);
}
template <typename T>
inline const T* ToElement(const Node* node) {
  SECURITY_DCHECK(!node || IsElementOfType<const T>(*node));
  return static_cast<const T*>(node);
}

template <typename T>
inline T& ToElementOrDie(Node& node) {
  CHECK(IsElementOfType<const T>(node));
  return static_cast<T&>(node);
}
template <typename T>
inline T* ToElementOrDie(Node* node) {
  CHECK(!node || IsElementOfType<const T>(*node));
  return static_cast<T*>(node);
}
template <typename T>
inline const T& ToElementOrDie(const Node& node) {
  CHECK(IsElementOfType<const T>(node));
  return static_cast<const T&>(node);
}
template <typename T>
inline const T* ToElementOrDie(const Node* node) {
  CHECK(!node || IsElementOfType<const T>(*node));
  return static_cast<const T*>(node);
}

inline bool IsDisabledFormControl(const Node* node) {
  return node->IsElementNode() && ToElement(node)->IsDisabledFormControl();
}

inline Element* Node::parentElement() const {
  ContainerNode* parent = parentNode();
  return parent && parent->IsElementNode() ? ToElement(parent) : nullptr;
}

inline bool Element::FastHasAttribute(const QualifiedName& name) const {
#if DCHECK_IS_ON()
  DCHECK(FastAttributeLookupAllowed(name));
#endif
  return GetElementData() &&
         GetElementData()->Attributes().FindIndex(name) != kNotFound;
}

inline const AtomicString& Element::FastGetAttribute(
    const QualifiedName& name) const {
#if DCHECK_IS_ON()
  DCHECK(FastAttributeLookupAllowed(name));
#endif
  if (GetElementData()) {
    if (const Attribute* attribute = GetElementData()->Attributes().Find(name))
      return attribute->Value();
  }
  return g_null_atom;
}

inline AttributeCollection Element::Attributes() const {
  if (!GetElementData())
    return AttributeCollection();
  SynchronizeAllAttributes();
  return GetElementData()->Attributes();
}

inline AttributeCollection Element::AttributesWithoutUpdate() const {
  if (!GetElementData())
    return AttributeCollection();
  return GetElementData()->Attributes();
}

inline bool Element::hasAttributes() const {
  return !Attributes().IsEmpty();
}

inline const AtomicString& Element::IdForStyleResolution() const {
  DCHECK(HasID());
  return GetElementData()->IdForStyleResolution();
}

inline const AtomicString& Element::GetIdAttribute() const {
  return HasID() ? FastGetAttribute(HTMLNames::idAttr) : g_null_atom;
}

inline const AtomicString& Element::GetNameAttribute() const {
  return HasName() ? FastGetAttribute(HTMLNames::nameAttr) : g_null_atom;
}

inline const AtomicString& Element::GetClassAttribute() const {
  if (!HasClass())
    return g_null_atom;
  if (IsSVGElement())
    return getAttribute(HTMLNames::classAttr);
  return FastGetAttribute(HTMLNames::classAttr);
}

inline void Element::SetIdAttribute(const AtomicString& value) {
  setAttribute(HTMLNames::idAttr, value);
}

inline const SpaceSplitString& Element::ClassNames() const {
  DCHECK(HasClass());
  DCHECK(GetElementData());
  return GetElementData()->ClassNames();
}

inline bool Element::HasID() const {
  return GetElementData() && GetElementData()->HasID();
}

inline bool Element::HasClass() const {
  return GetElementData() && GetElementData()->HasClass();
}

inline UniqueElementData& Element::EnsureUniqueElementData() {
  if (!GetElementData() || !GetElementData()->IsUnique())
    CreateUniqueElementData();
  return ToUniqueElementData(*element_data_);
}

inline Node::InsertionNotificationRequest Node::InsertedInto(
    ContainerNode* insertion_point) {
  DCHECK(!ChildNeedsStyleInvalidation());
  DCHECK(!NeedsStyleInvalidation());
  DCHECK(insertion_point->isConnected() || insertion_point->IsInShadowTree() ||
         IsContainerNode());
  if (insertion_point->isConnected()) {
    SetFlag(kIsConnectedFlag);
    insertion_point->GetDocument().IncrementNodeCount();
  }
  if (ParentOrShadowHostNode()->IsInShadowTree())
    SetFlag(kIsInShadowTreeFlag);
  if (ChildNeedsDistributionRecalc() &&
      !insertion_point->ChildNeedsDistributionRecalc())
    insertion_point->MarkAncestorsWithChildNeedsDistributionRecalc();
  return kInsertionDone;
}

inline void Node::RemovedFrom(ContainerNode* insertion_point) {
  DCHECK(insertion_point->isConnected() || IsContainerNode() ||
         IsInShadowTree());
  if (insertion_point->isConnected()) {
    ClearFlag(kIsConnectedFlag);
    insertion_point->GetDocument().DecrementNodeCount();
  }
  if (IsInShadowTree() && !ContainingTreeScope().RootNode().IsShadowRoot())
    ClearFlag(kIsInShadowTreeFlag);
  if (AXObjectCache* cache = GetDocument().ExistingAXObjectCache())
    cache->Remove(this);
}

inline void Element::InvalidateStyleAttribute() {
  DCHECK(GetElementData());
  GetElementData()->style_attribute_is_dirty_ = true;
}

inline const StylePropertySet* Element::PresentationAttributeStyle() {
  if (!GetElementData())
    return nullptr;
  if (GetElementData()->presentation_attribute_style_is_dirty_)
    UpdatePresentationAttributeStyle();
  // Need to call elementData() again since updatePresentationAttributeStyle()
  // might swap it with a UniqueElementData.
  return GetElementData()->PresentationAttributeStyle();
}

inline void Element::SetTagNameForCreateElementNS(
    const QualifiedName& tag_name) {
  // We expect this method to be called only to reset the prefix.
  DCHECK_EQ(tag_name.LocalName(), tag_name_.LocalName());
  DCHECK_EQ(tag_name.NamespaceURI(), tag_name_.NamespaceURI());
  tag_name_ = tag_name;
}

inline AtomicString Element::LocalNameForSelectorMatching() const {
  if (IsHTMLElement() || !GetDocument().IsHTMLDocument())
    return localName();
  return localName().DeprecatedLower();
}

inline bool IsShadowHost(const Node* node) {
  return node && node->IsElementNode() && ToElement(node)->Shadow();
}

inline bool IsShadowHost(const Node& node) {
  return node.IsElementNode() && ToElement(node).Shadow();
}

inline bool IsShadowHost(const Element* element) {
  return element && element->Shadow();
}

inline bool IsShadowHost(const Element& element) {
  return element.Shadow();
}

inline bool IsAtShadowBoundary(const Element* element) {
  if (!element)
    return false;
  ContainerNode* parent_node = element->parentNode();
  return parent_node && parent_node->IsShadowRoot();
}

// These macros do the same as their NODE equivalents but additionally provide a
// template specialization for isElementOfType<>() so that the Traversal<> API
// works for these Element types.
#define DEFINE_ELEMENT_TYPE_CASTS(thisType, predicate)            \
  template <>                                                     \
  inline bool IsElementOfType<const thisType>(const Node& node) { \
    return node.predicate;                                        \
  }                                                               \
  DEFINE_NODE_TYPE_CASTS(thisType, predicate)

#define DEFINE_ELEMENT_TYPE_CASTS_WITH_FUNCTION(thisType)         \
  template <>                                                     \
  inline bool IsElementOfType<const thisType>(const Node& node) { \
    return Is##thisType(node);                                    \
  }                                                               \
  DEFINE_NODE_TYPE_CASTS_WITH_FUNCTION(thisType)

#define DECLARE_ELEMENT_FACTORY_WITH_TAGNAME(T) \
  static T* Create(const QualifiedName&, Document&)
#define DEFINE_ELEMENT_FACTORY_WITH_TAGNAME(T)                     \
  T* T::Create(const QualifiedName& tagName, Document& document) { \
    return new T(tagName, document);                               \
  }

}  // namespace blink

#endif  // Element_h
