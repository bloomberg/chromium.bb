/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2009, 2010, 2011, 2012 Google Inc. All rights reserved.
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

#include "config.h"
#include "core/html/InputTypeUI.h"

#include "HTMLNames.h"
#include "RuntimeEnabledFeatures.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ExceptionStatePlaceholder.h"
#include "core/accessibility/AXObjectCache.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/KeyboardEvent.h"
#include "core/dom/NodeRenderStyle.h"
#include "core/dom/ScopedEventQueue.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/fileapi/FileList.h"
#include "core/html/ButtonInputType.h"
#include "core/html/CheckboxInputType.h"
#include "core/html/ColorInputType.h"
#include "core/html/DateInputType.h"
#include "core/html/DateTimeLocalInputType.h"
#include "core/html/EmailInputType.h"
#include "core/html/FileInputType.h"
#include "core/html/FormController.h"
#include "core/html/FormDataList.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLInputElement.h"
#include "core/html/HiddenInputType.h"
#include "core/html/ImageInputType.h"
#include "core/html/InputTypeNames.h"
#include "core/html/MonthInputType.h"
#include "core/html/NumberInputType.h"
#include "core/html/PasswordInputType.h"
#include "core/html/RadioInputType.h"
#include "core/html/RangeInputType.h"
#include "core/html/ResetInputType.h"
#include "core/html/SearchInputType.h"
#include "core/html/SubmitInputType.h"
#include "core/html/TelephoneInputType.h"
#include "core/html/TextInputType.h"
#include "core/html/TimeInputType.h"
#include "core/html/URLInputType.h"
#include "core/html/WeekInputType.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html/shadow/HTMLShadowElement.h"
#include "core/page/Page.h"
#include "core/platform/DateComponents.h"
#include "core/platform/LocalizedStrings.h"
#include "core/platform/text/TextBreakIterator.h"
#include "core/rendering/RenderObject.h"
#include "core/rendering/RenderTheme.h"
#include "wtf/Assertions.h"
#include "wtf/HashMap.h"
#include "wtf/text/StringHash.h"
#include <limits>

namespace WebCore {

using namespace HTMLNames;
using namespace std;

typedef PassOwnPtr<InputType> (*InputTypeFactoryFunction)(HTMLInputElement*);
typedef HashMap<AtomicString, InputTypeFactoryFunction, CaseFoldingHash> InputTypeFactoryMap;

static PassOwnPtr<InputTypeFactoryMap> createInputTypeFactoryMap()
{
    OwnPtr<InputTypeFactoryMap> map = adoptPtr(new InputTypeFactoryMap);
    map->add(InputTypeNames::button(), ButtonInputType::create);
    map->add(InputTypeNames::checkbox(), CheckboxInputType::create);
    if (RuntimeEnabledFeatures::inputTypeColorEnabled())
        map->add(InputTypeNames::color(), ColorInputType::create);
    map->add(InputTypeNames::date(), DateInputType::create);
    map->add(InputTypeNames::datetimelocal(), DateTimeLocalInputType::create);
    map->add(InputTypeNames::email(), EmailInputType::create);
    map->add(InputTypeNames::file(), FileInputType::create);
    map->add(InputTypeNames::hidden(), HiddenInputType::create);
    map->add(InputTypeNames::image(), ImageInputType::create);
    map->add(InputTypeNames::month(), MonthInputType::create);
    map->add(InputTypeNames::number(), NumberInputType::create);
    map->add(InputTypeNames::password(), PasswordInputType::create);
    map->add(InputTypeNames::radio(), RadioInputType::create);
    map->add(InputTypeNames::range(), RangeInputType::create);
    map->add(InputTypeNames::reset(), ResetInputType::create);
    map->add(InputTypeNames::search(), SearchInputType::create);
    map->add(InputTypeNames::submit(), SubmitInputType::create);
    map->add(InputTypeNames::telephone(), TelephoneInputType::create);
    map->add(InputTypeNames::time(), TimeInputType::create);
    map->add(InputTypeNames::url(), URLInputType::create);
    if (RuntimeEnabledFeatures::inputTypeWeekEnabled())
        map->add(InputTypeNames::week(), WeekInputType::create);
    // No need to register "text" because it is the default type.
    return map.release();
}

PassOwnPtr<InputType> InputType::create(HTMLInputElement* element, const AtomicString& typeName)
{
    static const InputTypeFactoryMap* factoryMap = createInputTypeFactoryMap().leakPtr();
    InputTypeFactoryFunction factory = typeName.isEmpty() ? 0 : factoryMap->get(typeName);
    if (!factory)
        factory = TextInputType::create;
    return factory(element);
}

PassOwnPtr<InputType> InputType::createText(HTMLInputElement* element)
{
    return TextInputType::create(element);
}

InputTypeUI::~InputTypeUI()
{
}

bool InputTypeUI::themeSupportsDataListUI(InputType* type)
{
    Document* document = type->element()->document();
    RefPtr<RenderTheme> theme = document->page() ? document->page()->theme() : RenderTheme::defaultTheme();
    return theme->supportsDataListUI(type->formControlType());
}

bool InputTypeUI::isTextField() const
{
    return false;
}

bool InputTypeUI::isTextType() const
{
    return false;
}

bool InputTypeUI::isRangeControl() const
{
    return false;
}

bool InputTypeUI::shouldSaveAndRestoreFormControlState() const
{
    return true;
}

FormControlState InputTypeUI::saveFormControlState() const
{
    String currentValue = element()->value();
    if (currentValue == element()->defaultValue())
        return FormControlState();
    return FormControlState(currentValue);
}

void InputTypeUI::restoreFormControlState(const FormControlState& state)
{
    element()->setValue(state[0]);
}

bool InputTypeUI::isFormDataAppendable() const
{
    // There is no form data unless there's a name for non-image types.
    return !element()->name().isEmpty();
}

bool InputTypeUI::appendFormData(FormDataList& encoding, bool) const
{
    // Always successful.
    encoding.appendData(element()->name(), element()->value());
    return true;
}

double InputTypeUI::valueAsDate() const
{
    return DateComponents::invalidMilliseconds();
}

void InputTypeUI::setValueAsDate(double, ExceptionState& es) const
{
    es.throwDOMException(InvalidStateError);
}

double InputTypeUI::valueAsDouble() const
{
    return numeric_limits<double>::quiet_NaN();
}

void InputTypeUI::setValueAsDouble(double doubleValue, TextFieldEventBehavior eventBehavior, ExceptionState& es) const
{
    setValueAsDecimal(Decimal::fromDouble(doubleValue), eventBehavior, es);
}

void InputTypeUI::setValueAsDecimal(const Decimal&, TextFieldEventBehavior, ExceptionState& es) const
{
    es.throwDOMException(InvalidStateError);
}

bool InputTypeUI::supportsValidation() const
{
    return true;
}

bool InputTypeUI::typeMismatchFor(const String&) const
{
    return false;
}

bool InputTypeUI::typeMismatch() const
{
    return false;
}

bool InputTypeUI::supportsRequired() const
{
    // Almost all validatable types support @required.
    return supportsValidation();
}

bool InputTypeUI::valueMissing(const String&) const
{
    return false;
}

bool InputTypeUI::hasBadInput() const
{
    return false;
}

bool InputTypeUI::patternMismatch(const String&) const
{
    return false;
}

bool InputTypeUI::rangeUnderflow(const String& value) const
{
    if (!isSteppable())
        return false;

    const Decimal numericValue = parseToNumberOrNaN(value);
    if (!numericValue.isFinite())
        return false;

    return numericValue < createStepRange(RejectAny).minimum();
}

bool InputTypeUI::rangeOverflow(const String& value) const
{
    if (!isSteppable())
        return false;

    const Decimal numericValue = parseToNumberOrNaN(value);
    if (!numericValue.isFinite())
        return false;

    return numericValue > createStepRange(RejectAny).maximum();
}

Decimal InputTypeUI::defaultValueForStepUp() const
{
    return 0;
}

double InputTypeUI::minimum() const
{
    return createStepRange(RejectAny).minimum().toDouble();
}

double InputTypeUI::maximum() const
{
    return createStepRange(RejectAny).maximum().toDouble();
}

bool InputTypeUI::sizeShouldIncludeDecoration(int, int& preferredSize) const
{
    preferredSize = element()->size();
    return false;
}

bool InputTypeUI::isInRange(const String& value) const
{
    if (!isSteppable())
        return false;

    const Decimal numericValue = parseToNumberOrNaN(value);
    if (!numericValue.isFinite())
        return true;

    StepRange stepRange(createStepRange(RejectAny));
    return numericValue >= stepRange.minimum() && numericValue <= stepRange.maximum();
}

bool InputTypeUI::isOutOfRange(const String& value) const
{
    if (!isSteppable())
        return false;

    const Decimal numericValue = parseToNumberOrNaN(value);
    if (!numericValue.isFinite())
        return true;

    StepRange stepRange(createStepRange(RejectAny));
    return numericValue < stepRange.minimum() || numericValue > stepRange.maximum();
}

bool InputTypeUI::stepMismatch(const String& value) const
{
    if (!isSteppable())
        return false;

    const Decimal numericValue = parseToNumberOrNaN(value);
    if (!numericValue.isFinite())
        return false;

    return createStepRange(RejectAny).stepMismatch(numericValue);
}

String InputTypeUI::badInputText() const
{
    ASSERT_NOT_REACHED();
    return validationMessageTypeMismatchText();
}

String InputTypeUI::typeMismatchText() const
{
    return validationMessageTypeMismatchText();
}

String InputTypeUI::valueMissingText() const
{
    return validationMessageValueMissingText();
}

String InputTypeUI::validationMessage() const
{
    const String value = element()->value();

    // The order of the following checks is meaningful. e.g. We'd like to show the
    // badInput message even if the control has other validation errors.
    if (hasBadInput())
        return badInputText();

    if (valueMissing(value))
        return valueMissingText();

    if (typeMismatch())
        return typeMismatchText();

    if (patternMismatch(value))
        return validationMessagePatternMismatchText();

    if (element()->tooLong())
        return validationMessageTooLongText(numGraphemeClusters(value), element()->maxLength());

    if (!isSteppable())
        return emptyString();

    const Decimal numericValue = parseToNumberOrNaN(value);
    if (!numericValue.isFinite())
        return emptyString();

    StepRange stepRange(createStepRange(RejectAny));

    if (numericValue < stepRange.minimum())
        return validationMessageRangeUnderflowText(serialize(stepRange.minimum()));

    if (numericValue > stepRange.maximum())
        return validationMessageRangeOverflowText(serialize(stepRange.maximum()));

    if (stepRange.stepMismatch(numericValue)) {
        const String stepString = stepRange.hasStep() ? serializeForNumberType(stepRange.step() / stepRange.stepScaleFactor()) : emptyString();
        return validationMessageStepMismatchText(serialize(stepRange.stepBase()), stepString);
    }

    return emptyString();
}

void InputTypeUI::handleClickEvent(MouseEvent*)
{
}

void InputTypeUI::handleMouseDownEvent(MouseEvent*)
{
}

void InputTypeUI::handleDOMActivateEvent(Event*)
{
}

void InputTypeUI::handleKeydownEvent(KeyboardEvent*)
{
}

void InputTypeUI::handleKeypressEvent(KeyboardEvent*)
{
}

void InputTypeUI::handleKeyupEvent(KeyboardEvent*)
{
}

void InputTypeUI::handleBeforeTextInsertedEvent(BeforeTextInsertedEvent*)
{
}

void InputTypeUI::handleTouchEvent(TouchEvent*)
{
}

void InputTypeUI::forwardEvent(Event*)
{
}

bool InputTypeUI::shouldSubmitImplicitly(Event* event)
{
    return event->isKeyboardEvent() && event->type() == eventNames().keypressEvent && toKeyboardEvent(event)->charCode() == '\r';
}

PassRefPtr<HTMLFormElement> InputTypeUI::formForSubmission() const
{
    return element()->form();
}

RenderObject* InputTypeUI::createRenderer(RenderStyle* style) const
{
    return RenderObject::createObject(element(), style);
}

PassRefPtr<RenderStyle> InputTypeUI::customStyleForRenderer(PassRefPtr<RenderStyle> originalStyle)
{
    return originalStyle;
}

void InputTypeUI::blur()
{
    element()->defaultBlur();
}

void InputTypeUI::createShadowSubtree()
{
}

void InputTypeUI::destroyShadowSubtree()
{
    ShadowRoot* root = element()->userAgentShadowRoot();
    if (!root)
        return;

    root->removeChildren();

    // It's ok to clear contents of all other ShadowRoots because they must have
    // been created by InputFieldPasswordGeneratorButtonElement, and we don't allow adding
    // AuthorShadowRoot to HTMLInputElement.
    // FIXME: Remove the PasswordGeneratorButtonElement's shadow root and then remove this loop.
    while ((root = root->youngerShadowRoot())) {
        root->removeChildren();
        root->appendChild(HTMLShadowElement::create(shadowTag, element()->document()));
    }
}

Decimal InputTypeUI::parseToNumber(const String&, const Decimal& defaultValue) const
{
    ASSERT_NOT_REACHED();
    return defaultValue;
}

Decimal InputTypeUI::parseToNumberOrNaN(const String& string) const
{
    return parseToNumber(string, Decimal::nan());
}

bool InputTypeUI::parseToDateComponents(const String&, DateComponents*) const
{
    ASSERT_NOT_REACHED();
    return false;
}

String InputTypeUI::serialize(const Decimal&) const
{
    ASSERT_NOT_REACHED();
    return String();
}

void InputTypeUI::dispatchSimulatedClickIfActive(KeyboardEvent* event) const
{
    if (element()->active())
        element()->dispatchSimulatedClick(event);
    event->setDefaultHandled();
}

Chrome* InputTypeUI::chrome() const
{
    if (Page* page = element()->document()->page())
        return &page->chrome();
    return 0;
}

bool InputTypeUI::canSetStringValue() const
{
    return true;
}

bool InputTypeUI::hasCustomFocusLogic() const
{
    return true;
}

bool InputTypeUI::isKeyboardFocusable() const
{
    return element()->isFocusable();
}

bool InputTypeUI::shouldShowFocusRingOnMouseFocus() const
{
    return false;
}

bool InputTypeUI::shouldUseInputMethod() const
{
    return false;
}

void InputTypeUI::handleFocusEvent(Element*, FocusDirection)
{
}

void InputTypeUI::handleBlurEvent()
{
}

void InputTypeUI::enableSecureTextInput()
{
}

void InputTypeUI::disableSecureTextInput()
{
}

void InputTypeUI::accessKeyAction(bool)
{
    element()->focus(false);
}

void InputTypeUI::attach()
{
}

void InputTypeUI::detach()
{
}

void InputTypeUI::countUsage()
{
}

void InputTypeUI::altAttributeChanged()
{
}

void InputTypeUI::srcAttributeChanged()
{
}

bool InputTypeUI::shouldRespectAlignAttribute()
{
    return false;
}

bool InputTypeUI::canChangeFromAnotherType() const
{
    return true;
}

void InputTypeUI::minOrMaxAttributeChanged()
{
}

void InputTypeUI::sanitizeValueInResponseToMinOrMaxAttributeChange()
{
}

void InputTypeUI::stepAttributeChanged()
{
}

bool InputTypeUI::canBeSuccessfulSubmitButton()
{
    return false;
}

HTMLElement* InputTypeUI::placeholderElement() const
{
    return 0;
}

bool InputTypeUI::rendererIsNeeded()
{
    return true;
}

FileList* InputTypeUI::files()
{
    return 0;
}

void InputTypeUI::setFiles(PassRefPtr<FileList>)
{
}

bool InputTypeUI::getTypeSpecificValue(String&)
{
    return false;
}

String InputTypeUI::fallbackValue() const
{
    return String();
}

String InputTypeUI::defaultValue() const
{
    return String();
}

bool InputTypeUI::canSetSuggestedValue()
{
    return false;
}

bool InputTypeUI::shouldSendChangeEventAfterCheckedChanged()
{
    return true;
}

bool InputTypeUI::storesValueSeparateFromAttribute()
{
    return true;
}

void InputTypeUI::setValue(const String& sanitizedValue, bool valueChanged, TextFieldEventBehavior eventBehavior)
{
    element()->setValueInternal(sanitizedValue, eventBehavior);
    element()->setNeedsStyleRecalc();
    if (!valueChanged)
        return;
    switch (eventBehavior) {
    case DispatchChangeEvent:
        element()->dispatchFormControlChangeEvent();
        break;
    case DispatchInputAndChangeEvent:
        element()->dispatchFormControlInputEvent();
        element()->dispatchFormControlChangeEvent();
        break;
    case DispatchNoEvent:
        break;
    }
}

bool InputTypeUI::canSetValue(const String&)
{
    return true;
}

PassOwnPtr<ClickHandlingState> InputTypeUI::willDispatchClick()
{
    return nullptr;
}

void InputTypeUI::didDispatchClick(Event*, const ClickHandlingState&)
{
}

String InputTypeUI::localizeValue(const String& proposedValue) const
{
    return proposedValue;
}

String InputTypeUI::visibleValue() const
{
    return element()->value();
}

String InputTypeUI::sanitizeValue(const String& proposedValue) const
{
    return proposedValue;
}

bool InputTypeUI::receiveDroppedFiles(const DragData*)
{
    ASSERT_NOT_REACHED();
    return false;
}

String InputTypeUI::droppedFileSystemId()
{
    ASSERT_NOT_REACHED();
    return String();
}

bool InputTypeUI::shouldResetOnDocumentActivation()
{
    return false;
}

bool InputTypeUI::shouldRespectListAttribute()
{
    return false;
}

bool InputTypeUI::shouldRespectSpeechAttribute()
{
    return false;
}

bool InputTypeUI::isTextButton() const
{
    return false;
}

bool InputTypeUI::isRadioButton() const
{
    return false;
}

bool InputTypeUI::isSearchField() const
{
    return false;
}

bool InputTypeUI::isHiddenType() const
{
    return false;
}

bool InputTypeUI::isPasswordField() const
{
    return false;
}

bool InputTypeUI::isCheckbox() const
{
    return false;
}

bool InputTypeUI::isEmailField() const
{
    return false;
}

bool InputTypeUI::isFileUpload() const
{
    return false;
}

bool InputTypeUI::isImageButton() const
{
    return false;
}

bool InputTypeUI::supportLabels() const
{
    return true;
}

bool InputTypeUI::isNumberField() const
{
    return false;
}

bool InputTypeUI::isSubmitButton() const
{
    return false;
}

bool InputTypeUI::isTelephoneField() const
{
    return false;
}

bool InputTypeUI::isURLField() const
{
    return false;
}

bool InputTypeUI::isDateField() const
{
    return false;
}

bool InputTypeUI::isDateTimeLocalField() const
{
    return false;
}

bool InputTypeUI::isMonthField() const
{
    return false;
}

bool InputTypeUI::isTimeField() const
{
    return false;
}

bool InputTypeUI::isWeekField() const
{
    return false;
}

bool InputTypeUI::isEnumeratable()
{
    return true;
}

bool InputTypeUI::isCheckable()
{
    return false;
}

bool InputTypeUI::isSteppable() const
{
    return false;
}

bool InputTypeUI::isColorControl() const
{
    return false;
}

bool InputTypeUI::shouldRespectHeightAndWidthAttributes()
{
    return false;
}

bool InputTypeUI::supportsPlaceholder() const
{
    return false;
}

bool InputTypeUI::supportsReadOnly() const
{
    return false;
}

void InputTypeUI::updateInnerTextValue()
{
}

void InputTypeUI::updatePlaceholderText()
{
}

void InputTypeUI::attributeChanged()
{
}

void InputTypeUI::multipleAttributeChanged()
{
}

void InputTypeUI::disabledAttributeChanged()
{
}

void InputTypeUI::readonlyAttributeChanged()
{
}

void InputTypeUI::requiredAttributeChanged()
{
}

void InputTypeUI::valueAttributeChanged()
{
}

void InputTypeUI::subtreeHasChanged()
{
    ASSERT_NOT_REACHED();
}

bool InputTypeUI::hasTouchEventHandler() const
{
    return false;
}

String InputTypeUI::defaultToolTip() const
{
    return String();
}

void InputTypeUI::listAttributeTargetChanged()
{
}

Decimal InputTypeUI::findClosestTickMarkValue(const Decimal&)
{
    ASSERT_NOT_REACHED();
    return Decimal::nan();
}

void InputTypeUI::updateClearButtonVisibility()
{
}

bool InputTypeUI::supportsIndeterminateAppearance() const
{
    return false;
}

bool InputTypeUI::supportsInputModeAttribute() const
{
    return false;
}

bool InputTypeUI::supportsSelectionAPI() const
{
    return false;
}

unsigned InputTypeUI::height() const
{
    return 0;
}

unsigned InputTypeUI::width() const
{
    return 0;
}

void InputTypeUI::applyStep(int count, AnyStepHandling anyStepHandling, TextFieldEventBehavior eventBehavior, ExceptionState& es)
{
    StepRange stepRange(createStepRange(anyStepHandling));
    if (!stepRange.hasStep()) {
        es.throwDOMException(InvalidStateError);
        return;
    }

    const Decimal current = parseToNumberOrNaN(element()->value());
    if (!current.isFinite()) {
        es.throwDOMException(InvalidStateError);
        return;
    }
    Decimal newValue = current + stepRange.step() * count;
    if (!newValue.isFinite()) {
        es.throwDOMException(InvalidStateError);
        return;
    }

    const Decimal acceptableErrorValue = stepRange.acceptableError();
    if (newValue - stepRange.minimum() < -acceptableErrorValue) {
        es.throwDOMException(InvalidStateError);
        return;
    }
    if (newValue < stepRange.minimum())
        newValue = stepRange.minimum();

    const AtomicString& stepString = element()->fastGetAttribute(stepAttr);
    if (!equalIgnoringCase(stepString, "any"))
        newValue = stepRange.alignValueForStep(current, newValue);

    if (newValue - stepRange.maximum() > acceptableErrorValue) {
        es.throwDOMException(InvalidStateError);
        return;
    }
    if (newValue > stepRange.maximum())
        newValue = stepRange.maximum();

    setValueAsDecimal(newValue, eventBehavior, es);

    if (AXObjectCache* cache = element()->document()->existingAXObjectCache())
        cache->postNotification(element(), AXObjectCache::AXValueChanged, true);
}

bool InputTypeUI::getAllowedValueStep(Decimal* step) const
{
    StepRange stepRange(createStepRange(RejectAny));
    *step = stepRange.step();
    return stepRange.hasStep();
}

StepRange InputTypeUI::createStepRange(AnyStepHandling) const
{
    ASSERT_NOT_REACHED();
    return StepRange();
}

void InputTypeUI::stepUp(int n, ExceptionState& es)
{
    if (!isSteppable()) {
        es.throwDOMException(InvalidStateError);
        return;
    }
    applyStep(n, RejectAny, DispatchNoEvent, es);
}

void InputTypeUI::stepUpFromRenderer(int n)
{
    // The differences from stepUp()/stepDown():
    //
    // Difference 1: the current value
    // If the current value is not a number, including empty, the current value is assumed as 0.
    //   * If 0 is in-range, and matches to step value
    //     - The value should be the +step if n > 0
    //     - The value should be the -step if n < 0
    //     If -step or +step is out of range, new value should be 0.
    //   * If 0 is smaller than the minimum value
    //     - The value should be the minimum value for any n
    //   * If 0 is larger than the maximum value
    //     - The value should be the maximum value for any n
    //   * If 0 is in-range, but not matched to step value
    //     - The value should be the larger matched value nearest to 0 if n > 0
    //       e.g. <input type=number min=-100 step=3> -> 2
    //     - The value should be the smaler matched value nearest to 0 if n < 0
    //       e.g. <input type=number min=-100 step=3> -> -1
    //   As for date/datetime-local/month/time/week types, the current value is assumed as "the current local date/time".
    //   As for datetime type, the current value is assumed as "the current date/time in UTC".
    // If the current value is smaller than the minimum value:
    //  - The value should be the minimum value if n > 0
    //  - Nothing should happen if n < 0
    // If the current value is larger than the maximum value:
    //  - The value should be the maximum value if n < 0
    //  - Nothing should happen if n > 0
    //
    // Difference 2: clamping steps
    // If the current value is not matched to step value:
    // - The value should be the larger matched value nearest to 0 if n > 0
    //   e.g. <input type=number value=3 min=-100 step=3> -> 5
    // - The value should be the smaler matched value nearest to 0 if n < 0
    //   e.g. <input type=number value=3 min=-100 step=3> -> 2
    //
    // n is assumed as -n if step < 0.

    ASSERT(isSteppable());
    if (!isSteppable())
        return;
    ASSERT(n);
    if (!n)
        return;

    StepRange stepRange(createStepRange(AnyIsDefaultStep));

    // FIXME: Not any changes after stepping, even if it is an invalid value, may be better.
    // (e.g. Stepping-up for <input type="number" value="foo" step="any" /> => "foo")
    if (!stepRange.hasStep())
        return;

    EventQueueScope scope;
    const Decimal step = stepRange.step();

    int sign;
    if (step > 0)
        sign = n;
    else if (step < 0)
        sign = -n;
    else
        sign = 0;

    String currentStringValue = element()->value();
    Decimal current = parseToNumberOrNaN(currentStringValue);
    if (!current.isFinite()) {
        current = defaultValueForStepUp();
        const Decimal nextDiff = step * n;
        if (current < stepRange.minimum() - nextDiff)
            current = stepRange.minimum() - nextDiff;
        if (current > stepRange.maximum() - nextDiff)
            current = stepRange.maximum() - nextDiff;
        setValueAsDecimal(current, DispatchNoEvent, IGNORE_EXCEPTION);
    }
    if ((sign > 0 && current < stepRange.minimum()) || (sign < 0 && current > stepRange.maximum())) {
        setValueAsDecimal(sign > 0 ? stepRange.minimum() : stepRange.maximum(), DispatchInputAndChangeEvent, IGNORE_EXCEPTION);
    } else {
        if (stepMismatch(element()->value())) {
            ASSERT(!step.isZero());
            const Decimal base = stepRange.stepBase();
            Decimal newValue;
            if (sign < 0)
                newValue = base + ((current - base) / step).floor() * step;
            else if (sign > 0)
                newValue = base + ((current - base) / step).ceiling() * step;
            else
                newValue = current;

            if (newValue < stepRange.minimum())
                newValue = stepRange.minimum();
            if (newValue > stepRange.maximum())
                newValue = stepRange.maximum();

            setValueAsDecimal(newValue, n == 1 || n == -1 ? DispatchInputAndChangeEvent : DispatchNoEvent, IGNORE_EXCEPTION);
            if (n > 1)
                applyStep(n - 1, AnyIsDefaultStep, DispatchInputAndChangeEvent, IGNORE_EXCEPTION);
            else if (n < -1)
                applyStep(n + 1, AnyIsDefaultStep, DispatchInputAndChangeEvent, IGNORE_EXCEPTION);
        } else {
            applyStep(n, AnyIsDefaultStep, DispatchInputAndChangeEvent, IGNORE_EXCEPTION);
        }
    }
}

void InputTypeUI::observeFeatureIfVisible(UseCounter::Feature feature) const
{
    if (RenderStyle* style = element()->renderStyle()) {
        if (style->visibility() != HIDDEN)
            UseCounter::count(element()->document(), feature);
    }
}

} // namespace WebCore
