/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2010 Apple Inc. All rights
 * reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
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

#include "core/html/forms/HTMLTextAreaElement.h"

#include "bindings/core/v8/ExceptionState.h"
#include "core/CSSValueKeywords.h"
#include "core/css/StyleChangeReason.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ShadowRoot.h"
#include "core/dom/Text.h"
#include "core/dom/events/Event.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/iterators/TextIterator.h"
#include "core/events/BeforeTextInsertedEvent.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/UseCounter.h"
#include "core/html/forms/FormController.h"
#include "core/html/forms/FormData.h"
#include "core/html/forms/TextControlInnerElements.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/html_names.h"
#include "core/layout/LayoutTextControlMultiLine.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "platform/text/PlatformLocale.h"
#include "platform/wtf/StdLibExtras.h"
#include "platform/wtf/text/StringBuilder.h"

namespace blink {

using namespace HTMLNames;

static const unsigned kDefaultRows = 2;
static const unsigned kDefaultCols = 20;

static inline unsigned ComputeLengthForAPIValue(const String& text) {
  unsigned length = text.length();
  unsigned crlf_count = 0;
  for (unsigned i = 0; i < length; ++i) {
    if (text[i] == '\r' && i + 1 < length && text[i + 1] == '\n')
      crlf_count++;
  }
  return text.length() - crlf_count;
}

HTMLTextAreaElement::HTMLTextAreaElement(Document& document)
    : TextControlElement(textareaTag, document),
      rows_(kDefaultRows),
      cols_(kDefaultCols),
      wrap_(kSoftWrap),
      is_dirty_(false),
      is_placeholder_visible_(false) {}

HTMLTextAreaElement* HTMLTextAreaElement::Create(Document& document) {
  HTMLTextAreaElement* text_area = new HTMLTextAreaElement(document);
  text_area->EnsureUserAgentShadowRootV1();
  return text_area;
}

void HTMLTextAreaElement::DidAddUserAgentShadowRoot(ShadowRoot& root) {
  root.AppendChild(CreateInnerEditorElement());
}

const AtomicString& HTMLTextAreaElement::FormControlType() const {
  DEFINE_STATIC_LOCAL(const AtomicString, textarea, ("textarea"));
  return textarea;
}

FormControlState HTMLTextAreaElement::SaveFormControlState() const {
  return is_dirty_ ? FormControlState(value()) : FormControlState();
}

void HTMLTextAreaElement::RestoreFormControlState(
    const FormControlState& state) {
  setValue(state[0]);
}

void HTMLTextAreaElement::ChildrenChanged(const ChildrenChange& change) {
  HTMLElement::ChildrenChanged(change);
  SetLastChangeWasNotUserEdit();
  if (is_dirty_)
    SetInnerEditorValue(value());
  else
    SetNonDirtyValue(defaultValue());
}

bool HTMLTextAreaElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  if (name == alignAttr) {
    // Don't map 'align' attribute.  This matches what Firefox, Opera and IE do.
    // See http://bugs.webkit.org/show_bug.cgi?id=7075
    return false;
  }

  if (name == wrapAttr)
    return true;
  return TextControlElement::IsPresentationAttribute(name);
}

void HTMLTextAreaElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == wrapAttr) {
    if (ShouldWrapText()) {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyWhiteSpace,
                                              CSSValuePreWrap);
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyWordWrap,
                                              CSSValueBreakWord);
    } else {
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyWhiteSpace,
                                              CSSValuePre);
      AddPropertyToPresentationAttributeStyle(style, CSSPropertyWordWrap,
                                              CSSValueNormal);
    }
  } else {
    TextControlElement::CollectStyleForPresentationAttribute(name, value,
                                                             style);
  }
}

void HTMLTextAreaElement::ParseAttribute(
    const AttributeModificationParams& params) {
  const QualifiedName& name = params.name;
  const AtomicString& value = params.new_value;
  if (name == rowsAttr) {
    unsigned rows = 0;
    if (value.IsEmpty() || !ParseHTMLNonNegativeInteger(value, rows) ||
        rows <= 0 || rows > 0x7fffffffu)
      rows = kDefaultRows;
    if (rows_ != rows) {
      rows_ = rows;
      if (GetLayoutObject()) {
        GetLayoutObject()
            ->SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
                LayoutInvalidationReason::kAttributeChanged);
      }
    }
  } else if (name == colsAttr) {
    unsigned cols = 0;
    if (value.IsEmpty() || !ParseHTMLNonNegativeInteger(value, cols) ||
        cols <= 0 || cols > 0x7fffffffu)
      cols = kDefaultCols;
    if (cols_ != cols) {
      cols_ = cols;
      if (LayoutObject* layout_object = GetLayoutObject()) {
        layout_object
            ->SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
                LayoutInvalidationReason::kAttributeChanged);
      }
    }
  } else if (name == wrapAttr) {
    // The virtual/physical values were a Netscape extension of HTML 3.0, now
    // deprecated.  The soft/hard /off values are a recommendation for HTML 4
    // extension by IE and NS 4.
    WrapMethod wrap;
    if (DeprecatedEqualIgnoringCase(value, "physical") ||
        DeprecatedEqualIgnoringCase(value, "hard") ||
        DeprecatedEqualIgnoringCase(value, "on"))
      wrap = kHardWrap;
    else if (DeprecatedEqualIgnoringCase(value, "off"))
      wrap = kNoWrap;
    else
      wrap = kSoftWrap;
    if (wrap != wrap_) {
      wrap_ = wrap;
      if (LayoutObject* layout_object = GetLayoutObject()) {
        layout_object
            ->SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
                LayoutInvalidationReason::kAttributeChanged);
      }
    }
  } else if (name == accesskeyAttr) {
    // ignore for the moment
  } else if (name == maxlengthAttr) {
    UseCounter::Count(GetDocument(), WebFeature::kTextAreaMaxLength);
    SetNeedsValidityCheck();
  } else if (name == minlengthAttr) {
    UseCounter::Count(GetDocument(), WebFeature::kTextAreaMinLength);
    SetNeedsValidityCheck();
  } else {
    TextControlElement::ParseAttribute(params);
  }
}

LayoutObject* HTMLTextAreaElement::CreateLayoutObject(const ComputedStyle&) {
  return new LayoutTextControlMultiLine(this);
}

void HTMLTextAreaElement::AppendToFormData(FormData& form_data) {
  if (GetName().IsEmpty())
    return;

  GetDocument().UpdateStyleAndLayout();

  const String& text =
      (wrap_ == kHardWrap) ? ValueWithHardLineBreaks() : value();
  form_data.append(GetName(), text);

  const AtomicString& dirname_attr_value = FastGetAttribute(dirnameAttr);
  if (!dirname_attr_value.IsNull())
    form_data.append(dirname_attr_value, DirectionForFormData());
}

void HTMLTextAreaElement::ResetImpl() {
  SetNonDirtyValue(defaultValue());
}

bool HTMLTextAreaElement::HasCustomFocusLogic() const {
  return true;
}

bool HTMLTextAreaElement::IsKeyboardFocusable() const {
  // If a given text area can be focused at all, then it will always be keyboard
  // focusable.
  return IsFocusable();
}

bool HTMLTextAreaElement::ShouldShowFocusRingOnMouseFocus() const {
  return true;
}

void HTMLTextAreaElement::UpdateFocusAppearanceWithOptions(
    SelectionBehaviorOnFocus selection_behavior,
    const FocusOptions& options) {
  switch (selection_behavior) {
    case SelectionBehaviorOnFocus::kReset:  // Fallthrough.
    case SelectionBehaviorOnFocus::kRestore:
      RestoreCachedSelection();
      break;
    case SelectionBehaviorOnFocus::kNone:
      return;
  }
  if (!options.preventScroll()) {
    if (GetDocument().GetFrame())
      GetDocument().GetFrame()->Selection().RevealSelection();
  }
}

void HTMLTextAreaElement::DefaultEventHandler(Event* event) {
  if (GetLayoutObject() && (event->IsMouseEvent() || event->IsDragEvent() ||
                            event->HasInterface(EventNames::WheelEvent) ||
                            event->type() == EventTypeNames::blur))
    ForwardEvent(event);
  else if (GetLayoutObject() && event->IsBeforeTextInsertedEvent())
    HandleBeforeTextInsertedEvent(static_cast<BeforeTextInsertedEvent*>(event));

  TextControlElement::DefaultEventHandler(event);
}

void HTMLTextAreaElement::SubtreeHasChanged() {
#if DCHECK_IS_ON()
  // The innerEditor should have either Text nodes or a placeholder break
  // element. If we see other nodes, it's a bug in editing code and we should
  // fix it.
  Element* inner_editor = InnerEditorElement();
  for (Node& node : NodeTraversal::DescendantsOf(*inner_editor)) {
    if (node.IsTextNode())
      continue;
    DCHECK(IsHTMLBRElement(node));
    DCHECK_EQ(&node, inner_editor->lastChild());
  }
#endif
  AddPlaceholderBreakElementIfNecessary();
  SetValueBeforeFirstUserEditIfNotSet();
  UpdateValue();
  CheckIfValueWasReverted(value());
  SetNeedsValidityCheck();
  SetAutofilled(false);
  UpdatePlaceholderVisibility();

  if (!IsFocused())
    return;

  // When typing in a textarea, childrenChanged is not called, so we need to
  // force the directionality check.
  CalculateAndAdjustDirectionality();

  DCHECK(GetDocument().IsActive());
  GetDocument().GetPage()->GetChromeClient().DidChangeValueInTextField(*this);
}

void HTMLTextAreaElement::HandleBeforeTextInsertedEvent(
    BeforeTextInsertedEvent* event) const {
  DCHECK(event);
  DCHECK(GetLayoutObject());
  int signed_max_length = maxLength();
  if (signed_max_length < 0)
    return;
  unsigned unsigned_max_length = static_cast<unsigned>(signed_max_length);

  const String& current_value = InnerEditorValue();
  unsigned current_length = ComputeLengthForAPIValue(current_value);
  if (current_length + ComputeLengthForAPIValue(event->GetText()) <
      unsigned_max_length)
    return;

  // selectionLength represents the selection length of this text field to be
  // removed by this insertion.
  // If the text field has no focus, we don't need to take account of the
  // selection length. The selection is the source of text drag-and-drop in
  // that case, and nothing in the text field will be removed.
  unsigned selection_length = 0;
  if (IsFocused()) {
    // TODO(editing-dev): Use of updateStyleAndLayoutIgnorePendingStylesheets
    // needs to be audited.  See http://crbug.com/590369 for more details.
    GetDocument().UpdateStyleAndLayoutIgnorePendingStylesheets();

    selection_length = ComputeLengthForAPIValue(
        GetDocument().GetFrame()->Selection().SelectedText());
  }
  DCHECK_GE(current_length, selection_length);
  unsigned base_length = current_length - selection_length;
  unsigned appendable_length =
      unsigned_max_length > base_length ? unsigned_max_length - base_length : 0;
  event->SetText(SanitizeUserInputValue(event->GetText(), appendable_length));
}

String HTMLTextAreaElement::SanitizeUserInputValue(const String& proposed_value,
                                                   unsigned max_length) {
  unsigned submission_length = 0;
  unsigned i = 0;
  for (; i < proposed_value.length(); ++i) {
    if (proposed_value[i] == '\r' && i + 1 < proposed_value.length() &&
        proposed_value[i + 1] == '\n')
      continue;
    ++submission_length;
    if (submission_length == max_length) {
      ++i;
      break;
    }
    if (submission_length > max_length)
      break;
  }
  if (i > 0 && U16_IS_LEAD(proposed_value[i - 1]))
    --i;
  return proposed_value.Left(i);
}

void HTMLTextAreaElement::UpdateValue() {
  value_ = InnerEditorValue();
  NotifyFormStateChanged();
  is_dirty_ = true;
  UpdatePlaceholderVisibility();
}

String HTMLTextAreaElement::value() const {
  return value_;
}

void HTMLTextAreaElement::setValue(const String& value,
                                   TextFieldEventBehavior event_behavior,
                                   TextControlSetValueSelection selection) {
  SetValueCommon(value, event_behavior, selection);
  is_dirty_ = true;
}

void HTMLTextAreaElement::SetNonDirtyValue(const String& value) {
  SetValueCommon(value, kDispatchNoEvent,
                 TextControlSetValueSelection::kSetSelectionToEnd);
  is_dirty_ = false;
}

void HTMLTextAreaElement::SetValueCommon(
    const String& new_value,
    TextFieldEventBehavior event_behavior,
    TextControlSetValueSelection selection) {
  // Code elsewhere normalizes line endings added by the user via the keyboard
  // or pasting.  We normalize line endings coming from JavaScript here.
  String normalized_value = new_value.IsNull() ? "" : new_value;
  normalized_value.Replace("\r\n", "\n");
  normalized_value.Replace('\r', '\n');

  // Clear the suggested value. Use the base class version to not trigger a view
  // update.
  TextControlElement::SetSuggestedValue(String());

  // Return early because we don't want to trigger other side effects when the
  // value isn't changing. This is interoperable.
  if (normalized_value == value())
    return;

  if (event_behavior != kDispatchNoEvent)
    SetValueBeforeFirstUserEditIfNotSet();
  value_ = normalized_value;
  SetInnerEditorValue(value_);
  if (event_behavior == kDispatchNoEvent)
    SetLastChangeWasNotUserEdit();
  else
    CheckIfValueWasReverted(value_);
  UpdatePlaceholderVisibility();
  SetNeedsStyleRecalc(
      kSubtreeStyleChange,
      StyleChangeReasonForTracing::Create(StyleChangeReason::kControlValue));
  SetNeedsValidityCheck();
  if (IsFinishedParsingChildren() &&
      selection == TextControlSetValueSelection::kSetSelectionToEnd) {
    // Set the caret to the end of the text value except for initialize.
    unsigned end_of_string = value_.length();
    SetSelectionRange(end_of_string, end_of_string);
  }

  NotifyFormStateChanged();
  switch (event_behavior) {
    case kDispatchChangeEvent:
      DispatchFormControlChangeEvent();
      break;

    case kDispatchInputAndChangeEvent:
      DispatchInputEvent();
      DispatchFormControlChangeEvent();
      break;

    case kDispatchNoEvent:
      break;
  }
}

String HTMLTextAreaElement::defaultValue() const {
  StringBuilder value;

  // Since there may be comments, ignore nodes other than text nodes.
  for (Node* n = firstChild(); n; n = n->nextSibling()) {
    if (n->IsTextNode())
      value.Append(ToText(n)->data());
  }

  return value.ToString();
}

void HTMLTextAreaElement::setDefaultValue(const String& default_value) {
  // To preserve comments, remove only the text nodes, then add a single text
  // node.
  HeapVector<Member<Node>> text_nodes;
  for (Node* n = firstChild(); n; n = n->nextSibling()) {
    if (n->IsTextNode())
      text_nodes.push_back(n);
  }
  for (const auto& text : text_nodes)
    RemoveChild(text.Get(), IGNORE_EXCEPTION_FOR_TESTING);

  // Normalize line endings.
  String value = default_value;
  value.Replace("\r\n", "\n");
  value.Replace('\r', '\n');

  InsertBefore(GetDocument().createTextNode(value), firstChild(),
               IGNORE_EXCEPTION_FOR_TESTING);

  if (!is_dirty_)
    SetNonDirtyValue(value);
}

void HTMLTextAreaElement::SetSuggestedValue(const String& value) {
  TextControlElement::SetSuggestedValue(value);
  SetNeedsStyleRecalc(
      kSubtreeStyleChange,
      StyleChangeReasonForTracing::Create(StyleChangeReason::kControlValue));
}

String HTMLTextAreaElement::validationMessage() const {
  if (!willValidate())
    return String();

  if (CustomError())
    return CustomValidationMessage();

  if (ValueMissing())
    return GetLocale().QueryString(WebLocalizedString::kValidationValueMissing);

  if (TooLong()) {
    return GetLocale().ValidationMessageTooLongText(value().length(),
                                                    maxLength());
  }

  if (TooShort()) {
    return GetLocale().ValidationMessageTooShortText(value().length(),
                                                     minLength());
  }

  return String();
}

bool HTMLTextAreaElement::ValueMissing() const {
  // We should not call value() for performance.
  return willValidate() && ValueMissing(nullptr);
}

bool HTMLTextAreaElement::ValueMissing(const String* value) const {
  return IsRequiredFormControl() && !IsDisabledOrReadOnly() &&
         (value ? *value : this->value()).IsEmpty();
}

bool HTMLTextAreaElement::TooLong() const {
  // We should not call value() for performance.
  return willValidate() && TooLong(nullptr, kCheckDirtyFlag);
}

bool HTMLTextAreaElement::TooShort() const {
  // We should not call value() for performance.
  return willValidate() && TooShort(nullptr, kCheckDirtyFlag);
}

bool HTMLTextAreaElement::TooLong(const String* value,
                                  NeedsToCheckDirtyFlag check) const {
  // Return false for the default value or value set by script even if it is
  // longer than maxLength.
  if (check == kCheckDirtyFlag && !LastChangeWasUserEdit())
    return false;

  int max = maxLength();
  if (max < 0)
    return false;
  unsigned len =
      value ? ComputeLengthForAPIValue(*value) : this->value().length();
  return len > static_cast<unsigned>(max);
}

bool HTMLTextAreaElement::TooShort(const String* value,
                                   NeedsToCheckDirtyFlag check) const {
  // Return false for the default value or value set by script even if it is
  // shorter than minLength.
  if (check == kCheckDirtyFlag && !LastChangeWasUserEdit())
    return false;

  int min = minLength();
  if (min <= 0)
    return false;
  // An empty string is excluded from minlength check.
  unsigned len =
      value ? ComputeLengthForAPIValue(*value) : this->value().length();
  return len > 0 && len < static_cast<unsigned>(min);
}

bool HTMLTextAreaElement::IsValidValue(const String& candidate) const {
  return !ValueMissing(&candidate) && !TooLong(&candidate, kIgnoreDirtyFlag) &&
         !TooShort(&candidate, kIgnoreDirtyFlag);
}

void HTMLTextAreaElement::AccessKeyAction(bool) {
  focus();
}

void HTMLTextAreaElement::setCols(unsigned cols) {
  SetUnsignedIntegralAttribute(colsAttr, cols ? cols : kDefaultCols,
                               kDefaultCols);
}

void HTMLTextAreaElement::setRows(unsigned rows) {
  SetUnsignedIntegralAttribute(rowsAttr, rows ? rows : kDefaultRows,
                               kDefaultRows);
}

bool HTMLTextAreaElement::MatchesReadOnlyPseudoClass() const {
  return IsReadOnly();
}

bool HTMLTextAreaElement::MatchesReadWritePseudoClass() const {
  return !IsReadOnly();
}

void HTMLTextAreaElement::SetPlaceholderVisibility(bool visible) {
  is_placeholder_visible_ = visible;
}

void HTMLTextAreaElement::UpdatePlaceholderText() {
  HTMLElement* placeholder = PlaceholderElement();
  const String placeholder_text = GetPlaceholderValue();
  if (placeholder_text.IsEmpty()) {
    if (placeholder)
      UserAgentShadowRoot()->RemoveChild(placeholder);
    return;
  }
  if (!placeholder) {
    HTMLDivElement* new_element = HTMLDivElement::Create(GetDocument());
    placeholder = new_element;
    placeholder->SetShadowPseudoId(AtomicString("-webkit-input-placeholder"));
    placeholder->setAttribute(idAttr, ShadowElementNames::Placeholder());
    placeholder->SetInlineStyleProperty(
        CSSPropertyDisplay,
        IsPlaceholderVisible() ? CSSValueBlock : CSSValueNone, true);
    UserAgentShadowRoot()->InsertBefore(placeholder, InnerEditorElement());
  }
  placeholder->setTextContent(placeholder_text);
}

String HTMLTextAreaElement::GetPlaceholderValue() const {
  return !SuggestedValue().IsEmpty() ? SuggestedValue()
                                     : FastGetAttribute(placeholderAttr);
}

bool HTMLTextAreaElement::IsInteractiveContent() const {
  return true;
}

bool HTMLTextAreaElement::SupportsAutofocus() const {
  return true;
}

const AtomicString& HTMLTextAreaElement::DefaultAutocapitalize() const {
  DEFINE_STATIC_LOCAL(const AtomicString, sentences, ("sentences"));
  return sentences;
}

void HTMLTextAreaElement::CopyNonAttributePropertiesFromElement(
    const Element& source) {
  const HTMLTextAreaElement& source_element =
      static_cast<const HTMLTextAreaElement&>(source);
  SetValueCommon(source_element.value(), kDispatchNoEvent,
                 TextControlSetValueSelection::kSetSelectionToEnd);
  is_dirty_ = source_element.is_dirty_;
  TextControlElement::CopyNonAttributePropertiesFromElement(source);
}

}  // namespace blink
