/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004-2007, 2009, 2014 Apple Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_ELEMENT_H_
#define THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_ELEMENT_H_

#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/renderer/core/core_export.h"
#include "third_party/blink/renderer/core/dom/element.h"
#include "third_party/blink/renderer/core/dom/events/simulated_click_options.h"
#include "third_party/blink/renderer/core/html/forms/labels_node_list.h"
#include "third_party/blink/renderer/platform/text/text_direction.h"
#include "third_party/blink/renderer/platform/wtf/casting.h"

namespace blink {

struct AttributeTriggers;
class Color;
class DocumentFragment;
class ElementInternals;
class ExceptionState;
class FormAssociated;
class HTMLFormElement;
class KeyboardEvent;
class V8UnionStringTreatNullAsEmptyStringOrTrustedScript;

enum TranslateAttributeMode {
  kTranslateAttributeYes,
  kTranslateAttributeNo,
  kTranslateAttributeInherit
};

enum class ContentEditableType {
  kInherit,
  kContentEditable,
  kNotContentEditable,
  kPlaintextOnly,
};

class CORE_EXPORT HTMLElement : public Element {
  DEFINE_WRAPPERTYPEINFO();

 public:

  HTMLElement(const QualifiedName& tag_name, Document&, ConstructionType);

  bool HasTagName(const HTMLQualifiedName& name) const {
    return HasLocalName(name.LocalName());
  }

  String title() const final;

  String innerText();
  void setInnerText(const String&, ExceptionState&);
  V8UnionStringTreatNullAsEmptyStringOrTrustedScript* innerTextForBinding();
  virtual void setInnerTextForBinding(
      const V8UnionStringTreatNullAsEmptyStringOrTrustedScript*
          string_or_trusted_script,
      ExceptionState& exception_state);
  void setOuterText(const String&, ExceptionState&);

  virtual bool HasCustomFocusLogic() const;

  ContentEditableType contentEditableNormalized() const;
  String contentEditable() const;
  void setContentEditable(const String&, ExceptionState&);
  // For HTMLElement.prototype.isContentEditable. This matches to neither
  // blink::isContentEditable() nor blink::isContentRichlyEditable().  Do not
  // use this function in Blink.
  bool isContentEditableForBinding() const;

  virtual const AtomicString& autocapitalize() const;
  void setAutocapitalize(const AtomicString&);

  virtual bool draggable() const;
  void setDraggable(bool);

  bool spellcheck() const;
  void setSpellcheck(bool);

  bool translate() const;
  void setTranslate(bool);

  const AtomicString& dir();
  void setDir(const AtomicString&);

  void click();

  void AccessKeyAction(SimulatedClickCreationScope creation_scope) override;

  bool ShouldSerializeEndTag() const;

  virtual HTMLFormElement* formOwner() const;

  HTMLFormElement* FindFormAncestor() const;

  bool HasDirectionAuto() const;

  virtual bool IsHTMLBodyElement() const { return false; }
  // TODO(crbug.com/1123606): Remove this virtual method once the fenced frame
  // origin trial is over.
  virtual bool IsHTMLFencedFrameElement() const { return false; }
  virtual bool IsHTMLFrameSetElement() const { return false; }
  virtual bool IsHTMLPortalElement() const { return false; }
  virtual bool IsHTMLUnknownElement() const { return false; }
  virtual bool IsPluginElement() const { return false; }

  // https://html.spec.whatwg.org/C/#category-label
  virtual bool IsLabelable() const;
  // |labels| IDL attribute implementation for IsLabelable()==true elements.
  LabelsNodeList* labels();

  // https://html.spec.whatwg.org/C/#interactive-content
  virtual bool IsInteractiveContent() const;
  void DefaultEventHandler(Event&) override;

  // Used to handle return/space key events and simulate clicks. Returns true
  // if the event is handled.
  bool HandleKeyboardActivation(Event& event);

  static const AtomicString& EventNameForAttributeName(
      const QualifiedName& attr_name);

  bool SupportsFocus() const override;
  bool IsDisabledFormControl() const override;
  bool MatchesEnabledPseudoClass() const override;
  bool MatchesReadOnlyPseudoClass() const override;
  bool MatchesReadWritePseudoClass() const override;
  bool MatchesValidityPseudoClasses() const override;
  bool willValidate() const override;
  bool IsValidElement() override;

  static const AtomicString& EventParameterName();

  virtual String AltText() const { return String(); }

  int offsetLeftForBinding();
  int offsetTopForBinding();
  int offsetWidthForBinding();
  int offsetHeightForBinding();

  Element* unclosedOffsetParent();

  ElementInternals* attachInternals(ExceptionState& exception_state);
  virtual FormAssociated* ToFormAssociatedOrNull() { return nullptr; }
  bool IsFormAssociatedCustomElement() const;

  static void AdjustCandidateDirectionalityForSlot(
      HeapHashSet<Member<Node>> candidate_set);
  void UpdateDescendantHasDirAutoAttribute(bool has_dir_auto);
  void UpdateDirectionalityAndDescendant(TextDirection direction);
  void UpdateDescendantDirectionality(TextDirection direction);
  void AdjustDirectionalityIfNeededAfterShadowRootChanged();
  void BeginParsingChildren() override;

 protected:
  enum AllowPercentage { kDontAllowPercentageValues, kAllowPercentageValues };
  enum AllowZero { kDontAllowZeroValues, kAllowZeroValues };
  void AddHTMLLengthToStyle(MutableCSSPropertyValueSet*,
                            CSSPropertyID,
                            const String& value,
                            AllowPercentage = kAllowPercentageValues,
                            AllowZero = kAllowZeroValues);
  void AddHTMLColorToStyle(MutableCSSPropertyValueSet*,
                           CSSPropertyID,
                           const String& color);

  // This corresponds to:
  //  'map to the aspect-ratio property (using dimension rules)'
  // described by:
  // https://html.spec.whatwg.org/multipage/rendering.html#map-to-the-aspect-ratio-property-(using-dimension-rules)
  void ApplyAspectRatioToStyle(const AtomicString& width,
                               const AtomicString& height,
                               MutableCSSPropertyValueSet*);
  // This corresponds to:
  //  'map to the aspect-ratio property'
  // described by:
  // https://html.spec.whatwg.org/multipage/rendering.html#map-to-the-aspect-ratio-property
  void ApplyIntegerAspectRatioToStyle(const AtomicString& width,
                                      const AtomicString& height,
                                      MutableCSSPropertyValueSet*);
  void ApplyAlignmentAttributeToStyle(const AtomicString&,
                                      MutableCSSPropertyValueSet*);
  void ApplyBorderAttributeToStyle(const AtomicString&,
                                   MutableCSSPropertyValueSet*);

  void AttributeChanged(const AttributeModificationParams&) override;
  void ParseAttribute(const AttributeModificationParams&) override;
  static bool ParseColorWithLegacyRules(const String& attribute_value,
                                        Color& parsed_color);
  bool IsPresentationAttribute(const QualifiedName&) const override;
  void CollectStyleForPresentationAttribute(
      const QualifiedName&,
      const AtomicString&,
      MutableCSSPropertyValueSet*) override;
  unsigned ParseBorderWidthAttribute(const AtomicString&) const;

  void ChildrenChanged(const ChildrenChange&) override;
  bool CalculateAndAdjustAutoDirectionality(Node* stay_within);

  InsertionNotificationRequest InsertedInto(ContainerNode&) override;
  void RemovedFrom(ContainerNode& insertion_point) override;
  void DidMoveToNewDocument(Document& old_document) override;
  void FinishParsingChildren() override;

 private:
  String DebugNodeName() const final;
  String nodeName() const final;

  bool IsHTMLElement() const =
      delete;  // This will catch anyone doing an unnecessary check.
  bool IsStyledElement() const =
      delete;  // This will catch anyone doing an unnecessary check.

  void MapLanguageAttributeToLocale(const AtomicString&,
                                    MutableCSSPropertyValueSet*);
  void ApplyAspectRatioToStyle(double width,
                               double height,
                               MutableCSSPropertyValueSet* style);

  DocumentFragment* TextToFragment(const String&, ExceptionState&);

  void AdjustDirectionalityIfNeededAfterChildAttributeChanged(Element* child);
  void AdjustDirectionalityIfNeededAfterChildrenChanged(
      const ChildrenChange& change);

  template <typename Traversal>
  absl::optional<TextDirection> ResolveAutoDirectionality(
      bool& is_deferred,
      Node* stay_within) const;

  TranslateAttributeMode GetTranslateAttributeMode() const;

  void HandleKeypressEvent(KeyboardEvent&);

  static AttributeTriggers* TriggersForAttributeName(
      const QualifiedName& attr_name);

  void OnDirAttrChanged(const AttributeModificationParams&);
  void OnFocusgroupAttrChanged(const AttributeModificationParams&);
  void OnFormAttrChanged(const AttributeModificationParams&);
  void OnLangAttrChanged(const AttributeModificationParams&);
  void OnNonceAttrChanged(const AttributeModificationParams&);
  void OnTabIndexAttrChanged(const AttributeModificationParams&);
  void OnXMLLangAttrChanged(const AttributeModificationParams&);
};

template <typename T>
bool IsElementOfType(const HTMLElement&);
template <>
inline bool IsElementOfType<const HTMLElement>(const HTMLElement&) {
  return true;
}
template <>
inline bool IsElementOfType<const HTMLElement>(const Node& node) {
  return IsA<HTMLElement>(node);
}
template <>
struct DowncastTraits<HTMLElement> {
  static bool AllowFrom(const Node& node) { return node.IsHTMLElement(); }
};

inline HTMLElement::HTMLElement(const QualifiedName& tag_name,
                                Document& document,
                                ConstructionType type = kCreateHTMLElement)
    : Element(tag_name, &document, type) {
  DCHECK(!tag_name.LocalName().IsNull());
}

inline bool Node::HasTagName(const HTMLQualifiedName& name) const {
  auto* html_element = DynamicTo<HTMLElement>(this);
  return html_element && html_element->HasTagName(name);
}

// Functor used to match HTMLElements with a specific HTML tag when using the
// ElementTraversal API.
class HasHTMLTagName {
  STACK_ALLOCATED();

 public:
  explicit HasHTMLTagName(const HTMLQualifiedName& tag_name)
      : tag_name_(tag_name) {}
  bool operator()(const HTMLElement& element) const {
    return element.HasTagName(tag_name_);
  }

 private:
  const HTMLQualifiedName& tag_name_;
};

}  // namespace blink

#include "third_party/blink/renderer/core/html_element_type_helpers.h"

#endif  // THIRD_PARTY_BLINK_RENDERER_CORE_HTML_HTML_ELEMENT_H_
