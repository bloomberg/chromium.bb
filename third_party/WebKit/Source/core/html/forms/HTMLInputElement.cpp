/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All
 * rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved.
 * (http://www.torchmobile.com/)
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
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

#include "core/html/forms/HTMLInputElement.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptEventListener.h"
#include "core/CSSPropertyNames.h"
#include "core/css/StyleChangeReason.h"
#include "core/dom/AXObjectCache.h"
#include "core/dom/Document.h"
#include "core/dom/IdTargetObserver.h"
#include "core/dom/ShadowRoot.h"
#include "core/dom/SyncReattachContext.h"
#include "core/dom/V0InsertionPoint.h"
#include "core/dom/events/ScopedEventQueue.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/spellcheck/SpellChecker.h"
#include "core/events/BeforeTextInsertedEvent.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/MouseEvent.h"
#include "core/frame/Deprecation.h"
#include "core/frame/LocalFrame.h"
#include "core/frame/LocalFrameView.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLImageLoader.h"
#include "core/html/forms/ColorChooser.h"
#include "core/html/forms/DateTimeChooser.h"
#include "core/html/forms/FileInputType.h"
#include "core/html/forms/FormController.h"
#include "core/html/forms/HTMLDataListElement.h"
#include "core/html/forms/HTMLDataListOptionsCollection.h"
#include "core/html/forms/HTMLFormElement.h"
#include "core/html/forms/HTMLOptionElement.h"
#include "core/html/forms/InputType.h"
#include "core/html/forms/SearchInputType.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html_names.h"
#include "core/input_type_names.h"
#include "core/layout/LayoutObject.h"
#include "core/page/ChromeClient.h"
#include "core/page/Page.h"
#include "platform/Language.h"
#include "platform/runtime_enabled_features.h"
#include "platform/text/PlatformLocale.h"
#include "platform/wtf/MathExtras.h"
#include "public/platform/TaskType.h"
#include "public/platform/WebScrollIntoViewParams.h"

namespace blink {

using ValueMode = InputType::ValueMode;
using namespace HTMLNames;

class ListAttributeTargetObserver : public IdTargetObserver {
 public:
  static ListAttributeTargetObserver* Create(const AtomicString& id,
                                             HTMLInputElement*);
  void Trace(blink::Visitor*) override;
  void IdTargetChanged() override;

 private:
  ListAttributeTargetObserver(const AtomicString& id, HTMLInputElement*);

  Member<HTMLInputElement> element_;
};

const int kDefaultSize = 20;

HTMLInputElement::HTMLInputElement(Document& document,
                                   const CreateElementFlags flags)
    : TextControlElement(inputTag, document),
      size_(kDefaultSize),
      has_dirty_value_(false),
      is_checked_(false),
      dirty_checkedness_(false),
      is_indeterminate_(false),
      is_activated_submit_(false),
      autocomplete_(kUninitialized),
      has_non_empty_list_(false),
      state_restored_(false),
      parsing_in_progress_(flags.IsCreatedByParser()),
      can_receive_dropped_files_(false),
      should_reveal_password_(false),
      needs_to_update_view_value_(true),
      is_placeholder_visible_(false),
      has_been_password_field_(false),
      // |input_type_| is lazily created when constructed by the parser to avoid
      // constructing unnecessarily a text InputType and its shadow subtree,
      // just to destroy them when the |type| attribute gets set by the parser
      // to something else than 'text'.
      input_type_(flags.IsCreatedByParser() ? nullptr
                                            : InputType::CreateText(*this)),
      input_type_view_(input_type_ ? input_type_->CreateView() : nullptr) {
  SetHasCustomStyleCallbacks();
}

HTMLInputElement* HTMLInputElement::Create(Document& document,
                                           const CreateElementFlags flags) {
  auto* input_element = new HTMLInputElement(document, flags);
  if (!flags.IsCreatedByParser()) {
    DCHECK(input_element->input_type_view_->NeedsShadowSubtree());
    input_element->CreateUserAgentShadowRoot();
    input_element->CreateShadowSubtree();
  }
  return input_element;
}

void HTMLInputElement::Trace(blink::Visitor* visitor) {
  visitor->Trace(input_type_);
  visitor->Trace(input_type_view_);
  visitor->Trace(list_attribute_target_observer_);
  visitor->Trace(image_loader_);
  TextControlElement::Trace(visitor);
}

bool HTMLInputElement::HasPendingActivity() const {
  return ImageLoader() && ImageLoader()->HasPendingActivity();
}

HTMLImageLoader& HTMLInputElement::EnsureImageLoader() {
  if (!image_loader_)
    image_loader_ = HTMLImageLoader::Create(this);
  return *image_loader_;
}

HTMLInputElement::~HTMLInputElement() = default;

const AtomicString& HTMLInputElement::GetName() const {
  return name_.IsNull() ? g_empty_atom : name_;
}

Vector<FileChooserFileInfo>
HTMLInputElement::FilesFromFileInputFormControlState(
    const FormControlState& state) {
  return FileInputType::FilesFromFormControlState(state);
}

bool HTMLInputElement::ShouldAutocomplete() const {
  if (autocomplete_ != kUninitialized)
    return autocomplete_ == kOn;
  return TextControlElement::ShouldAutocomplete();
}

bool HTMLInputElement::IsValidValue(const String& value) const {
  if (!input_type_->CanSetStringValue()) {
    NOTREACHED();
    return false;
  }
  return !input_type_->TypeMismatchFor(value) &&
         !input_type_->StepMismatch(value) &&
         !input_type_->RangeUnderflow(value) &&
         !input_type_->RangeOverflow(value) &&
         !TooLong(value, kIgnoreDirtyFlag) &&
         !TooShort(value, kIgnoreDirtyFlag) &&
         !input_type_->PatternMismatch(value) &&
         !input_type_->ValueMissing(value);
}

bool HTMLInputElement::TooLong() const {
  return willValidate() && TooLong(value(), kCheckDirtyFlag);
}

bool HTMLInputElement::TooShort() const {
  return willValidate() && TooShort(value(), kCheckDirtyFlag);
}

bool HTMLInputElement::TypeMismatch() const {
  return willValidate() && input_type_->TypeMismatch();
}

bool HTMLInputElement::ValueMissing() const {
  return willValidate() && input_type_->ValueMissing(value());
}

bool HTMLInputElement::HasBadInput() const {
  return willValidate() && input_type_view_->HasBadInput();
}

bool HTMLInputElement::PatternMismatch() const {
  return willValidate() && input_type_->PatternMismatch(value());
}

bool HTMLInputElement::TooLong(const String& value,
                               NeedsToCheckDirtyFlag check) const {
  return input_type_->TooLong(value, check);
}

bool HTMLInputElement::TooShort(const String& value,
                                NeedsToCheckDirtyFlag check) const {
  return input_type_->TooShort(value, check);
}

bool HTMLInputElement::RangeUnderflow() const {
  return willValidate() && input_type_->RangeUnderflow(value());
}

bool HTMLInputElement::RangeOverflow() const {
  return willValidate() && input_type_->RangeOverflow(value());
}

String HTMLInputElement::validationMessage() const {
  if (!willValidate())
    return String();

  if (CustomError())
    return CustomValidationMessage();

  return input_type_->ValidationMessage(*input_type_view_).first;
}

String HTMLInputElement::ValidationSubMessage() const {
  if (!willValidate() || CustomError())
    return String();
  return input_type_->ValidationMessage(*input_type_view_).second;
}

double HTMLInputElement::Minimum() const {
  return input_type_->Minimum();
}

double HTMLInputElement::Maximum() const {
  return input_type_->Maximum();
}

bool HTMLInputElement::StepMismatch() const {
  return willValidate() && input_type_->StepMismatch(value());
}

bool HTMLInputElement::GetAllowedValueStep(Decimal* step) const {
  return input_type_->GetAllowedValueStep(step);
}

StepRange HTMLInputElement::CreateStepRange(
    AnyStepHandling any_step_handling) const {
  return input_type_->CreateStepRange(any_step_handling);
}

Decimal HTMLInputElement::FindClosestTickMarkValue(const Decimal& value) {
  return input_type_->FindClosestTickMarkValue(value);
}

void HTMLInputElement::stepUp(int n, ExceptionState& exception_state) {
  input_type_->StepUp(n, exception_state);
}

void HTMLInputElement::stepDown(int n, ExceptionState& exception_state) {
  input_type_->StepUp(-1.0 * n, exception_state);
}

void HTMLInputElement::blur() {
  input_type_view_->Blur();
}

void HTMLInputElement::DefaultBlur() {
  TextControlElement::blur();
}

bool HTMLInputElement::HasCustomFocusLogic() const {
  return input_type_view_->HasCustomFocusLogic();
}

bool HTMLInputElement::IsKeyboardFocusable() const {
  return input_type_->IsKeyboardFocusable();
}

bool HTMLInputElement::ShouldShowFocusRingOnMouseFocus() const {
  return input_type_->ShouldShowFocusRingOnMouseFocus();
}

void HTMLInputElement::UpdateFocusAppearanceWithOptions(
    SelectionBehaviorOnFocus selection_behavior,
    const FocusOptions& options) {
  if (IsTextField()) {
    switch (selection_behavior) {
      case SelectionBehaviorOnFocus::kReset:
        select();
        break;
      case SelectionBehaviorOnFocus::kRestore:
        RestoreCachedSelection();
        break;
      case SelectionBehaviorOnFocus::kNone:
        return;
    }
    // TODO(tkent): scrollRectToVisible is a workaround of a bug of
    // FrameSelection::revealSelection().  It doesn't scroll correctly in a
    // case of RangeSelection. crbug.com/443061.
    GetDocument().EnsurePaintLocationDataValidForNode(this);
    if (!options.preventScroll()) {
      if (GetLayoutObject()) {
        GetLayoutObject()->ScrollRectToVisible(BoundingBoxForScrollIntoView(),
                                               WebScrollIntoViewParams());
      }
      if (GetDocument().GetFrame())
        GetDocument().GetFrame()->Selection().RevealSelection();
    }
  } else {
    TextControlElement::UpdateFocusAppearanceWithOptions(selection_behavior,
                                                         options);
  }
}

void HTMLInputElement::EndEditing() {
  DCHECK(GetDocument().IsActive());
  if (!GetDocument().IsActive())
    return;

  if (!IsTextField())
    return;

  LocalFrame* frame = GetDocument().GetFrame();
  frame->GetSpellChecker().DidEndEditingOnTextField(this);
  frame->GetPage()->GetChromeClient().DidEndEditingOnTextField(*this);
}

void HTMLInputElement::HandleFocusEvent(Element* old_focused_element,
                                        WebFocusType type) {
  input_type_->EnableSecureTextInput();
}

void HTMLInputElement::DispatchFocusInEvent(
    const AtomicString& event_type,
    Element* old_focused_element,
    WebFocusType type,
    InputDeviceCapabilities* source_capabilities) {
  if (event_type == EventTypeNames::DOMFocusIn)
    input_type_view_->HandleFocusInEvent(old_focused_element, type);
  HTMLFormControlElementWithState::DispatchFocusInEvent(
      event_type, old_focused_element, type, source_capabilities);
}

void HTMLInputElement::HandleBlurEvent() {
  input_type_->DisableSecureTextInput();
  input_type_view_->HandleBlurEvent();
}

void HTMLInputElement::setType(const AtomicString& type) {
  setAttribute(typeAttr, type);
}

void HTMLInputElement::InitializeTypeInParsing() {
  DCHECK(parsing_in_progress_);
  DCHECK(!input_type_);
  DCHECK(!input_type_view_);

  const AtomicString& new_type_name =
      InputType::NormalizeTypeName(FastGetAttribute(typeAttr));
  input_type_ = InputType::Create(*this, new_type_name);
  input_type_view_ = input_type_->CreateView();
  String default_value = FastGetAttribute(valueAttr);
  if (input_type_->GetValueMode() == ValueMode::kValue)
    non_attribute_value_ = SanitizeValue(default_value);
  has_been_password_field_ |= new_type_name == InputTypeNames::password;

  if (input_type_view_->NeedsShadowSubtree()) {
    CreateUserAgentShadowRoot();
    CreateShadowSubtree();
  }

  SetNeedsWillValidateCheck();

  if (!default_value.IsNull())
    input_type_->WarnIfValueIsInvalid(default_value);

  input_type_view_->UpdateView();
}

void HTMLInputElement::UpdateType() {
  DCHECK(input_type_);
  DCHECK(input_type_view_);

  const AtomicString& new_type_name =
      InputType::NormalizeTypeName(FastGetAttribute(typeAttr));
  if (input_type_->FormControlType() == new_type_name)
    return;

  InputType* new_type = InputType::Create(*this, new_type_name);
  RemoveFromRadioButtonGroup();

  ValueMode old_value_mode = input_type_->GetValueMode();
  bool did_respect_height_and_width =
      input_type_->ShouldRespectHeightAndWidthAttributes();
  bool could_be_successful_submit_button = CanBeSuccessfulSubmitButton();

  input_type_view_->DestroyShadowSubtree();
  DropInnerEditorElement();
  LazyReattachIfAttached();

  if (input_type_->SupportsRequired() != new_type->SupportsRequired() &&
      IsRequired()) {
    PseudoStateChanged(CSSSelector::kPseudoRequired);
    PseudoStateChanged(CSSSelector::kPseudoOptional);
  }
  if (input_type_->SupportsReadOnly() != new_type->SupportsReadOnly()) {
    PseudoStateChanged(CSSSelector::kPseudoReadOnly);
    PseudoStateChanged(CSSSelector::kPseudoReadWrite);
  }
  if (input_type_->IsCheckable() != new_type->IsCheckable()) {
    PseudoStateChanged(CSSSelector::kPseudoChecked);
  }
  PseudoStateChanged(CSSSelector::kPseudoIndeterminate);
  if (input_type_->IsSteppable() || new_type->IsSteppable()) {
    PseudoStateChanged(CSSSelector::kPseudoInRange);
    PseudoStateChanged(CSSSelector::kPseudoOutOfRange);
  }

  bool placeholder_changed =
      input_type_->SupportsPlaceholder() != new_type->SupportsPlaceholder();

  has_been_password_field_ |= new_type_name == InputTypeNames::password;

  input_type_ = new_type;
  input_type_view_ = input_type_->CreateView();
  if (input_type_view_->NeedsShadowSubtree()) {
    EnsureUserAgentShadowRoot();
    CreateShadowSubtree();
  }

  SetNeedsWillValidateCheck();

  if (placeholder_changed) {
    // We need to update the UA shadow and then the placeholder visibility flag
    // here. Otherwise it would happen as part of attaching the layout tree
    // which would be too late in order to make style invalidation work for
    // the upcoming frame.
    UpdatePlaceholderText();
    UpdatePlaceholderVisibility();
    PseudoStateChanged(CSSSelector::kPseudoPlaceholderShown);
  }

  ValueMode new_value_mode = input_type_->GetValueMode();

  // https://html.spec.whatwg.org/multipage/forms.html#input-type-change
  //
  // 1. If the previous state of the element's type attribute put the value IDL
  // attribute in the value mode, and the element's value is not the empty
  // string, and the new state of the element's type attribute puts the value
  // IDL attribute in either the default mode or the default/on mode, then set
  // the element's value content attribute to the element's value.
  if (old_value_mode == ValueMode::kValue &&
      (new_value_mode == ValueMode::kDefault ||
       new_value_mode == ValueMode::kDefaultOn)) {
    if (HasDirtyValue())
      setAttribute(valueAttr, AtomicString(non_attribute_value_));
    non_attribute_value_ = String();
    has_dirty_value_ = false;
  }
  // 2. Otherwise, if the previous state of the element's type attribute put the
  // value IDL attribute in any mode other than the value mode, and the new
  // state of the element's type attribute puts the value IDL attribute in the
  // value mode, then set the value of the element to the value of the value
  // content attribute, if there is one, or the empty string otherwise, and then
  // set the control's dirty value flag to false.
  else if (old_value_mode != ValueMode::kValue &&
           new_value_mode == ValueMode::kValue) {
    AtomicString value_string = FastGetAttribute(valueAttr);
    input_type_->WarnIfValueIsInvalid(value_string);
    non_attribute_value_ = SanitizeValue(value_string);
    has_dirty_value_ = false;
  }
  // 3. Otherwise, if the previous state of the element's type attribute put the
  // value IDL attribute in any mode other than the filename mode, and the new
  // state of the element's type attribute puts the value IDL attribute in the
  // filename mode, then set the value of the element to the empty string.
  else if (old_value_mode != ValueMode::kFilename &&
           new_value_mode == ValueMode::kFilename) {
    non_attribute_value_ = String();
    has_dirty_value_ = false;

  } else {
    // ValueMode wasn't changed, or kDefault <-> kDefaultOn.
    if (!HasDirtyValue()) {
      String default_value = FastGetAttribute(valueAttr);
      if (!default_value.IsNull())
        input_type_->WarnIfValueIsInvalid(default_value);
    }

    if (new_value_mode == ValueMode::kValue) {
      String new_value = SanitizeValue(non_attribute_value_);
      if (!EqualIgnoringNullity(new_value, non_attribute_value_)) {
        if (HasDirtyValue())
          setValue(new_value);
        else
          SetNonDirtyValue(new_value);
      }
    }
  }

  needs_to_update_view_value_ = true;
  input_type_view_->UpdateView();

  if (did_respect_height_and_width !=
      input_type_->ShouldRespectHeightAndWidthAttributes()) {
    DCHECK(GetElementData());
    AttributeCollection attributes = AttributesWithoutUpdate();
    if (const Attribute* height = attributes.Find(heightAttr)) {
      TextControlElement::AttributeChanged(AttributeModificationParams(
          heightAttr, height->Value(), height->Value(),
          AttributeModificationReason::kDirectly));
    }
    if (const Attribute* width = attributes.Find(widthAttr)) {
      TextControlElement::AttributeChanged(
          AttributeModificationParams(widthAttr, width->Value(), width->Value(),
                                      AttributeModificationReason::kDirectly));
    }
    if (const Attribute* align = attributes.Find(alignAttr)) {
      TextControlElement::AttributeChanged(
          AttributeModificationParams(alignAttr, align->Value(), align->Value(),
                                      AttributeModificationReason::kDirectly));
    }
  }

  // UA Shadow tree was recreated. We need to set selection again. We do it
  // later in order to avoid force layout.
  if (GetDocument().FocusedElement() == this)
    GetDocument().UpdateFocusAppearanceLater();

  // TODO(tkent): Should we dispatch a change event?
  ClearValueBeforeFirstUserEdit();

  AddToRadioButtonGroup();

  SetNeedsValidityCheck();
  if ((could_be_successful_submit_button || CanBeSuccessfulSubmitButton()) &&
      formOwner() && isConnected())
    formOwner()->InvalidateDefaultButtonStyle();
  NotifyFormStateChanged();
}

void HTMLInputElement::SubtreeHasChanged() {
  input_type_view_->SubtreeHasChanged();
  // When typing in an input field, childrenChanged is not called, so we need to
  // force the directionality check.
  CalculateAndAdjustDirectionality();
}

const AtomicString& HTMLInputElement::FormControlType() const {
  return input_type_->FormControlType();
}

bool HTMLInputElement::ShouldSaveAndRestoreFormControlState() const {
  if (!input_type_->ShouldSaveAndRestoreFormControlState())
    return false;
  return TextControlElement::ShouldSaveAndRestoreFormControlState();
}

FormControlState HTMLInputElement::SaveFormControlState() const {
  return input_type_view_->SaveFormControlState();
}

void HTMLInputElement::RestoreFormControlState(const FormControlState& state) {
  input_type_view_->RestoreFormControlState(state);
  state_restored_ = true;
}

bool HTMLInputElement::CanStartSelection() const {
  if (!IsTextField())
    return false;
  return TextControlElement::CanStartSelection();
}

unsigned HTMLInputElement::selectionStartForBinding(
    bool& is_null,
    ExceptionState& exception_state) const {
  if (!input_type_->SupportsSelectionAPI()) {
    is_null = true;
    return 0;
  }
  return TextControlElement::selectionStart();
}

unsigned HTMLInputElement::selectionEndForBinding(
    bool& is_null,
    ExceptionState& exception_state) const {
  if (!input_type_->SupportsSelectionAPI()) {
    is_null = true;
    return 0;
  }
  return TextControlElement::selectionEnd();
}

String HTMLInputElement::selectionDirectionForBinding(
    ExceptionState& exception_state) const {
  if (!input_type_->SupportsSelectionAPI()) {
    return String();
  }
  return TextControlElement::selectionDirection();
}

void HTMLInputElement::setSelectionStartForBinding(
    unsigned start,
    bool is_null,
    ExceptionState& exception_state) {
  if (!input_type_->SupportsSelectionAPI()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "The input element's type ('" +
                                          input_type_->FormControlType() +
                                          "') does not support selection.");
    return;
  }
  TextControlElement::setSelectionStart(start);
}

void HTMLInputElement::setSelectionEndForBinding(
    unsigned end,
    bool is_null,
    ExceptionState& exception_state) {
  if (!input_type_->SupportsSelectionAPI()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "The input element's type ('" +
                                          input_type_->FormControlType() +
                                          "') does not support selection.");
    return;
  }
  TextControlElement::setSelectionEnd(end);
}

void HTMLInputElement::setSelectionDirectionForBinding(
    const String& direction,
    ExceptionState& exception_state) {
  if (!input_type_->SupportsSelectionAPI()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "The input element's type ('" +
                                          input_type_->FormControlType() +
                                          "') does not support selection.");
    return;
  }
  TextControlElement::setSelectionDirection(direction);
}

void HTMLInputElement::setSelectionRangeForBinding(
    unsigned start,
    unsigned end,
    ExceptionState& exception_state) {
  if (!input_type_->SupportsSelectionAPI()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "The input element's type ('" +
                                          input_type_->FormControlType() +
                                          "') does not support selection.");
    return;
  }
  TextControlElement::setSelectionRangeForBinding(start, end);
}

void HTMLInputElement::setSelectionRangeForBinding(
    unsigned start,
    unsigned end,
    const String& direction,
    ExceptionState& exception_state) {
  if (!input_type_->SupportsSelectionAPI()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "The input element's type ('" +
                                          input_type_->FormControlType() +
                                          "') does not support selection.");
    return;
  }
  TextControlElement::setSelectionRangeForBinding(start, end, direction);
}

void HTMLInputElement::AccessKeyAction(bool send_mouse_events) {
  input_type_view_->AccessKeyAction(send_mouse_events);
}

bool HTMLInputElement::IsPresentationAttribute(
    const QualifiedName& name) const {
  // FIXME: Remove type check.
  if (name == vspaceAttr || name == hspaceAttr || name == alignAttr ||
      name == widthAttr || name == heightAttr ||
      (name == borderAttr && type() == InputTypeNames::image))
    return true;
  return TextControlElement::IsPresentationAttribute(name);
}

void HTMLInputElement::CollectStyleForPresentationAttribute(
    const QualifiedName& name,
    const AtomicString& value,
    MutableCSSPropertyValueSet* style) {
  if (name == vspaceAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyMarginTop, value);
    AddHTMLLengthToStyle(style, CSSPropertyMarginBottom, value);
  } else if (name == hspaceAttr) {
    AddHTMLLengthToStyle(style, CSSPropertyMarginLeft, value);
    AddHTMLLengthToStyle(style, CSSPropertyMarginRight, value);
  } else if (name == alignAttr) {
    if (input_type_->ShouldRespectAlignAttribute())
      ApplyAlignmentAttributeToStyle(value, style);
  } else if (name == widthAttr) {
    if (input_type_->ShouldRespectHeightAndWidthAttributes())
      AddHTMLLengthToStyle(style, CSSPropertyWidth, value);
  } else if (name == heightAttr) {
    if (input_type_->ShouldRespectHeightAndWidthAttributes())
      AddHTMLLengthToStyle(style, CSSPropertyHeight, value);
  } else if (name == borderAttr &&
             type() == InputTypeNames::image) {  // FIXME: Remove type check.
    ApplyBorderAttributeToStyle(value, style);
  } else {
    TextControlElement::CollectStyleForPresentationAttribute(name, value,
                                                             style);
  }
}

void HTMLInputElement::ParseAttribute(
    const AttributeModificationParams& params) {
  DCHECK(input_type_);
  DCHECK(input_type_view_);
  const QualifiedName& name = params.name;
  const AtomicString& value = params.new_value;

  if (name == nameAttr) {
    RemoveFromRadioButtonGroup();
    name_ = value;
    AddToRadioButtonGroup();
    TextControlElement::ParseAttribute(params);
  } else if (name == autocompleteAttr) {
    if (DeprecatedEqualIgnoringCase(value, "off")) {
      autocomplete_ = kOff;
    } else {
      if (value.IsEmpty())
        autocomplete_ = kUninitialized;
      else
        autocomplete_ = kOn;
    }
  } else if (name == typeAttr) {
    UpdateType();
  } else if (name == valueAttr) {
    // We only need to setChanged if the form is looking at the default value
    // right now.
    if (!HasDirtyValue()) {
      if (input_type_->GetValueMode() == ValueMode::kValue)
        non_attribute_value_ = SanitizeValue(value);
      UpdatePlaceholderVisibility();
      SetNeedsStyleRecalc(
          kSubtreeStyleChange,
          StyleChangeReasonForTracing::FromAttribute(valueAttr));
    }
    needs_to_update_view_value_ = true;
    SetNeedsValidityCheck();
    input_type_->WarnIfValueIsInvalidAndElementIsVisible(value);
    input_type_->InRangeChanged();
    input_type_view_->ValueAttributeChanged();
  } else if (name == checkedAttr) {
    // Another radio button in the same group might be checked by state
    // restore. We shouldn't call setChecked() even if this has the checked
    // attribute. So, delay the setChecked() call until
    // finishParsingChildren() is called if parsing is in progress.
    if ((!parsing_in_progress_ ||
         !GetDocument().GetFormController().HasFormStates()) &&
        !dirty_checkedness_) {
      setChecked(!value.IsNull());
      dirty_checkedness_ = false;
    }
    PseudoStateChanged(CSSSelector::kPseudoDefault);
  } else if (name == maxlengthAttr) {
    SetNeedsValidityCheck();
  } else if (name == minlengthAttr) {
    SetNeedsValidityCheck();
  } else if (name == sizeAttr) {
    unsigned size = 0;
    if (value.IsEmpty() || !ParseHTMLNonNegativeInteger(value, size) ||
        size == 0 || size > 0x7fffffffu)
      size = kDefaultSize;
    if (size_ != size) {
      size_ = size;
      if (GetLayoutObject())
        GetLayoutObject()
            ->SetNeedsLayoutAndPrefWidthsRecalcAndFullPaintInvalidation(
                LayoutInvalidationReason::kAttributeChanged);
    }
  } else if (name == altAttr) {
    input_type_view_->AltAttributeChanged();
  } else if (name == srcAttr) {
    input_type_view_->SrcAttributeChanged();
  } else if (name == usemapAttr || name == accesskeyAttr) {
    // FIXME: ignore for the moment
  } else if (name == onsearchAttr) {
    // Search field and slider attributes all just cause updateFromElement to be
    // called through style recalcing.
    SetAttributeEventListener(
        EventTypeNames::search,
        CreateAttributeEventListener(this, name, value, EventParameterName()));
  } else if (name == incrementalAttr) {
    UseCounter::Count(GetDocument(), WebFeature::kIncrementalAttribute);
  } else if (name == minAttr) {
    input_type_view_->MinOrMaxAttributeChanged();
    input_type_->SanitizeValueInResponseToMinOrMaxAttributeChange();
    input_type_->InRangeChanged();
    SetNeedsValidityCheck();
    UseCounter::Count(GetDocument(), WebFeature::kMinAttribute);
  } else if (name == maxAttr) {
    input_type_view_->MinOrMaxAttributeChanged();
    input_type_->SanitizeValueInResponseToMinOrMaxAttributeChange();
    input_type_->InRangeChanged();
    SetNeedsValidityCheck();
    UseCounter::Count(GetDocument(), WebFeature::kMaxAttribute);
  } else if (name == multipleAttr) {
    input_type_view_->MultipleAttributeChanged();
    SetNeedsValidityCheck();
  } else if (name == stepAttr) {
    input_type_view_->StepAttributeChanged();
    SetNeedsValidityCheck();
    UseCounter::Count(GetDocument(), WebFeature::kStepAttribute);
  } else if (name == patternAttr) {
    SetNeedsValidityCheck();
    UseCounter::Count(GetDocument(), WebFeature::kPatternAttribute);
  } else if (name == readonlyAttr) {
    TextControlElement::ParseAttribute(params);
    input_type_view_->ReadonlyAttributeChanged();
  } else if (name == listAttr) {
    has_non_empty_list_ = !value.IsEmpty();
    if (has_non_empty_list_) {
      ResetListAttributeTargetObserver();
      ListAttributeTargetChanged();
    }
    UseCounter::Count(GetDocument(), WebFeature::kListAttribute);
  } else if (name == webkitdirectoryAttr) {
    TextControlElement::ParseAttribute(params);
    UseCounter::Count(GetDocument(), WebFeature::kPrefixedDirectoryAttribute);
  } else {
    if (name == formactionAttr)
      LogUpdateAttributeIfIsolatedWorldAndInDocument("input", params);
    TextControlElement::ParseAttribute(params);
  }
  input_type_view_->AttributeChanged();
}

void HTMLInputElement::ParserDidSetAttributes() {
  DCHECK(parsing_in_progress_);
  InitializeTypeInParsing();
}

void HTMLInputElement::FinishParsingChildren() {
  parsing_in_progress_ = false;
  DCHECK(input_type_);
  DCHECK(input_type_view_);
  TextControlElement::FinishParsingChildren();
  if (!state_restored_) {
    bool checked = hasAttribute(checkedAttr);
    if (checked)
      setChecked(checked);
    dirty_checkedness_ = false;
  }
}

bool HTMLInputElement::LayoutObjectIsNeeded(const ComputedStyle& style) {
  return input_type_->LayoutObjectIsNeeded() &&
         TextControlElement::LayoutObjectIsNeeded(style);
}

LayoutObject* HTMLInputElement::CreateLayoutObject(const ComputedStyle& style) {
  return input_type_view_->CreateLayoutObject(style);
}

void HTMLInputElement::AttachLayoutTree(AttachContext& context) {
  SyncReattachContext reattach_context(context);
  TextControlElement::AttachLayoutTree(context);
  if (GetLayoutObject()) {
    input_type_->OnAttachWithLayoutObject();
  }

  input_type_view_->StartResourceLoading();
  input_type_->CountUsage();
}

void HTMLInputElement::DetachLayoutTree(const AttachContext& context) {
  if (GetLayoutObject()) {
    input_type_->OnDetachWithLayoutObject();
  }
  TextControlElement::DetachLayoutTree(context);
  needs_to_update_view_value_ = true;
  input_type_view_->ClosePopupView();
}

String HTMLInputElement::AltText() const {
  // http://www.w3.org/TR/1998/REC-html40-19980424/appendix/notes.html#altgen
  // also heavily discussed by Hixie on bugzilla
  // note this is intentionally different to HTMLImageElement::altText()
  String alt = FastGetAttribute(altAttr);
  // fall back to title attribute
  if (alt.IsNull())
    alt = FastGetAttribute(titleAttr);
  if (alt.IsNull())
    alt = FastGetAttribute(valueAttr);
  if (alt.IsNull())
    alt = GetLocale().QueryString(WebLocalizedString::kInputElementAltText);
  return alt;
}

bool HTMLInputElement::CanBeSuccessfulSubmitButton() const {
  return input_type_->CanBeSuccessfulSubmitButton();
}

bool HTMLInputElement::IsActivatedSubmit() const {
  return is_activated_submit_;
}

void HTMLInputElement::SetActivatedSubmit(bool flag) {
  is_activated_submit_ = flag;
}

void HTMLInputElement::AppendToFormData(FormData& form_data) {
  if (input_type_->IsFormDataAppendable())
    input_type_->AppendToFormData(form_data);
}

String HTMLInputElement::ResultForDialogSubmit() {
  return input_type_->ResultForDialogSubmit();
}

void HTMLInputElement::ResetImpl() {
  if (input_type_->GetValueMode() == ValueMode::kValue) {
    SetNonDirtyValue(DefaultValue());
    SetNeedsValidityCheck();
  } else if (input_type_->GetValueMode() == ValueMode::kFilename) {
    SetNonDirtyValue(String());
    SetNeedsValidityCheck();
  }

  setChecked(hasAttribute(checkedAttr));
  dirty_checkedness_ = false;
}

bool HTMLInputElement::IsTextField() const {
  return input_type_->IsTextField();
}

bool HTMLInputElement::HasBeenPasswordField() const {
  return has_been_password_field_;
}

void HTMLInputElement::DispatchChangeEventIfNeeded() {
  if (isConnected() && input_type_->ShouldSendChangeEventAfterCheckedChanged())
    DispatchChangeEvent();
}

void HTMLInputElement::DispatchInputAndChangeEventIfNeeded() {
  if (isConnected() &&
      input_type_->ShouldSendChangeEventAfterCheckedChanged()) {
    DispatchInputEvent();
    DispatchChangeEvent();
  }
}

bool HTMLInputElement::checked() const {
  input_type_->ReadingChecked();
  return is_checked_;
}

void HTMLInputElement::setChecked(bool now_checked,
                                  TextFieldEventBehavior event_behavior) {
  dirty_checkedness_ = true;
  if (checked() == now_checked)
    return;

  is_checked_ = now_checked;

  if (RadioButtonGroupScope* scope = GetRadioButtonGroupScope())
    scope->UpdateCheckedState(this);
  if (LayoutObject* o = GetLayoutObject())
    o->InvalidateIfControlStateChanged(kCheckedControlState);
  SetNeedsValidityCheck();

  // Ideally we'd do this from the layout tree (matching
  // LayoutTextView), but it's not possible to do it at the moment
  // because of the way the code is structured.
  if (GetLayoutObject()) {
    if (AXObjectCache* cache =
            GetLayoutObject()->GetDocument().ExistingAXObjectCache())
      cache->CheckedStateChanged(this);
  }

  // Only send a change event for items in the document (avoid firing during
  // parsing) and don't send a change event for a radio button that's getting
  // unchecked to match other browsers. DOM is not a useful standard for this
  // because it says only to fire change events at "lose focus" time, which is
  // definitely wrong in practice for these types of elements.
  if (event_behavior == kDispatchInputAndChangeEvent && isConnected() &&
      input_type_->ShouldSendChangeEventAfterCheckedChanged()) {
    DispatchInputEvent();
  }

  PseudoStateChanged(CSSSelector::kPseudoChecked);
}

void HTMLInputElement::setIndeterminate(bool new_value) {
  if (indeterminate() == new_value)
    return;

  is_indeterminate_ = new_value;

  PseudoStateChanged(CSSSelector::kPseudoIndeterminate);

  if (LayoutObject* o = GetLayoutObject())
    o->InvalidateIfControlStateChanged(kCheckedControlState);
}

unsigned HTMLInputElement::size() const {
  return size_;
}

bool HTMLInputElement::SizeShouldIncludeDecoration(int& preferred_size) const {
  return input_type_view_->SizeShouldIncludeDecoration(kDefaultSize,
                                                       preferred_size);
}

void HTMLInputElement::CloneNonAttributePropertiesFrom(const Element& source,
                                                       CloneChildrenFlag flag) {
  const HTMLInputElement& source_element = ToHTMLInputElement(source);

  non_attribute_value_ = source_element.non_attribute_value_;
  has_dirty_value_ = source_element.has_dirty_value_;
  setChecked(source_element.is_checked_);
  dirty_checkedness_ = source_element.dirty_checkedness_;
  is_indeterminate_ = source_element.is_indeterminate_;
  input_type_->CopyNonAttributeProperties(source_element);

  TextControlElement::CloneNonAttributePropertiesFrom(source, flag);

  needs_to_update_view_value_ = true;
  input_type_view_->UpdateView();
}

String HTMLInputElement::value() const {
  switch (input_type_->GetValueMode()) {
    case ValueMode::kFilename:
      return input_type_->ValueInFilenameValueMode();
    case ValueMode::kDefault:
      return FastGetAttribute(valueAttr);
    case ValueMode::kDefaultOn: {
      AtomicString value_string = FastGetAttribute(valueAttr);
      return value_string.IsNull() ? "on" : value_string;
    }
    case ValueMode::kValue:
      return non_attribute_value_;
  }
  NOTREACHED();
  return g_empty_string;
}

String HTMLInputElement::ValueOrDefaultLabel() const {
  String value = this->value();
  if (!value.IsNull())
    return value;
  return input_type_->DefaultLabel();
}

void HTMLInputElement::SetValueForUser(const String& value) {
  // Call setValue and make it send a change event.
  setValue(value, kDispatchChangeEvent);
}

void HTMLInputElement::SetSuggestedValue(const String& value) {
  if (!input_type_->CanSetSuggestedValue())
    return;
  needs_to_update_view_value_ = true;
  TextControlElement::SetSuggestedValue(SanitizeValue(value));
  SetNeedsStyleRecalc(
      kSubtreeStyleChange,
      StyleChangeReasonForTracing::Create(StyleChangeReason::kControlValue));
  input_type_view_->UpdateView();
}

void HTMLInputElement::SetEditingValue(const String& value) {
  if (!GetLayoutObject() || !IsTextField())
    return;
  SetInnerEditorValue(value);
  SubtreeHasChanged();

  unsigned max = value.length();
  SetSelectionRange(max, max);
  DispatchInputEvent();
}

void HTMLInputElement::SetInnerEditorValue(const String& value) {
  TextControlElement::SetInnerEditorValue(value);
  needs_to_update_view_value_ = false;
}

void HTMLInputElement::setValue(const String& value,
                                ExceptionState& exception_state,
                                TextFieldEventBehavior event_behavior) {
  // FIXME: Remove type check.
  if (type() == InputTypeNames::file && !value.IsEmpty()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "This input element accepts a filename, "
                                      "which may only be programmatically set "
                                      "to the empty string.");
    return;
  }
  setValue(value, event_behavior);
}

void HTMLInputElement::setValue(const String& value,
                                TextFieldEventBehavior event_behavior,
                                TextControlSetValueSelection selection) {
  input_type_->WarnIfValueIsInvalidAndElementIsVisible(value);
  if (!input_type_->CanSetValue(value))
    return;

  // Clear the suggested value. Use the base class version to not trigger a view
  // update.
  TextControlElement::SetSuggestedValue(String());

  EventQueueScope scope;
  String sanitized_value = SanitizeValue(value);
  bool value_changed = sanitized_value != this->value();

  SetLastChangeWasNotUserEdit();
  needs_to_update_view_value_ = true;

  input_type_->SetValue(sanitized_value, value_changed, event_behavior,
                        selection);
  input_type_view_->DidSetValue(sanitized_value, value_changed);

  if (value_changed)
    NotifyFormStateChanged();
}

void HTMLInputElement::SetNonAttributeValue(const String& sanitized_value) {
  // This is a common code for ValueMode::kValue.
  DCHECK_EQ(input_type_->GetValueMode(), ValueMode::kValue);
  non_attribute_value_ = sanitized_value;
  has_dirty_value_ = true;
  SetNeedsValidityCheck();
  input_type_->InRangeChanged();
}

void HTMLInputElement::SetNonAttributeValueByUserEdit(
    const String& sanitized_value) {
  SetValueBeforeFirstUserEditIfNotSet();
  SetNonAttributeValue(sanitized_value);
  CheckIfValueWasReverted(sanitized_value);
}

void HTMLInputElement::SetNonDirtyValue(const String& new_value) {
  setValue(new_value);
  has_dirty_value_ = false;
}

bool HTMLInputElement::HasDirtyValue() const {
  return has_dirty_value_;
}

void HTMLInputElement::UpdateView() {
  input_type_view_->UpdateView();
}

double HTMLInputElement::valueAsDate(bool& is_null) const {
  double date = input_type_->ValueAsDate();
  is_null = !std::isfinite(date);
  return date;
}

void HTMLInputElement::setValueAsDate(double value,
                                      bool is_null,
                                      ExceptionState& exception_state) {
  input_type_->SetValueAsDate(value, exception_state);
}

double HTMLInputElement::valueAsNumber() const {
  return input_type_->ValueAsDouble();
}

void HTMLInputElement::setValueAsNumber(double new_value,
                                        ExceptionState& exception_state,
                                        TextFieldEventBehavior event_behavior) {
  // http://www.whatwg.org/specs/web-apps/current-work/multipage/common-input-element-attributes.html#dom-input-valueasnumber
  // On setting, if the new value is infinite, then throw a TypeError exception.
  if (std::isinf(new_value)) {
    exception_state.ThrowTypeError(
        ExceptionMessages::NotAFiniteNumber(new_value));
    return;
  }
  input_type_->SetValueAsDouble(new_value, event_behavior, exception_state);
}

void HTMLInputElement::SetValueFromRenderer(const String& value) {
  // File upload controls will never use this.
  DCHECK_NE(type(), InputTypeNames::file);

  // Clear the suggested value. Use the base class version to not trigger a view
  // update.
  TextControlElement::SetSuggestedValue(String());

  // Renderer and our event handler are responsible for sanitizing values.
  DCHECK(value == input_type_->SanitizeUserInputValue(value) ||
         input_type_->SanitizeUserInputValue(value).IsEmpty());

  DCHECK(!value.IsNull());
  SetValueBeforeFirstUserEditIfNotSet();
  non_attribute_value_ = value;
  has_dirty_value_ = true;
  needs_to_update_view_value_ = false;
  CheckIfValueWasReverted(value);

  // Input event is fired by the Node::defaultEventHandler for editable
  // controls.
  if (!IsTextField())
    DispatchInputEvent();
  NotifyFormStateChanged();

  SetNeedsValidityCheck();

  // Clear autofill flag (and yellow background) on user edit.
  SetAutofilled(false);
}

EventDispatchHandlingState* HTMLInputElement::PreDispatchEventHandler(
    Event* event) {
  if (event->type() == EventTypeNames::textInput &&
      input_type_view_->ShouldSubmitImplicitly(event)) {
    event->stopPropagation();
    return nullptr;
  }
  if (event->type() != EventTypeNames::click)
    return nullptr;
  if (!event->IsMouseEvent() ||
      ToMouseEvent(event)->button() !=
          static_cast<short>(WebPointerProperties::Button::kLeft))
    return nullptr;
  return input_type_view_->WillDispatchClick();
}

void HTMLInputElement::PostDispatchEventHandler(
    Event* event,
    EventDispatchHandlingState* state) {
  if (!state)
    return;
  input_type_view_->DidDispatchClick(event,
                                     *static_cast<ClickHandlingState*>(state));
}

void HTMLInputElement::DefaultEventHandler(Event* evt) {
  if (evt->IsMouseEvent() && evt->type() == EventTypeNames::click &&
      ToMouseEvent(evt)->button() ==
          static_cast<short>(WebPointerProperties::Button::kLeft)) {
    input_type_view_->HandleClickEvent(ToMouseEvent(evt));
    if (evt->DefaultHandled())
      return;
  }

  if (evt->IsKeyboardEvent() && evt->type() == EventTypeNames::keydown) {
    input_type_view_->HandleKeydownEvent(ToKeyboardEvent(evt));
    if (evt->DefaultHandled())
      return;
  }

  // Call the base event handler before any of our own event handling for almost
  // all events in text fields.  Makes editing keyboard handling take precedence
  // over the keydown and keypress handling in this function.
  bool call_base_class_early =
      IsTextField() && (evt->type() == EventTypeNames::keydown ||
                        evt->type() == EventTypeNames::keypress);
  if (call_base_class_early) {
    TextControlElement::DefaultEventHandler(evt);
    if (evt->DefaultHandled())
      return;
  }

  // DOMActivate events cause the input to be "activated" - in the case of image
  // and submit inputs, this means actually submitting the form. For reset
  // inputs, the form is reset. These events are sent when the user clicks on
  // the element, or presses enter while it is the active element. JavaScript
  // code wishing to activate the element must dispatch a DOMActivate event - a
  // click event will not do the job.
  if (evt->type() == EventTypeNames::DOMActivate) {
    input_type_view_->HandleDOMActivateEvent(evt);
    if (evt->DefaultHandled())
      return;
  }

  // Use key press event here since sending simulated mouse events
  // on key down blocks the proper sending of the key press event.
  if (evt->IsKeyboardEvent() && evt->type() == EventTypeNames::keypress) {
    input_type_view_->HandleKeypressEvent(ToKeyboardEvent(evt));
    if (evt->DefaultHandled())
      return;
  }

  if (evt->IsKeyboardEvent() && evt->type() == EventTypeNames::keyup) {
    input_type_view_->HandleKeyupEvent(ToKeyboardEvent(evt));
    if (evt->DefaultHandled())
      return;
  }

  if (input_type_view_->ShouldSubmitImplicitly(evt)) {
    // FIXME: Remove type check.
    if (type() == InputTypeNames::search) {
      GetDocument()
          .GetTaskRunner(TaskType::kUserInteraction)
          ->PostTask(FROM_HERE, WTF::Bind(&HTMLInputElement::OnSearch,
                                          WrapPersistent(this)));
    }
    // Form submission finishes editing, just as loss of focus does.
    // If there was a change, send the event now.
    DispatchFormControlChangeEvent();

    HTMLFormElement* form_for_submission =
        input_type_view_->FormForSubmission();
    // Form may never have been present, or may have been destroyed by code
    // responding to the change event.
    if (form_for_submission) {
      form_for_submission->SubmitImplicitly(evt,
                                            CanTriggerImplicitSubmission());
    }
    evt->SetDefaultHandled();
    return;
  }

  if (evt->IsBeforeTextInsertedEvent()) {
    input_type_view_->HandleBeforeTextInsertedEvent(
        static_cast<BeforeTextInsertedEvent*>(evt));
  }

  if (evt->IsMouseEvent() && evt->type() == EventTypeNames::mousedown) {
    input_type_view_->HandleMouseDownEvent(ToMouseEvent(evt));
    if (evt->DefaultHandled())
      return;
  }

  input_type_view_->ForwardEvent(evt);

  if (!call_base_class_early && !evt->DefaultHandled())
    TextControlElement::DefaultEventHandler(evt);
}

void HTMLInputElement::CreateShadowSubtree() {
  input_type_view_->CreateShadowSubtree();
}

bool HTMLInputElement::HasActivationBehavior() const {
  return true;
}

bool HTMLInputElement::WillRespondToMouseClickEvents() {
  // FIXME: Consider implementing willRespondToMouseClickEvents() in InputType
  // if more accurate results are necessary.
  if (!IsDisabledFormControl())
    return true;

  return TextControlElement::WillRespondToMouseClickEvents();
}

bool HTMLInputElement::IsURLAttribute(const Attribute& attribute) const {
  return attribute.GetName() == srcAttr ||
         attribute.GetName() == formactionAttr ||
         TextControlElement::IsURLAttribute(attribute);
}

bool HTMLInputElement::HasLegalLinkAttribute(const QualifiedName& name) const {
  return input_type_->HasLegalLinkAttribute(name) ||
         TextControlElement::HasLegalLinkAttribute(name);
}

const QualifiedName& HTMLInputElement::SubResourceAttributeName() const {
  return input_type_->SubResourceAttributeName();
}

const AtomicString& HTMLInputElement::DefaultValue() const {
  return FastGetAttribute(valueAttr);
}

static inline bool IsRFC2616TokenCharacter(UChar ch) {
  return IsASCII(ch) && ch > ' ' && ch != '"' && ch != '(' && ch != ')' &&
         ch != ',' && ch != '/' && (ch < ':' || ch > '@') &&
         (ch < '[' || ch > ']') && ch != '{' && ch != '}' && ch != 0x7f;
}

static bool IsValidMIMEType(const String& type) {
  size_t slash_position = type.find('/');
  if (slash_position == kNotFound || !slash_position ||
      slash_position == type.length() - 1)
    return false;
  for (size_t i = 0; i < type.length(); ++i) {
    if (!IsRFC2616TokenCharacter(type[i]) && i != slash_position)
      return false;
  }
  return true;
}

static bool IsValidFileExtension(const String& type) {
  if (type.length() < 2)
    return false;
  return type[0] == '.';
}

static Vector<String> ParseAcceptAttribute(const String& accept_string,
                                           bool (*predicate)(const String&)) {
  Vector<String> types;
  if (accept_string.IsEmpty())
    return types;

  Vector<String> split_types;
  accept_string.Split(',', false, split_types);
  for (const String& split_type : split_types) {
    String trimmed_type = StripLeadingAndTrailingHTMLSpaces(split_type);
    if (trimmed_type.IsEmpty())
      continue;
    if (!predicate(trimmed_type))
      continue;
    types.push_back(trimmed_type.DeprecatedLower());
  }

  return types;
}

Vector<String> HTMLInputElement::AcceptMIMETypes() const {
  return ParseAcceptAttribute(FastGetAttribute(acceptAttr), IsValidMIMEType);
}

Vector<String> HTMLInputElement::AcceptFileExtensions() const {
  return ParseAcceptAttribute(FastGetAttribute(acceptAttr),
                              IsValidFileExtension);
}

const AtomicString& HTMLInputElement::Alt() const {
  return FastGetAttribute(altAttr);
}

bool HTMLInputElement::Multiple() const {
  return FastHasAttribute(multipleAttr);
}

void HTMLInputElement::setSize(unsigned size, ExceptionState& exception_state) {
  if (size == 0) {
    exception_state.ThrowDOMException(
        kIndexSizeError, "The value provided is 0, which is an invalid size.");
  } else {
    SetUnsignedIntegralAttribute(sizeAttr, size ? size : kDefaultSize,
                                 kDefaultSize);
  }
}

KURL HTMLInputElement::Src() const {
  return GetDocument().CompleteURL(FastGetAttribute(srcAttr));
}

FileList* HTMLInputElement::files() const {
  return input_type_->Files();
}

void HTMLInputElement::setFiles(FileList* files) {
  input_type_->SetFiles(files);
}

bool HTMLInputElement::ReceiveDroppedFiles(const DragData* drag_data) {
  return input_type_->ReceiveDroppedFiles(drag_data);
}

String HTMLInputElement::DroppedFileSystemId() {
  return input_type_->DroppedFileSystemId();
}

bool HTMLInputElement::CanReceiveDroppedFiles() const {
  return can_receive_dropped_files_;
}

void HTMLInputElement::SetCanReceiveDroppedFiles(
    bool can_receive_dropped_files) {
  if (!!can_receive_dropped_files_ == can_receive_dropped_files)
    return;
  can_receive_dropped_files_ = can_receive_dropped_files;
  if (GetLayoutObject())
    GetLayoutObject()->UpdateFromElement();
}

String HTMLInputElement::SanitizeValue(const String& proposed_value) const {
  return input_type_->SanitizeValue(proposed_value);
}

String HTMLInputElement::LocalizeValue(const String& proposed_value) const {
  if (proposed_value.IsNull())
    return proposed_value;
  return input_type_->LocalizeValue(proposed_value);
}

bool HTMLInputElement::IsInRange() const {
  return willValidate() && input_type_->IsInRange(value());
}

bool HTMLInputElement::IsOutOfRange() const {
  return willValidate() && input_type_->IsOutOfRange(value());
}

bool HTMLInputElement::IsRequiredFormControl() const {
  return input_type_->SupportsRequired() && IsRequired();
}

bool HTMLInputElement::MatchesReadOnlyPseudoClass() const {
  return input_type_->SupportsReadOnly() && IsReadOnly();
}

bool HTMLInputElement::MatchesReadWritePseudoClass() const {
  return input_type_->SupportsReadOnly() && !IsReadOnly();
}

void HTMLInputElement::OnSearch() {
  input_type_->DispatchSearchEvent();
}

void HTMLInputElement::UpdateClearButtonVisibility() {
  input_type_view_->UpdateClearButtonVisibility();
}

void HTMLInputElement::WillChangeForm() {
  if (input_type_)
    RemoveFromRadioButtonGroup();
  TextControlElement::WillChangeForm();
}

void HTMLInputElement::DidChangeForm() {
  TextControlElement::DidChangeForm();
  if (input_type_)
    AddToRadioButtonGroup();
}

Node::InsertionNotificationRequest HTMLInputElement::InsertedInto(
    ContainerNode* insertion_point) {
  TextControlElement::InsertedInto(insertion_point);
  if (insertion_point->isConnected() && !Form())
    AddToRadioButtonGroup();
  ResetListAttributeTargetObserver();
  LogAddElementIfIsolatedWorldAndInDocument("input", typeAttr, formactionAttr);
  return kInsertionShouldCallDidNotifySubtreeInsertions;
}

void HTMLInputElement::RemovedFrom(ContainerNode* insertion_point) {
  input_type_view_->ClosePopupView();
  if (insertion_point->isConnected() && !Form())
    RemoveFromRadioButtonGroup();
  TextControlElement::RemovedFrom(insertion_point);
  DCHECK(!isConnected());
  ResetListAttributeTargetObserver();
}

void HTMLInputElement::DidMoveToNewDocument(Document& old_document) {
  if (ImageLoader())
    ImageLoader()->ElementDidMoveToNewDocument();

  // FIXME: Remove type check.
  if (type() == InputTypeNames::radio)
    GetTreeScope().GetRadioButtonGroupScope().RemoveButton(this);

  TextControlElement::DidMoveToNewDocument(old_document);
}

bool HTMLInputElement::RecalcWillValidate() const {
  return input_type_->SupportsValidation() &&
         TextControlElement::RecalcWillValidate();
}

void HTMLInputElement::RequiredAttributeChanged() {
  TextControlElement::RequiredAttributeChanged();
  if (RadioButtonGroupScope* scope = GetRadioButtonGroupScope())
    scope->RequiredAttributeChanged(this);
  input_type_view_->RequiredAttributeChanged();
}

void HTMLInputElement::DisabledAttributeChanged() {
  TextControlElement::DisabledAttributeChanged();
  input_type_view_->DisabledAttributeChanged();
}

void HTMLInputElement::SelectColorInColorChooser(const Color& color) {
  if (ColorChooserClient* client = input_type_->GetColorChooserClient())
    client->DidChooseColor(color);
}

void HTMLInputElement::EndColorChooser() {
  if (ColorChooserClient* client = input_type_->GetColorChooserClient())
    client->DidEndChooser();
}

HTMLElement* HTMLInputElement::list() const {
  return DataList();
}

HTMLDataListElement* HTMLInputElement::DataList() const {
  if (!has_non_empty_list_)
    return nullptr;

  if (!input_type_->ShouldRespectListAttribute())
    return nullptr;

  return ToHTMLDataListElementOrNull(
      GetTreeScope().getElementById(FastGetAttribute(listAttr)));
}

bool HTMLInputElement::HasValidDataListOptions() const {
  HTMLDataListElement* data_list = DataList();
  if (!data_list)
    return false;
  HTMLDataListOptionsCollection* options = data_list->options();
  for (unsigned i = 0; HTMLOptionElement* option = options->Item(i); ++i) {
    if (!option->value().IsEmpty() && !option->IsDisabledFormControl() &&
        IsValidValue(option->value()))
      return true;
  }
  return false;
}

HeapVector<Member<HTMLOptionElement>>
HTMLInputElement::FilteredDataListOptions() const {
  HeapVector<Member<HTMLOptionElement>> filtered;
  HTMLDataListElement* data_list = DataList();
  if (!data_list)
    return filtered;

  String value = InnerEditorValue();
  if (Multiple() && type() == InputTypeNames::email) {
    Vector<String> emails;
    value.Split(',', true, emails);
    if (!emails.IsEmpty())
      value = emails.back().StripWhiteSpace();
  }

  HTMLDataListOptionsCollection* options = data_list->options();
  filtered.ReserveCapacity(options->length());
  value = value.FoldCase();
  for (unsigned i = 0; i < options->length(); ++i) {
    HTMLOptionElement* option = options->Item(i);
    DCHECK(option);
    if (!value.IsEmpty()) {
      // Firefox shows OPTIONs with matched labels, Edge shows OPTIONs
      // with matches values. We show both.
      if (option->value().FoldCase().Find(value) == kNotFound &&
          option->label().FoldCase().Find(value) == kNotFound)
        continue;
    }
    // TODO(tkent): Should allow invalid strings. crbug.com/607097.
    if (option->value().IsEmpty() || option->IsDisabledFormControl() ||
        !IsValidValue(option->value()))
      continue;
    filtered.push_back(option);
  }
  return filtered;
}

void HTMLInputElement::SetListAttributeTargetObserver(
    ListAttributeTargetObserver* new_observer) {
  if (list_attribute_target_observer_)
    list_attribute_target_observer_->Unregister();
  list_attribute_target_observer_ = new_observer;
}

void HTMLInputElement::ResetListAttributeTargetObserver() {
  const AtomicString& value = FastGetAttribute(listAttr);
  if (!value.IsNull() && isConnected()) {
    SetListAttributeTargetObserver(
        ListAttributeTargetObserver::Create(value, this));
  } else {
    SetListAttributeTargetObserver(nullptr);
  }
}

void HTMLInputElement::ListAttributeTargetChanged() {
  input_type_view_->ListAttributeTargetChanged();
}

bool HTMLInputElement::IsSteppable() const {
  return input_type_->IsSteppable();
}

bool HTMLInputElement::IsTextButton() const {
  return input_type_->IsTextButton();
}

bool HTMLInputElement::IsEnumeratable() const {
  return input_type_->IsEnumeratable();
}

bool HTMLInputElement::SupportLabels() const {
  return input_type_->IsInteractiveContent();
}

bool HTMLInputElement::MatchesDefaultPseudoClass() const {
  return input_type_->MatchesDefaultPseudoClass();
}

bool HTMLInputElement::ShouldAppearChecked() const {
  return checked() && input_type_->IsCheckable();
}

void HTMLInputElement::SetPlaceholderVisibility(bool visible) {
  is_placeholder_visible_ = visible;
}

bool HTMLInputElement::SupportsPlaceholder() const {
  return input_type_->SupportsPlaceholder();
}

void HTMLInputElement::UpdatePlaceholderText() {
  return input_type_view_->UpdatePlaceholderText();
}

String HTMLInputElement::GetPlaceholderValue() const {
  return !SuggestedValue().IsEmpty() ? SuggestedValue() : StrippedPlaceholder();
}

String HTMLInputElement::DefaultToolTip() const {
  return input_type_->DefaultToolTip(*input_type_view_);
}

bool HTMLInputElement::ShouldAppearIndeterminate() const {
  return input_type_->ShouldAppearIndeterminate();
}

bool HTMLInputElement::IsInRequiredRadioButtonGroup() {
  // TODO(tkent): Remove type check.
  DCHECK_EQ(type(), InputTypeNames::radio);
  if (RadioButtonGroupScope* scope = GetRadioButtonGroupScope())
    return scope->IsInRequiredGroup(this);
  return false;
}

HTMLInputElement* HTMLInputElement::CheckedRadioButtonForGroup() {
  if (checked())
    return this;
  if (RadioButtonGroupScope* scope = GetRadioButtonGroupScope())
    return scope->CheckedButtonForGroup(GetName());
  return nullptr;
}

RadioButtonGroupScope* HTMLInputElement::GetRadioButtonGroupScope() const {
  // FIXME: Remove type check.
  if (type() != InputTypeNames::radio)
    return nullptr;
  if (HTMLFormElement* form_element = Form())
    return &form_element->GetRadioButtonGroupScope();
  if (isConnected())
    return &GetTreeScope().GetRadioButtonGroupScope();
  return nullptr;
}

unsigned HTMLInputElement::SizeOfRadioGroup() const {
  RadioButtonGroupScope* scope = GetRadioButtonGroupScope();
  if (!scope)
    return 0;
  return scope->GroupSizeFor(this);
}

inline void HTMLInputElement::AddToRadioButtonGroup() {
  if (RadioButtonGroupScope* scope = GetRadioButtonGroupScope())
    scope->AddButton(this);
}

inline void HTMLInputElement::RemoveFromRadioButtonGroup() {
  if (RadioButtonGroupScope* scope = GetRadioButtonGroupScope())
    scope->RemoveButton(this);
}

unsigned HTMLInputElement::height() const {
  return input_type_->Height();
}

unsigned HTMLInputElement::width() const {
  return input_type_->Width();
}

void HTMLInputElement::setHeight(unsigned height) {
  SetUnsignedIntegralAttribute(heightAttr, height);
}

void HTMLInputElement::setWidth(unsigned width) {
  SetUnsignedIntegralAttribute(widthAttr, width);
}

ListAttributeTargetObserver* ListAttributeTargetObserver::Create(
    const AtomicString& id,
    HTMLInputElement* element) {
  return new ListAttributeTargetObserver(id, element);
}

ListAttributeTargetObserver::ListAttributeTargetObserver(
    const AtomicString& id,
    HTMLInputElement* element)
    : IdTargetObserver(element->GetTreeScope().GetIdTargetObserverRegistry(),
                       id),
      element_(element) {}

void ListAttributeTargetObserver::Trace(blink::Visitor* visitor) {
  visitor->Trace(element_);
  IdTargetObserver::Trace(visitor);
}

void ListAttributeTargetObserver::IdTargetChanged() {
  element_->ListAttributeTargetChanged();
}

void HTMLInputElement::setRangeText(const String& replacement,
                                    ExceptionState& exception_state) {
  if (!input_type_->SupportsSelectionAPI()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "The input element's type ('" +
                                          input_type_->FormControlType() +
                                          "') does not support selection.");
    return;
  }

  TextControlElement::setRangeText(replacement, exception_state);
}

void HTMLInputElement::setRangeText(const String& replacement,
                                    unsigned start,
                                    unsigned end,
                                    const String& selection_mode,
                                    ExceptionState& exception_state) {
  if (!input_type_->SupportsSelectionAPI()) {
    exception_state.ThrowDOMException(kInvalidStateError,
                                      "The input element's type ('" +
                                          input_type_->FormControlType() +
                                          "') does not support selection.");
    return;
  }

  TextControlElement::setRangeText(replacement, start, end, selection_mode,
                                   exception_state);
}

bool HTMLInputElement::SetupDateTimeChooserParameters(
    DateTimeChooserParameters& parameters) {
  if (!GetDocument().View())
    return false;

  parameters.type = type();
  parameters.minimum = Minimum();
  parameters.maximum = Maximum();
  parameters.required = IsRequired();
  if (!RuntimeEnabledFeatures::LangAttributeAwareFormControlUIEnabled()) {
    parameters.locale = DefaultLanguage();
  } else {
    AtomicString computed_locale = ComputeInheritedLanguage();
    parameters.locale =
        computed_locale.IsEmpty() ? DefaultLanguage() : computed_locale;
  }

  StepRange step_range = CreateStepRange(kRejectAny);
  if (step_range.HasStep()) {
    parameters.step = step_range.Step().ToDouble();
    parameters.step_base = step_range.StepBase().ToDouble();
  } else {
    parameters.step = 1.0;
    parameters.step_base = 0;
  }

  parameters.anchor_rect_in_screen =
      GetDocument().View()->ContentsToScreen(PixelSnappedBoundingBox());
  parameters.double_value = input_type_->ValueAsDouble();
  parameters.is_anchor_element_rtl =
      input_type_view_->ComputedTextDirection() == TextDirection::kRtl;
  if (HTMLDataListElement* data_list = DataList()) {
    HTMLDataListOptionsCollection* options = data_list->options();
    for (unsigned i = 0; HTMLOptionElement* option = options->Item(i); ++i) {
      if (option->value().IsEmpty() || option->IsDisabledFormControl() ||
          !IsValidValue(option->value()))
        continue;
      DateTimeSuggestion suggestion;
      suggestion.value =
          input_type_->ParseToNumber(option->value(), Decimal::Nan())
              .ToDouble();
      if (std::isnan(suggestion.value))
        continue;
      suggestion.localized_value = LocalizeValue(option->value());
      suggestion.label =
          option->value() == option->label() ? String() : option->label();
      parameters.suggestions.push_back(suggestion);
    }
  }
  return true;
}

bool HTMLInputElement::SupportsInputModeAttribute() const {
  return input_type_->SupportsInputModeAttribute();
}

void HTMLInputElement::SetShouldRevealPassword(bool value) {
  if (!!should_reveal_password_ == value)
    return;
  should_reveal_password_ = value;
  LazyReattachIfAttached();
}

bool HTMLInputElement::IsInteractiveContent() const {
  return input_type_->IsInteractiveContent();
}

bool HTMLInputElement::SupportsAutofocus() const {
  return input_type_->IsInteractiveContent();
}

scoped_refptr<ComputedStyle> HTMLInputElement::CustomStyleForLayoutObject() {
  return input_type_view_->CustomStyleForLayoutObject(
      OriginalStyleForLayoutObject());
}

void HTMLInputElement::DidNotifySubtreeInsertionsToDocument() {
  ListAttributeTargetChanged();
}

AXObject* HTMLInputElement::PopupRootAXObject() {
  return input_type_view_->PopupRootAXObject();
}

void HTMLInputElement::EnsureFallbackContent() {
  input_type_view_->EnsureFallbackContent();
}

void HTMLInputElement::EnsurePrimaryContent() {
  input_type_view_->EnsurePrimaryContent();
}

bool HTMLInputElement::HasFallbackContent() const {
  return input_type_view_->HasFallbackContent();
}

void HTMLInputElement::SetFilesFromPaths(const Vector<String>& paths) {
  return input_type_->SetFilesFromPaths(paths);
}

void HTMLInputElement::ChildrenChanged(const ChildrenChange& change) {
  // Some input types only need shadow roots to hide any children that may
  // have been appended by script. For such types, shadow roots are lazily
  // created when children are added for the first time.
  EnsureUserAgentShadowRoot();
  ContainerNode::ChildrenChanged(change);
}

}  // namespace blink
