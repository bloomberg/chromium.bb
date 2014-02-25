/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2007 Samuel Weinig (sam@webkit.org)
 * Copyright (C) 2010 Google Inc. All rights reserved.
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
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
#include "core/html/HTMLInputElement.h"

#include "CSSPropertyNames.h"
#include "HTMLNames.h"
#include "RuntimeEnabledFeatures.h"
#include "bindings/v8/ExceptionMessages.h"
#include "bindings/v8/ExceptionState.h"
#include "bindings/v8/ScriptEventListener.h"
#include "core/accessibility/AXObjectCache.h"
#include "core/dom/Document.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/IdTargetObserver.h"
#include "core/dom/shadow/ElementShadow.h"
#include "core/dom/shadow/InsertionPoint.h"
#include "core/dom/shadow/ShadowRoot.h"
#include "core/editing/FrameSelection.h"
#include "core/editing/SpellChecker.h"
#include "core/events/BeforeTextInsertedEvent.h"
#include "core/events/KeyboardEvent.h"
#include "core/events/MouseEvent.h"
#include "core/events/ScopedEventQueue.h"
#include "core/events/ThreadLocalEventNames.h"
#include "core/events/TouchEvent.h"
#include "core/fileapi/FileList.h"
#include "core/frame/Frame.h"
#include "core/frame/FrameHost.h"
#include "core/frame/FrameView.h"
#include "core/frame/UseCounter.h"
#include "core/html/HTMLCollection.h"
#include "core/html/HTMLDataListElement.h"
#include "core/html/HTMLFormElement.h"
#include "core/html/HTMLImageLoader.h"
#include "core/html/HTMLOptionElement.h"
#include "core/html/forms/ColorInputType.h"
#include "core/html/forms/FileInputType.h"
#include "core/html/forms/FormController.h"
#include "core/html/forms/InputType.h"
#include "core/html/forms/SearchInputType.h"
#include "core/html/parser/HTMLParserIdioms.h"
#include "core/html/shadow/ShadowElementNames.h"
#include "core/page/Chrome.h"
#include "core/page/ChromeClient.h"
#include "core/rendering/RenderTextControlSingleLine.h"
#include "core/rendering/RenderTheme.h"
#include "platform/DateTimeChooser.h"
#include "platform/Language.h"
#include "platform/PlatformMouseEvent.h"
#include "platform/text/PlatformLocale.h"
#include "wtf/MathExtras.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

class ListAttributeTargetObserver : IdTargetObserver {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassOwnPtr<ListAttributeTargetObserver> create(const AtomicString& id, HTMLInputElement*);
    virtual void idTargetChanged() OVERRIDE;

private:
    ListAttributeTargetObserver(const AtomicString& id, HTMLInputElement*);

    HTMLInputElement* m_element;
};

// FIXME: According to HTML4, the length attribute's value can be arbitrarily
// large. However, due to https://bugs.webkit.org/show_bug.cgi?id=14536 things
// get rather sluggish when a text field has a larger number of characters than
// this, even when just clicking in the text field.
const int HTMLInputElement::maximumLength = 524288;
const int defaultSize = 20;
const int maxSavedResults = 256;

HTMLInputElement::HTMLInputElement(Document& document, HTMLFormElement* form, bool createdByParser)
    : HTMLTextFormControlElement(inputTag, document, form)
    , m_size(defaultSize)
    , m_maxLength(maximumLength)
    , m_maxResults(-1)
    , m_isChecked(false)
    , m_reflectsCheckedAttribute(true)
    , m_isIndeterminate(false)
    , m_hasType(false)
    , m_isActivatedSubmit(false)
    , m_autocomplete(Uninitialized)
    , m_hasNonEmptyList(false)
    , m_stateRestored(false)
    , m_parsingInProgress(createdByParser)
    , m_valueAttributeWasUpdatedAfterParsing(false)
    , m_canReceiveDroppedFiles(false)
    , m_hasTouchEventHandler(false)
    , m_shouldRevealPassword(false)
    , m_inputType(InputType::createText(*this))
    , m_inputTypeView(m_inputType)
{
#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
    setHasCustomStyleCallbacks();
#endif
    ScriptWrappable::init(this);
}

PassRefPtr<HTMLInputElement> HTMLInputElement::create(Document& document, HTMLFormElement* form, bool createdByParser)
{
    RefPtr<HTMLInputElement> inputElement = adoptRef(new HTMLInputElement(document, form, createdByParser));
    inputElement->ensureUserAgentShadowRoot();
    return inputElement.release();
}

HTMLImageLoader* HTMLInputElement::imageLoader()
{
    if (!m_imageLoader)
        m_imageLoader = adoptPtr(new HTMLImageLoader(this));
    return m_imageLoader.get();
}

void HTMLInputElement::didAddUserAgentShadowRoot(ShadowRoot&)
{
    m_inputTypeView->createShadowSubtree();
}

void HTMLInputElement::didAddShadowRoot(ShadowRoot& root)
{
    if (!root.isOldestAuthorShadowRoot())
        return;
    m_inputTypeView->destroyShadowSubtree();
    m_inputTypeView = InputTypeView::create(*this);
    lazyReattachIfAttached();
}

HTMLInputElement::~HTMLInputElement()
{
    // Need to remove form association while this is still an HTMLInputElement
    // so that virtual functions are called correctly.
    setForm(0);
    // setForm(0) may register this to a document-level radio button group.
    // We should unregister it to avoid accessing a deleted object.
    if (isRadioButton())
        document().formController()->radioButtonGroupScope().removeButton(this);
    if (m_hasTouchEventHandler)
        document().didRemoveTouchEventHandler(this);
}

const AtomicString& HTMLInputElement::name() const
{
    return m_name.isNull() ? emptyAtom : m_name;
}

Vector<FileChooserFileInfo> HTMLInputElement::filesFromFileInputFormControlState(const FormControlState& state)
{
    return FileInputType::filesFromFormControlState(state);
}

HTMLElement* HTMLInputElement::passwordGeneratorButtonElement() const
{
    return toHTMLElement(userAgentShadowRoot()->getElementById(ShadowElementNames::passwordGenerator()));
}

bool HTMLInputElement::shouldAutocomplete() const
{
    if (m_autocomplete != Uninitialized)
        return m_autocomplete == On;
    return HTMLTextFormControlElement::shouldAutocomplete();
}

bool HTMLInputElement::isValidValue(const String& value) const
{
    if (!m_inputType->canSetStringValue()) {
        ASSERT_NOT_REACHED();
        return false;
    }
    return !m_inputType->typeMismatchFor(value)
        && !m_inputType->stepMismatch(value)
        && !m_inputType->rangeUnderflow(value)
        && !m_inputType->rangeOverflow(value)
        && !tooLong(value, IgnoreDirtyFlag)
        && !m_inputType->patternMismatch(value)
        && !m_inputType->valueMissing(value);
}

bool HTMLInputElement::tooLong() const
{
    return willValidate() && tooLong(value(), CheckDirtyFlag);
}

bool HTMLInputElement::typeMismatch() const
{
    return willValidate() && m_inputType->typeMismatch();
}

bool HTMLInputElement::valueMissing() const
{
    return willValidate() && m_inputType->valueMissing(value());
}

bool HTMLInputElement::hasBadInput() const
{
    return willValidate() && m_inputType->hasBadInput();
}

bool HTMLInputElement::patternMismatch() const
{
    return willValidate() && m_inputType->patternMismatch(value());
}

bool HTMLInputElement::tooLong(const String& value, NeedsToCheckDirtyFlag check) const
{
    // We use isTextType() instead of supportsMaxLength() because of the
    // 'virtual' overhead.
    if (!isTextType())
        return false;
    int max = maxLength();
    if (max < 0)
        return false;
    if (check == CheckDirtyFlag) {
        // Return false for the default value or a value set by a script even if
        // it is longer than maxLength.
        if (!hasDirtyValue() || !lastChangeWasUserEdit())
            return false;
    }
    return value.length() > static_cast<unsigned>(max);
}

bool HTMLInputElement::rangeUnderflow() const
{
    return willValidate() && m_inputType->rangeUnderflow(value());
}

bool HTMLInputElement::rangeOverflow() const
{
    return willValidate() && m_inputType->rangeOverflow(value());
}

String HTMLInputElement::validationMessage() const
{
    if (!willValidate())
        return String();

    if (customError())
        return customValidationMessage();

    return m_inputType->validationMessage();
}

double HTMLInputElement::minimum() const
{
    return m_inputType->minimum();
}

double HTMLInputElement::maximum() const
{
    return m_inputType->maximum();
}

bool HTMLInputElement::stepMismatch() const
{
    return willValidate() && m_inputType->stepMismatch(value());
}

bool HTMLInputElement::getAllowedValueStep(Decimal* step) const
{
    return m_inputType->getAllowedValueStep(step);
}

StepRange HTMLInputElement::createStepRange(AnyStepHandling anyStepHandling) const
{
    return m_inputType->createStepRange(anyStepHandling);
}

Decimal HTMLInputElement::findClosestTickMarkValue(const Decimal& value)
{
    return m_inputType->findClosestTickMarkValue(value);
}

void HTMLInputElement::stepUp(int n, ExceptionState& exceptionState)
{
    m_inputType->stepUp(n, exceptionState);
}

void HTMLInputElement::stepDown(int n, ExceptionState& exceptionState)
{
    m_inputType->stepUp(-n, exceptionState);
}

void HTMLInputElement::blur()
{
    m_inputTypeView->blur();
}

void HTMLInputElement::defaultBlur()
{
    HTMLTextFormControlElement::blur();
}

bool HTMLInputElement::hasCustomFocusLogic() const
{
    return m_inputTypeView->hasCustomFocusLogic();
}

bool HTMLInputElement::isKeyboardFocusable() const
{
    return m_inputType->isKeyboardFocusable();
}

bool HTMLInputElement::shouldShowFocusRingOnMouseFocus() const
{
    return m_inputType->shouldShowFocusRingOnMouseFocus();
}

void HTMLInputElement::updateFocusAppearance(bool restorePreviousSelection)
{
    if (isTextField()) {
        if (!restorePreviousSelection || !hasCachedSelection())
            select();
        else
            restoreCachedSelection();
        if (document().frame())
            document().frame()->selection().revealSelection();
    } else
        HTMLTextFormControlElement::updateFocusAppearance(restorePreviousSelection);
}

void HTMLInputElement::beginEditing()
{
    ASSERT(document().isActive());
    if (!document().isActive())
        return;

    if (!isTextField())
        return;

    document().frame()->spellChecker().didBeginEditing(this);
}

void HTMLInputElement::endEditing()
{
    ASSERT(document().isActive());
    if (!document().isActive())
        return;

    if (!isTextField())
        return;

    Frame* frame = document().frame();
    frame->spellChecker().didEndEditingOnTextField(this);
    frame->host()->chrome().client().didEndEditingOnTextField(*this);
}

bool HTMLInputElement::shouldUseInputMethod()
{
    return m_inputType->shouldUseInputMethod();
}

void HTMLInputElement::handleFocusEvent(Element* oldFocusedElement, FocusType type)
{
    m_inputTypeView->handleFocusEvent(oldFocusedElement, type);
    m_inputType->enableSecureTextInput();
}

void HTMLInputElement::handleBlurEvent()
{
    m_inputType->disableSecureTextInput();
    m_inputTypeView->handleBlurEvent();
}

void HTMLInputElement::setType(const AtomicString& type)
{
    setAttribute(typeAttr, type);
}

void HTMLInputElement::updateType()
{
    const AtomicString& newTypeName = InputType::normalizeTypeName(fastGetAttribute(typeAttr));
    bool hadType = m_hasType;
    m_hasType = true;
    if (m_inputType->formControlType() == newTypeName)
        return;

    if (hadType && !InputType::canChangeFromAnotherType(newTypeName)) {
        // Set the attribute back to the old value.
        // Useful in case we were called from inside parseAttribute.
        setAttribute(typeAttr, type());
        return;
    }

    RefPtr<InputType> newType = InputType::create(*this, newTypeName);
    removeFromRadioButtonGroup();

    bool didStoreValue = m_inputType->storesValueSeparateFromAttribute();
    bool didRespectHeightAndWidth = m_inputType->shouldRespectHeightAndWidthAttributes();

    m_inputTypeView->destroyShadowSubtree();
    lazyReattachIfAttached();

    m_inputType = newType.release();
    if (hasAuthorShadowRoot())
        m_inputTypeView = InputTypeView::create(*this);
    else
        m_inputTypeView = m_inputType;
    m_inputTypeView->createShadowSubtree();

    bool hasTouchEventHandler = m_inputTypeView->hasTouchEventHandler();
    if (hasTouchEventHandler != m_hasTouchEventHandler) {
        if (hasTouchEventHandler)
            document().didAddTouchEventHandler(this);
        else
            document().didRemoveTouchEventHandler(this);
        m_hasTouchEventHandler = hasTouchEventHandler;
    }

    setNeedsWillValidateCheck();

    bool willStoreValue = m_inputType->storesValueSeparateFromAttribute();

    if (didStoreValue && !willStoreValue && hasDirtyValue()) {
        setAttribute(valueAttr, AtomicString(m_valueIfDirty));
        m_valueIfDirty = String();
    }
    if (!didStoreValue && willStoreValue) {
        AtomicString valueString = fastGetAttribute(valueAttr);
        m_valueIfDirty = sanitizeValue(valueString);
    } else
        updateValueIfNeeded();

    setFormControlValueMatchesRenderer(false);
    m_inputTypeView->updateView();

    if (didRespectHeightAndWidth != m_inputType->shouldRespectHeightAndWidthAttributes()) {
        ASSERT(elementData());
        if (const Attribute* height = getAttributeItem(heightAttr))
            attributeChanged(heightAttr, height->value());
        if (const Attribute* width = getAttributeItem(widthAttr))
            attributeChanged(widthAttr, width->value());
        if (const Attribute* align = getAttributeItem(alignAttr))
            attributeChanged(alignAttr, align->value());
    }

    if (document().focusedElement() == this)
        document().updateFocusAppearanceSoon(true /* restore selection */);

    setChangedSinceLastFormControlChangeEvent(false);

    addToRadioButtonGroup();

    setNeedsValidityCheck();
    notifyFormStateChanged();
}

void HTMLInputElement::subtreeHasChanged()
{
    m_inputTypeView->subtreeHasChanged();
    // When typing in an input field, childrenChanged is not called, so we need to force the directionality check.
    calculateAndAdjustDirectionality();
}

const AtomicString& HTMLInputElement::formControlType() const
{
    return m_inputType->formControlType();
}

bool HTMLInputElement::shouldSaveAndRestoreFormControlState() const
{
    if (!m_inputType->shouldSaveAndRestoreFormControlState())
        return false;
    return HTMLTextFormControlElement::shouldSaveAndRestoreFormControlState();
}

FormControlState HTMLInputElement::saveFormControlState() const
{
    return m_inputType->saveFormControlState();
}

void HTMLInputElement::restoreFormControlState(const FormControlState& state)
{
    m_inputType->restoreFormControlState(state);
    m_stateRestored = true;
}

bool HTMLInputElement::canStartSelection() const
{
    if (!isTextField())
        return false;
    return HTMLTextFormControlElement::canStartSelection();
}

int HTMLInputElement::selectionStartForBinding(ExceptionState& exceptionState) const
{
    if (!m_inputType->supportsSelectionAPI()) {
        exceptionState.throwDOMException(InvalidStateError, "The input element's type ('" + m_inputType->formControlType() + "') does not support selection.");
        return 0;
    }
    return HTMLTextFormControlElement::selectionStart();
}

int HTMLInputElement::selectionEndForBinding(ExceptionState& exceptionState) const
{
    if (!m_inputType->supportsSelectionAPI()) {
        exceptionState.throwDOMException(InvalidStateError, "The input element's type ('" + m_inputType->formControlType() + "') does not support selection.");
        return 0;
    }
    return HTMLTextFormControlElement::selectionEnd();
}

String HTMLInputElement::selectionDirectionForBinding(ExceptionState& exceptionState) const
{
    if (!m_inputType->supportsSelectionAPI()) {
        exceptionState.throwDOMException(InvalidStateError, "The input element's type ('" + m_inputType->formControlType() + "') does not support selection.");
        return String();
    }
    return HTMLTextFormControlElement::selectionDirection();
}

void HTMLInputElement::setSelectionStartForBinding(int start, ExceptionState& exceptionState)
{
    if (!m_inputType->supportsSelectionAPI()) {
        exceptionState.throwDOMException(InvalidStateError, "The input element's type ('" + m_inputType->formControlType() + "') does not support selection.");
        return;
    }
    HTMLTextFormControlElement::setSelectionStart(start);
}

void HTMLInputElement::setSelectionEndForBinding(int end, ExceptionState& exceptionState)
{
    if (!m_inputType->supportsSelectionAPI()) {
        exceptionState.throwDOMException(InvalidStateError, "The input element's type ('" + m_inputType->formControlType() + "') does not support selection.");
        return;
    }
    HTMLTextFormControlElement::setSelectionEnd(end);
}

void HTMLInputElement::setSelectionDirectionForBinding(const String& direction, ExceptionState& exceptionState)
{
    if (!m_inputType->supportsSelectionAPI()) {
        exceptionState.throwDOMException(InvalidStateError, "The input element's type ('" + m_inputType->formControlType() + "') does not support selection.");
        return;
    }
    HTMLTextFormControlElement::setSelectionDirection(direction);
}

void HTMLInputElement::setSelectionRangeForBinding(int start, int end, ExceptionState& exceptionState)
{
    if (!m_inputType->supportsSelectionAPI()) {
        exceptionState.throwDOMException(InvalidStateError, "The input element's type ('" + m_inputType->formControlType() + "') does not support selection.");
        return;
    }
    HTMLTextFormControlElement::setSelectionRange(start, end);
}

void HTMLInputElement::setSelectionRangeForBinding(int start, int end, const String& direction, ExceptionState& exceptionState)
{
    if (!m_inputType->supportsSelectionAPI()) {
        exceptionState.throwDOMException(InvalidStateError, "The input element's type ('" + m_inputType->formControlType() + "') does not support selection.");
        return;
    }
    HTMLTextFormControlElement::setSelectionRange(start, end, direction);
}

void HTMLInputElement::accessKeyAction(bool sendMouseEvents)
{
    m_inputType->accessKeyAction(sendMouseEvents);
}

bool HTMLInputElement::isPresentationAttribute(const QualifiedName& name) const
{
    if (name == vspaceAttr || name == hspaceAttr || name == alignAttr || name == widthAttr || name == heightAttr || (name == borderAttr && isImageButton()))
        return true;
    return HTMLTextFormControlElement::isPresentationAttribute(name);
}

void HTMLInputElement::collectStyleForPresentationAttribute(const QualifiedName& name, const AtomicString& value, MutableStylePropertySet* style)
{
    if (name == vspaceAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginTop, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginBottom, value);
    } else if (name == hspaceAttr) {
        addHTMLLengthToStyle(style, CSSPropertyMarginLeft, value);
        addHTMLLengthToStyle(style, CSSPropertyMarginRight, value);
    } else if (name == alignAttr) {
        if (m_inputType->shouldRespectAlignAttribute())
            applyAlignmentAttributeToStyle(value, style);
    } else if (name == widthAttr) {
        if (m_inputType->shouldRespectHeightAndWidthAttributes())
            addHTMLLengthToStyle(style, CSSPropertyWidth, value);
    } else if (name == heightAttr) {
        if (m_inputType->shouldRespectHeightAndWidthAttributes())
            addHTMLLengthToStyle(style, CSSPropertyHeight, value);
    } else if (name == borderAttr && isImageButton())
        applyBorderAttributeToStyle(value, style);
    else
        HTMLTextFormControlElement::collectStyleForPresentationAttribute(name, value, style);
}

void HTMLInputElement::parseAttribute(const QualifiedName& name, const AtomicString& value)
{
    if (name == nameAttr) {
        removeFromRadioButtonGroup();
        m_name = value;
        addToRadioButtonGroup();
        HTMLTextFormControlElement::parseAttribute(name, value);
    } else if (name == autocompleteAttr) {
        if (equalIgnoringCase(value, "off"))
            m_autocomplete = Off;
        else {
            if (value.isEmpty())
                m_autocomplete = Uninitialized;
            else
                m_autocomplete = On;
        }
    } else if (name == typeAttr)
        updateType();
    else if (name == valueAttr) {
        // We only need to setChanged if the form is looking at the default value right now.
        if (!hasDirtyValue()) {
            updatePlaceholderVisibility(false);
            setNeedsStyleRecalc(SubtreeStyleChange);
        }
        setFormControlValueMatchesRenderer(false);
        setNeedsValidityCheck();
        m_valueAttributeWasUpdatedAfterParsing = !m_parsingInProgress;
        m_inputTypeView->valueAttributeChanged();
    } else if (name == checkedAttr) {
        // Another radio button in the same group might be checked by state
        // restore. We shouldn't call setChecked() even if this has the checked
        // attribute. So, delay the setChecked() call until
        // finishParsingChildren() is called if parsing is in progress.
        if (!m_parsingInProgress && m_reflectsCheckedAttribute) {
            setChecked(!value.isNull());
            m_reflectsCheckedAttribute = true;
        }
    } else if (name == maxlengthAttr)
        parseMaxLengthAttribute(value);
    else if (name == sizeAttr) {
        int oldSize = m_size;
        int valueAsInteger = value.toInt();
        m_size = valueAsInteger > 0 ? valueAsInteger : defaultSize;
        if (m_size != oldSize && renderer())
            renderer()->setNeedsLayoutAndPrefWidthsRecalc();
    } else if (name == altAttr)
        m_inputTypeView->altAttributeChanged();
    else if (name == srcAttr)
        m_inputTypeView->srcAttributeChanged();
    else if (name == usemapAttr || name == accesskeyAttr) {
        // FIXME: ignore for the moment
    } else if (name == onsearchAttr) {
        // Search field and slider attributes all just cause updateFromElement to be called through style recalcing.
        setAttributeEventListener(EventTypeNames::search, createAttributeEventListener(this, name, value));
    } else if (name == resultsAttr) {
        int oldResults = m_maxResults;
        m_maxResults = !value.isNull() ? std::min(value.toInt(), maxSavedResults) : -1;
        // FIXME: Detaching just for maxResults change is not ideal.  We should figure out the right
        // time to relayout for this change.
        if (m_maxResults != oldResults && (m_maxResults <= 0 || oldResults <= 0))
            lazyReattachIfAttached();
        setNeedsStyleRecalc(SubtreeStyleChange);
        UseCounter::count(document(), UseCounter::ResultsAttribute);
    } else if (name == incrementalAttr) {
        setNeedsStyleRecalc(SubtreeStyleChange);
        UseCounter::count(document(), UseCounter::IncrementalAttribute);
    } else if (name == minAttr) {
        m_inputTypeView->minOrMaxAttributeChanged();
        m_inputType->sanitizeValueInResponseToMinOrMaxAttributeChange();
        setNeedsValidityCheck();
        UseCounter::count(document(), UseCounter::MinAttribute);
    } else if (name == maxAttr) {
        m_inputTypeView->minOrMaxAttributeChanged();
        setNeedsValidityCheck();
        UseCounter::count(document(), UseCounter::MaxAttribute);
    } else if (name == multipleAttr) {
        m_inputTypeView->multipleAttributeChanged();
        setNeedsValidityCheck();
    } else if (name == stepAttr) {
        m_inputTypeView->stepAttributeChanged();
        setNeedsValidityCheck();
        UseCounter::count(document(), UseCounter::StepAttribute);
    } else if (name == patternAttr) {
        setNeedsValidityCheck();
        UseCounter::count(document(), UseCounter::PatternAttribute);
    } else if (name == precisionAttr) {
        setNeedsValidityCheck();
        UseCounter::count(document(), UseCounter::PrecisionAttribute);
    } else if (name == disabledAttr) {
        HTMLTextFormControlElement::parseAttribute(name, value);
        m_inputTypeView->disabledAttributeChanged();
    } else if (name == readonlyAttr) {
        HTMLTextFormControlElement::parseAttribute(name, value);
        m_inputTypeView->readonlyAttributeChanged();
    } else if (name == listAttr) {
        m_hasNonEmptyList = !value.isEmpty();
        if (m_hasNonEmptyList) {
            resetListAttributeTargetObserver();
            listAttributeTargetChanged();
        }
        UseCounter::count(document(), UseCounter::ListAttribute);
    }
#if ENABLE(INPUT_SPEECH)
    else if (name == webkitspeechAttr) {
        if (RuntimeEnabledFeatures::speechInputEnabled() && m_inputType->shouldRespectSpeechAttribute()) {
            // This renderer and its children have quite different layouts and
            // styles depending on whether the speech button is visible or
            // not. So we reset the whole thing and recreate to get the right
            // styles and layout.
            m_inputTypeView->destroyShadowSubtree();
            lazyReattachIfAttached();
            m_inputTypeView->createShadowSubtree();
            setFormControlValueMatchesRenderer(false);
        }
        UseCounter::countDeprecation(document(), UseCounter::PrefixedSpeechAttribute);
    } else if (name == onwebkitspeechchangeAttr)
        setAttributeEventListener(EventTypeNames::webkitspeechchange, createAttributeEventListener(this, name, value));
#endif
    else if (name == webkitdirectoryAttr) {
        HTMLTextFormControlElement::parseAttribute(name, value);
        UseCounter::count(document(), UseCounter::PrefixedDirectoryAttribute);
    }
    else
        HTMLTextFormControlElement::parseAttribute(name, value);
    m_inputTypeView->attributeChanged();
}

void HTMLInputElement::finishParsingChildren()
{
    m_parsingInProgress = false;
    HTMLTextFormControlElement::finishParsingChildren();
    if (!m_stateRestored) {
        bool checked = hasAttribute(checkedAttr);
        if (checked)
            setChecked(checked);
        m_reflectsCheckedAttribute = true;
    }
}

bool HTMLInputElement::rendererIsNeeded(const RenderStyle& style)
{
    return m_inputType->rendererIsNeeded() && HTMLTextFormControlElement::rendererIsNeeded(style);
}

RenderObject* HTMLInputElement::createRenderer(RenderStyle* style)
{
    return m_inputTypeView->createRenderer(style);
}

void HTMLInputElement::attach(const AttachContext& context)
{
    if (!m_hasType)
        updateType();

    HTMLTextFormControlElement::attach(context);

    m_inputTypeView->startResourceLoading();
    m_inputType->countUsage();

    if (document().focusedElement() == this)
        document().updateFocusAppearanceSoon(true /* restore selection */);
}

void HTMLInputElement::detach(const AttachContext& context)
{
    HTMLTextFormControlElement::detach(context);
    setFormControlValueMatchesRenderer(false);
    m_inputTypeView->closePopupView();
}

String HTMLInputElement::altText() const
{
    // http://www.w3.org/TR/1998/REC-html40-19980424/appendix/notes.html#altgen
    // also heavily discussed by Hixie on bugzilla
    // note this is intentionally different to HTMLImageElement::altText()
    String alt = fastGetAttribute(altAttr);
    // fall back to title attribute
    if (alt.isNull())
        alt = fastGetAttribute(titleAttr);
    if (alt.isNull())
        alt = fastGetAttribute(valueAttr);
    if (alt.isEmpty())
        alt = locale().queryString(blink::WebLocalizedString::InputElementAltText);
    return alt;
}

bool HTMLInputElement::canBeSuccessfulSubmitButton() const
{
    return m_inputType->canBeSuccessfulSubmitButton();
}

bool HTMLInputElement::isActivatedSubmit() const
{
    return m_isActivatedSubmit;
}

void HTMLInputElement::setActivatedSubmit(bool flag)
{
    m_isActivatedSubmit = flag;
}

bool HTMLInputElement::appendFormData(FormDataList& encoding, bool multipart)
{
    return m_inputType->isFormDataAppendable() && m_inputType->appendFormData(encoding, multipart);
}

String HTMLInputElement::resultForDialogSubmit()
{
    return m_inputType->resultForDialogSubmit();
}

void HTMLInputElement::resetImpl()
{
    if (m_inputType->storesValueSeparateFromAttribute()) {
        setValue(String());
        setNeedsValidityCheck();
    }

    setChecked(hasAttribute(checkedAttr));
    m_reflectsCheckedAttribute = true;
}

bool HTMLInputElement::isTextField() const
{
    return m_inputType->isTextField();
}

bool HTMLInputElement::isTextType() const
{
    return m_inputType->isTextType();
}

void HTMLInputElement::setChecked(bool nowChecked, TextFieldEventBehavior eventBehavior)
{
    if (checked() == nowChecked)
        return;

    m_reflectsCheckedAttribute = false;
    m_isChecked = nowChecked;
    setNeedsStyleRecalc(SubtreeStyleChange);

    if (RadioButtonGroupScope* scope = radioButtonGroupScope())
        scope->updateCheckedState(this);
    if (renderer() && renderer()->style()->hasAppearance())
        RenderTheme::theme().stateChanged(renderer(), CheckedState);

    setNeedsValidityCheck();

    // Ideally we'd do this from the render tree (matching
    // RenderTextView), but it's not possible to do it at the moment
    // because of the way the code is structured.
    if (renderer()) {
        if (AXObjectCache* cache = renderer()->document().existingAXObjectCache())
            cache->checkedStateChanged(this);
    }

    // Only send a change event for items in the document (avoid firing during
    // parsing) and don't send a change event for a radio button that's getting
    // unchecked to match other browsers. DOM is not a useful standard for this
    // because it says only to fire change events at "lose focus" time, which is
    // definitely wrong in practice for these types of elements.
    if (eventBehavior != DispatchNoEvent && inDocument() && m_inputType->shouldSendChangeEventAfterCheckedChanged()) {
        setTextAsOfLastFormControlChangeEvent(String());
        dispatchFormControlChangeEvent();
    }

    didAffectSelector(AffectedSelectorChecked);
}

void HTMLInputElement::setIndeterminate(bool newValue)
{
    if (indeterminate() == newValue)
        return;

    m_isIndeterminate = newValue;

    didAffectSelector(AffectedSelectorIndeterminate);

    if (renderer() && renderer()->style()->hasAppearance())
        RenderTheme::theme().stateChanged(renderer(), CheckedState);
}

int HTMLInputElement::size() const
{
    return m_size;
}

bool HTMLInputElement::sizeShouldIncludeDecoration(int& preferredSize) const
{
    return m_inputTypeView->sizeShouldIncludeDecoration(defaultSize, preferredSize);
}

void HTMLInputElement::copyNonAttributePropertiesFromElement(const Element& source)
{
    const HTMLInputElement& sourceElement = static_cast<const HTMLInputElement&>(source);

    m_valueIfDirty = sourceElement.m_valueIfDirty;
    setChecked(sourceElement.m_isChecked);
    m_reflectsCheckedAttribute = sourceElement.m_reflectsCheckedAttribute;
    m_isIndeterminate = sourceElement.m_isIndeterminate;

    HTMLTextFormControlElement::copyNonAttributePropertiesFromElement(source);

    setFormControlValueMatchesRenderer(false);
    m_inputTypeView->updateView();
}

String HTMLInputElement::value() const
{
    String value;
    if (m_inputType->getTypeSpecificValue(value))
        return value;

    value = m_valueIfDirty;
    if (!value.isNull())
        return value;

    AtomicString valueString = fastGetAttribute(valueAttr);
    value = sanitizeValue(valueString);
    if (!value.isNull())
        return value;

    return m_inputType->fallbackValue();
}

String HTMLInputElement::valueWithDefault() const
{
    String value = this->value();
    if (!value.isNull())
        return value;

    return m_inputType->defaultValue();
}

void HTMLInputElement::setValueForUser(const String& value)
{
    // Call setValue and make it send a change event.
    setValue(value, DispatchChangeEvent);
}

const String& HTMLInputElement::suggestedValue() const
{
    return m_suggestedValue;
}

void HTMLInputElement::setSuggestedValue(const String& value)
{
    if (!m_inputType->canSetSuggestedValue())
        return;
    setFormControlValueMatchesRenderer(false);
    m_suggestedValue = sanitizeValue(value);
    setNeedsStyleRecalc(SubtreeStyleChange);
    m_inputTypeView->updateView();
}

void HTMLInputElement::setEditingValue(const String& value)
{
    if (!renderer() || !isTextField())
        return;
    setInnerTextValue(value);
    subtreeHasChanged();

    unsigned max = value.length();
    if (focused())
        setSelectionRange(max, max);
    else
        cacheSelectionInResponseToSetValue(max);

    dispatchInputEvent();
}

void HTMLInputElement::setValue(const String& value, ExceptionState& exceptionState, TextFieldEventBehavior eventBehavior)
{
    if (isFileUpload() && !value.isEmpty()) {
        exceptionState.throwDOMException(InvalidStateError, "This input element accepts a filename, which may only be programmatically set to the empty string.");
        return;
    }
    setValue(value, eventBehavior);
}

void HTMLInputElement::setValue(const String& value, TextFieldEventBehavior eventBehavior)
{
    if (!m_inputType->canSetValue(value))
        return;

    RefPtr<HTMLInputElement> protector(this);
    EventQueueScope scope;
    String sanitizedValue = sanitizeValue(value);
    bool valueChanged = sanitizedValue != this->value();

    setLastChangeWasNotUserEdit();
    setFormControlValueMatchesRenderer(false);
    m_suggestedValue = String(); // Prevent TextFieldInputType::setValue from using the suggested value.

    m_inputType->setValue(sanitizedValue, valueChanged, eventBehavior);

    if (valueChanged && eventBehavior == DispatchNoEvent)
        setTextAsOfLastFormControlChangeEvent(sanitizedValue);

    if (!valueChanged)
        return;

    notifyFormStateChanged();
}

void HTMLInputElement::setValueInternal(const String& sanitizedValue, TextFieldEventBehavior eventBehavior)
{
    m_valueIfDirty = sanitizedValue;
    setNeedsValidityCheck();
}

void HTMLInputElement::updateView()
{
    m_inputTypeView->updateView();
}

double HTMLInputElement::valueAsDate() const
{
    return m_inputType->valueAsDate();
}

void HTMLInputElement::setValueAsDate(double value, ExceptionState& exceptionState)
{
    m_inputType->setValueAsDate(value, exceptionState);
}

double HTMLInputElement::valueAsNumber() const
{
    return m_inputType->valueAsDouble();
}

void HTMLInputElement::setValueAsNumber(double newValue, ExceptionState& exceptionState, TextFieldEventBehavior eventBehavior)
{
    if (std::isinf(newValue)) {
        exceptionState.throwTypeError(ExceptionMessages::notAFiniteNumber(newValue));
        return;
    }
    m_inputType->setValueAsDouble(newValue, eventBehavior, exceptionState);
}

void HTMLInputElement::setValueFromRenderer(const String& value)
{
    // File upload controls will never use this.
    ASSERT(!isFileUpload());

    m_suggestedValue = String();

    // Renderer and our event handler are responsible for sanitizing values.
    ASSERT(value == sanitizeValue(value) || sanitizeValue(value).isEmpty());

    m_valueIfDirty = value;

    setFormControlValueMatchesRenderer(true);

    // Input event is fired by the Node::defaultEventHandler for editable controls.
    if (!isTextField())
        dispatchInputEvent();
    notifyFormStateChanged();

    setNeedsValidityCheck();

    // Clear autofill flag (and yellow background) on user edit.
    setAutofilled(false);
}

void* HTMLInputElement::preDispatchEventHandler(Event* event)
{
    if (event->type() == EventTypeNames::textInput && m_inputTypeView->shouldSubmitImplicitly(event)) {
        event->stopPropagation();
        return 0;
    }
    if (event->type() != EventTypeNames::click)
        return 0;
    if (!event->isMouseEvent() || toMouseEvent(event)->button() != LeftButton)
        return 0;
    // FIXME: Check whether there are any cases where this actually ends up leaking.
    return m_inputTypeView->willDispatchClick().leakPtr();
}

void HTMLInputElement::postDispatchEventHandler(Event* event, void* dataFromPreDispatch)
{
    OwnPtr<ClickHandlingState> state = adoptPtr(static_cast<ClickHandlingState*>(dataFromPreDispatch));
    if (!state)
        return;
    m_inputTypeView->didDispatchClick(event, *state);
}

void HTMLInputElement::defaultEventHandler(Event* evt)
{
    if (evt->isMouseEvent() && evt->type() == EventTypeNames::click && toMouseEvent(evt)->button() == LeftButton) {
        m_inputTypeView->handleClickEvent(toMouseEvent(evt));
        if (evt->defaultHandled())
            return;
    }

    if (evt->isTouchEvent() && m_inputTypeView->hasTouchEventHandler()) {
        m_inputTypeView->handleTouchEvent(toTouchEvent(evt));
        if (evt->defaultHandled())
            return;
    }

    if (evt->isKeyboardEvent() && evt->type() == EventTypeNames::keydown) {
        m_inputTypeView->handleKeydownEvent(toKeyboardEvent(evt));
        if (evt->defaultHandled())
            return;
    }

    // Call the base event handler before any of our own event handling for almost all events in text fields.
    // Makes editing keyboard handling take precedence over the keydown and keypress handling in this function.
    bool callBaseClassEarly = isTextField() && (evt->type() == EventTypeNames::keydown || evt->type() == EventTypeNames::keypress);
    if (callBaseClassEarly) {
        HTMLTextFormControlElement::defaultEventHandler(evt);
        if (evt->defaultHandled())
            return;
    }

    // DOMActivate events cause the input to be "activated" - in the case of image and submit inputs, this means
    // actually submitting the form. For reset inputs, the form is reset. These events are sent when the user clicks
    // on the element, or presses enter while it is the active element. JavaScript code wishing to activate the element
    // must dispatch a DOMActivate event - a click event will not do the job.
    if (evt->type() == EventTypeNames::DOMActivate) {
        m_inputType->handleDOMActivateEvent(evt);
        if (evt->defaultHandled())
            return;
    }

    // Use key press event here since sending simulated mouse events
    // on key down blocks the proper sending of the key press event.
    if (evt->isKeyboardEvent() && evt->type() == EventTypeNames::keypress) {
        m_inputTypeView->handleKeypressEvent(toKeyboardEvent(evt));
        if (evt->defaultHandled())
            return;
    }

    if (evt->isKeyboardEvent() && evt->type() == EventTypeNames::keyup) {
        m_inputTypeView->handleKeyupEvent(toKeyboardEvent(evt));
        if (evt->defaultHandled())
            return;
    }

    if (m_inputTypeView->shouldSubmitImplicitly(evt)) {
        if (isSearchField())
            onSearch();
        // Form submission finishes editing, just as loss of focus does.
        // If there was a change, send the event now.
        if (wasChangedSinceLastFormControlChangeEvent())
            dispatchFormControlChangeEvent();

        RefPtr<HTMLFormElement> formForSubmission = m_inputTypeView->formForSubmission();
        // Form may never have been present, or may have been destroyed by code responding to the change event.
        if (formForSubmission)
            formForSubmission->submitImplicitly(evt, canTriggerImplicitSubmission());

        evt->setDefaultHandled();
        return;
    }

    if (evt->isBeforeTextInsertedEvent())
        m_inputTypeView->handleBeforeTextInsertedEvent(static_cast<BeforeTextInsertedEvent*>(evt));

    if (evt->isMouseEvent() && evt->type() == EventTypeNames::mousedown) {
        m_inputTypeView->handleMouseDownEvent(toMouseEvent(evt));
        if (evt->defaultHandled())
            return;
    }

    m_inputTypeView->forwardEvent(evt);

    if (!callBaseClassEarly && !evt->defaultHandled())
        HTMLTextFormControlElement::defaultEventHandler(evt);
}

bool HTMLInputElement::willRespondToMouseClickEvents()
{
    // FIXME: Consider implementing willRespondToMouseClickEvents() in InputType if more accurate results are necessary.
    if (!isDisabledFormControl())
        return true;

    return HTMLTextFormControlElement::willRespondToMouseClickEvents();
}

bool HTMLInputElement::isURLAttribute(const Attribute& attribute) const
{
    return attribute.name() == srcAttr || attribute.name() == formactionAttr || HTMLTextFormControlElement::isURLAttribute(attribute);
}

const AtomicString& HTMLInputElement::defaultValue() const
{
    return fastGetAttribute(valueAttr);
}

void HTMLInputElement::setDefaultValue(const AtomicString& value)
{
    setAttribute(valueAttr, value);
}

static inline bool isRFC2616TokenCharacter(UChar ch)
{
    return isASCII(ch) && ch > ' ' && ch != '"' && ch != '(' && ch != ')' && ch != ',' && ch != '/' && (ch < ':' || ch > '@') && (ch < '[' || ch > ']') && ch != '{' && ch != '}' && ch != 0x7f;
}

static bool isValidMIMEType(const String& type)
{
    size_t slashPosition = type.find('/');
    if (slashPosition == kNotFound || !slashPosition || slashPosition == type.length() - 1)
        return false;
    for (size_t i = 0; i < type.length(); ++i) {
        if (!isRFC2616TokenCharacter(type[i]) && i != slashPosition)
            return false;
    }
    return true;
}

static bool isValidFileExtension(const String& type)
{
    if (type.length() < 2)
        return false;
    return type[0] == '.';
}

static Vector<String> parseAcceptAttribute(const String& acceptString, bool (*predicate)(const String&))
{
    Vector<String> types;
    if (acceptString.isEmpty())
        return types;

    Vector<String> splitTypes;
    acceptString.split(',', false, splitTypes);
    for (size_t i = 0; i < splitTypes.size(); ++i) {
        String trimmedType = stripLeadingAndTrailingHTMLSpaces(splitTypes[i]);
        if (trimmedType.isEmpty())
            continue;
        if (!predicate(trimmedType))
            continue;
        types.append(trimmedType.lower());
    }

    return types;
}

Vector<String> HTMLInputElement::acceptMIMETypes()
{
    return parseAcceptAttribute(fastGetAttribute(acceptAttr), isValidMIMEType);
}

Vector<String> HTMLInputElement::acceptFileExtensions()
{
    return parseAcceptAttribute(fastGetAttribute(acceptAttr), isValidFileExtension);
}

const AtomicString& HTMLInputElement::alt() const
{
    return fastGetAttribute(altAttr);
}

int HTMLInputElement::maxLength() const
{
    return m_maxLength;
}

void HTMLInputElement::setMaxLength(int maxLength, ExceptionState& exceptionState)
{
    if (maxLength < 0)
        exceptionState.throwDOMException(IndexSizeError, "The value provided (" + String::number(maxLength) + ") is negative.");
    else
        setIntegralAttribute(maxlengthAttr, maxLength);
}

bool HTMLInputElement::multiple() const
{
    return fastHasAttribute(multipleAttr);
}

void HTMLInputElement::setSize(unsigned size)
{
    setUnsignedIntegralAttribute(sizeAttr, size);
}

void HTMLInputElement::setSize(unsigned size, ExceptionState& exceptionState)
{
    if (!size)
        exceptionState.throwDOMException(IndexSizeError, "The value provided is 0, which is an invalid size.");
    else
        setSize(size);
}

KURL HTMLInputElement::src() const
{
    return document().completeURL(fastGetAttribute(srcAttr));
}

FileList* HTMLInputElement::files()
{
    return m_inputType->files();
}

void HTMLInputElement::setFiles(PassRefPtr<FileList> files)
{
    m_inputType->setFiles(files);
}

bool HTMLInputElement::receiveDroppedFiles(const DragData* dragData)
{
    return m_inputType->receiveDroppedFiles(dragData);
}

String HTMLInputElement::droppedFileSystemId()
{
    return m_inputType->droppedFileSystemId();
}

bool HTMLInputElement::canReceiveDroppedFiles() const
{
    return m_canReceiveDroppedFiles;
}

void HTMLInputElement::setCanReceiveDroppedFiles(bool canReceiveDroppedFiles)
{
    if (m_canReceiveDroppedFiles == canReceiveDroppedFiles)
        return;
    m_canReceiveDroppedFiles = canReceiveDroppedFiles;
    if (renderer())
        renderer()->updateFromElement();
}

String HTMLInputElement::sanitizeValue(const String& proposedValue) const
{
    if (proposedValue.isNull())
        return proposedValue;
    return m_inputType->sanitizeValue(proposedValue);
}

String HTMLInputElement::localizeValue(const String& proposedValue) const
{
    if (proposedValue.isNull())
        return proposedValue;
    return m_inputType->localizeValue(proposedValue);
}

bool HTMLInputElement::isInRange() const
{
    return m_inputType->isInRange(value());
}

bool HTMLInputElement::isOutOfRange() const
{
    return m_inputType->isOutOfRange(value());
}

bool HTMLInputElement::isRequiredFormControl() const
{
    return m_inputType->supportsRequired() && isRequired();
}

bool HTMLInputElement::matchesReadOnlyPseudoClass() const
{
    return m_inputType->supportsReadOnly() && isReadOnly();
}

bool HTMLInputElement::matchesReadWritePseudoClass() const
{
    return m_inputType->supportsReadOnly() && !isReadOnly();
}

void HTMLInputElement::onSearch()
{
    ASSERT(isSearchField());
    if (m_inputType)
        static_cast<SearchInputType*>(m_inputType.get())->stopSearchEventTimer();
    dispatchEvent(Event::createBubble(EventTypeNames::search));
}

void HTMLInputElement::updateClearButtonVisibility()
{
    m_inputTypeView->updateClearButtonVisibility();
}

void HTMLInputElement::willChangeForm()
{
    removeFromRadioButtonGroup();
    HTMLTextFormControlElement::willChangeForm();
}

void HTMLInputElement::didChangeForm()
{
    HTMLTextFormControlElement::didChangeForm();
    addToRadioButtonGroup();
}

Node::InsertionNotificationRequest HTMLInputElement::insertedInto(ContainerNode* insertionPoint)
{
    HTMLTextFormControlElement::insertedInto(insertionPoint);
    if (insertionPoint->inDocument() && !form())
        addToRadioButtonGroup();
    resetListAttributeTargetObserver();
    return InsertionDone;
}

void HTMLInputElement::removedFrom(ContainerNode* insertionPoint)
{
    if (insertionPoint->inDocument() && !form())
        removeFromRadioButtonGroup();
    HTMLTextFormControlElement::removedFrom(insertionPoint);
    ASSERT(!inDocument());
    resetListAttributeTargetObserver();
}

void HTMLInputElement::didMoveToNewDocument(Document& oldDocument)
{
    if (hasImageLoader())
        imageLoader()->elementDidMoveToNewDocument();

    if (isRadioButton())
        oldDocument.formController()->radioButtonGroupScope().removeButton(this);
    if (m_hasTouchEventHandler)
        oldDocument.didRemoveTouchEventHandler(this);

    if (m_hasTouchEventHandler)
        document().didAddTouchEventHandler(this);

    HTMLTextFormControlElement::didMoveToNewDocument(oldDocument);
}

void HTMLInputElement::removeAllEventListeners()
{
    HTMLTextFormControlElement::removeAllEventListeners();
    m_hasTouchEventHandler = false;
}

bool HTMLInputElement::recalcWillValidate() const
{
    return m_inputType->supportsValidation() && HTMLTextFormControlElement::recalcWillValidate();
}

void HTMLInputElement::requiredAttributeChanged()
{
    HTMLTextFormControlElement::requiredAttributeChanged();
    if (RadioButtonGroupScope* scope = radioButtonGroupScope())
        scope->requiredAttributeChanged(this);
    m_inputTypeView->requiredAttributeChanged();
}

void HTMLInputElement::selectColorInColorChooser(const Color& color)
{
    if (!m_inputType->isColorControl())
        return;
    static_cast<ColorInputType*>(m_inputType.get())->didChooseColor(color);
}

HTMLElement* HTMLInputElement::list() const
{
    return dataList();
}

HTMLDataListElement* HTMLInputElement::dataList() const
{
    if (!m_hasNonEmptyList)
        return 0;

    if (!m_inputType->shouldRespectListAttribute())
        return 0;

    Element* element = treeScope().getElementById(fastGetAttribute(listAttr));
    if (!element)
        return 0;
    if (!element->hasTagName(datalistTag))
        return 0;

    return toHTMLDataListElement(element);
}

bool HTMLInputElement::hasValidDataListOptions() const
{
    HTMLDataListElement* dataList = this->dataList();
    if (!dataList)
        return false;
    RefPtr<HTMLCollection> options = dataList->options();
    for (unsigned i = 0; HTMLOptionElement* option = toHTMLOptionElement(options->item(i)); ++i) {
        if (isValidValue(option->value()))
            return true;
    }
    return false;
}

void HTMLInputElement::resetListAttributeTargetObserver()
{
    if (inDocument())
        m_listAttributeTargetObserver = ListAttributeTargetObserver::create(fastGetAttribute(listAttr), this);
    else
        m_listAttributeTargetObserver = nullptr;
}

void HTMLInputElement::listAttributeTargetChanged()
{
    m_inputTypeView->listAttributeTargetChanged();
}

bool HTMLInputElement::isSteppable() const
{
    return m_inputType->isSteppable();
}

#if ENABLE(INPUT_SPEECH)

bool HTMLInputElement::isSpeechEnabled() const
{
    // FIXME: Add support for RANGE, EMAIL, URL, COLOR and DATE/TIME input types.
    return m_inputType->shouldRespectSpeechAttribute() && RuntimeEnabledFeatures::speechInputEnabled() && hasAttribute(webkitspeechAttr);
}

#endif

bool HTMLInputElement::isTextButton() const
{
    return m_inputType->isTextButton();
}

bool HTMLInputElement::isRadioButton() const
{
    return m_inputType->isRadioButton();
}

bool HTMLInputElement::isSearchField() const
{
    return m_inputType->isSearchField();
}

bool HTMLInputElement::isInputTypeHidden() const
{
    return m_inputType->isHiddenType();
}

bool HTMLInputElement::isPasswordField() const
{
    return m_inputType->isPasswordField();
}

bool HTMLInputElement::isCheckbox() const
{
    return m_inputType->isCheckbox();
}

bool HTMLInputElement::isRangeControl() const
{
    return m_inputType->isRangeControl();
}

bool HTMLInputElement::isText() const
{
    return m_inputType->isTextType();
}

bool HTMLInputElement::isEmailField() const
{
    return m_inputType->isEmailField();
}

bool HTMLInputElement::isFileUpload() const
{
    return m_inputType->isFileUpload();
}

bool HTMLInputElement::isImageButton() const
{
    return m_inputType->isImageButton();
}

bool HTMLInputElement::isNumberField() const
{
    return m_inputType->isNumberField();
}

bool HTMLInputElement::isTelephoneField() const
{
    return m_inputType->isTelephoneField();
}

bool HTMLInputElement::isURLField() const
{
    return m_inputType->isURLField();
}

bool HTMLInputElement::isDateField() const
{
    return m_inputType->isDateField();
}

bool HTMLInputElement::isDateTimeLocalField() const
{
    return m_inputType->isDateTimeLocalField();
}

bool HTMLInputElement::isMonthField() const
{
    return m_inputType->isMonthField();
}

bool HTMLInputElement::isTimeField() const
{
    return m_inputType->isTimeField();
}

bool HTMLInputElement::isWeekField() const
{
    return m_inputType->isWeekField();
}

bool HTMLInputElement::isEnumeratable() const
{
    return m_inputType->isEnumeratable();
}

bool HTMLInputElement::supportLabels() const
{
    return m_inputType->isInteractiveContent();
}

bool HTMLInputElement::shouldAppearChecked() const
{
    return checked() && m_inputType->isCheckable();
}

bool HTMLInputElement::supportsPlaceholder() const
{
    return m_inputType->supportsPlaceholder();
}

void HTMLInputElement::updatePlaceholderText()
{
    return m_inputTypeView->updatePlaceholderText();
}

void HTMLInputElement::parseMaxLengthAttribute(const AtomicString& value)
{
    int maxLength;
    if (!parseHTMLInteger(value, maxLength))
        maxLength = maximumLength;
    if (maxLength < 0 || maxLength > maximumLength)
        maxLength = maximumLength;
    int oldMaxLength = m_maxLength;
    m_maxLength = maxLength;
    if (oldMaxLength != maxLength)
        updateValueIfNeeded();
    setNeedsStyleRecalc(SubtreeStyleChange);
    setNeedsValidityCheck();
}

void HTMLInputElement::updateValueIfNeeded()
{
    String newValue = sanitizeValue(m_valueIfDirty);
    ASSERT(!m_valueIfDirty.isNull() || newValue.isNull());
    if (newValue != m_valueIfDirty)
        setValue(newValue);
}

String HTMLInputElement::defaultToolTip() const
{
    return m_inputType->defaultToolTip();
}

bool HTMLInputElement::shouldAppearIndeterminate() const
{
    return m_inputType->supportsIndeterminateAppearance() && indeterminate();
}

#if ENABLE(MEDIA_CAPTURE)
bool HTMLInputElement::capture() const
{
    if (!isFileUpload() || !fastHasAttribute(captureAttr))
        return false;

    // As per crbug.com/240252, emit a deprecation warning when the "capture"
    // attribute is used as an enum. The spec has been updated and "capture" is
    // supposed to be used as a boolean.
    bool hasDeprecatedUsage = !fastGetAttribute(captureAttr).isEmpty();
    if (hasDeprecatedUsage)
        UseCounter::countDeprecation(document(), UseCounter::CaptureAttributeAsEnum);
    else
        UseCounter::count(document(), UseCounter::CaptureAttributeAsBoolean);

    return true;
}
#endif

bool HTMLInputElement::isInRequiredRadioButtonGroup()
{
    ASSERT(isRadioButton());
    if (RadioButtonGroupScope* scope = radioButtonGroupScope())
        return scope->isInRequiredGroup(this);
    return false;
}

HTMLInputElement* HTMLInputElement::checkedRadioButtonForGroup() const
{
    if (RadioButtonGroupScope* scope = radioButtonGroupScope())
        return scope->checkedButtonForGroup(name());
    return 0;
}

RadioButtonGroupScope* HTMLInputElement::radioButtonGroupScope() const
{
    if (!isRadioButton())
        return 0;
    if (HTMLFormElement* formElement = form())
        return &formElement->radioButtonGroupScope();
    if (inDocument())
        return &document().formController()->radioButtonGroupScope();
    return 0;
}

inline void HTMLInputElement::addToRadioButtonGroup()
{
    if (RadioButtonGroupScope* scope = radioButtonGroupScope())
        scope->addButton(this);
}

inline void HTMLInputElement::removeFromRadioButtonGroup()
{
    if (RadioButtonGroupScope* scope = radioButtonGroupScope())
        scope->removeButton(this);
}

unsigned HTMLInputElement::height() const
{
    return m_inputType->height();
}

unsigned HTMLInputElement::width() const
{
    return m_inputType->width();
}

void HTMLInputElement::setHeight(unsigned height)
{
    setUnsignedIntegralAttribute(heightAttr, height);
}

void HTMLInputElement::setWidth(unsigned width)
{
    setUnsignedIntegralAttribute(widthAttr, width);
}

PassOwnPtr<ListAttributeTargetObserver> ListAttributeTargetObserver::create(const AtomicString& id, HTMLInputElement* element)
{
    return adoptPtr(new ListAttributeTargetObserver(id, element));
}

ListAttributeTargetObserver::ListAttributeTargetObserver(const AtomicString& id, HTMLInputElement* element)
    : IdTargetObserver(element->treeScope().idTargetObserverRegistry(), id)
    , m_element(element)
{
}

void ListAttributeTargetObserver::idTargetChanged()
{
    m_element->listAttributeTargetChanged();
}

void HTMLInputElement::setRangeText(const String& replacement, ExceptionState& exceptionState)
{
    if (!m_inputType->supportsSelectionAPI()) {
        exceptionState.throwDOMException(InvalidStateError, "The input element's type ('" + m_inputType->formControlType() + "') does not support selection.");
        return;
    }

    HTMLTextFormControlElement::setRangeText(replacement, exceptionState);
}

void HTMLInputElement::setRangeText(const String& replacement, unsigned start, unsigned end, const String& selectionMode, ExceptionState& exceptionState)
{
    if (!m_inputType->supportsSelectionAPI()) {
        exceptionState.throwDOMException(InvalidStateError, "The input element's type ('" + m_inputType->formControlType() + "') does not support selection.");
        return;
    }

    HTMLTextFormControlElement::setRangeText(replacement, start, end, selectionMode, exceptionState);
}

bool HTMLInputElement::setupDateTimeChooserParameters(DateTimeChooserParameters& parameters)
{
    if (!document().view())
        return false;

    parameters.type = type();
    parameters.minimum = minimum();
    parameters.maximum = maximum();
    parameters.required = isRequired();
    if (!RuntimeEnabledFeatures::langAttributeAwareFormControlUIEnabled())
        parameters.locale = defaultLanguage();
    else {
        AtomicString computedLocale = computeInheritedLanguage();
        parameters.locale = computedLocale.isEmpty() ? defaultLanguage() : computedLocale;
    }

    StepRange stepRange = createStepRange(RejectAny);
    if (stepRange.hasStep()) {
        parameters.step = stepRange.step().toDouble();
        parameters.stepBase = stepRange.stepBase().toDouble();
    } else {
        parameters.step = 1.0;
        parameters.stepBase = 0;
    }

    parameters.anchorRectInRootView = document().view()->contentsToRootView(pixelSnappedBoundingBox());
    parameters.currentValue = value();
    parameters.doubleValue = m_inputType->valueAsDouble();
    parameters.isAnchorElementRTL = computedStyle()->direction() == RTL;
    if (HTMLDataListElement* dataList = this->dataList()) {
        RefPtr<HTMLCollection> options = dataList->options();
        for (unsigned i = 0; HTMLOptionElement* option = toHTMLOptionElement(options->item(i)); ++i) {
            if (!isValidValue(option->value()))
                continue;
            DateTimeSuggestion suggestion;
            suggestion.value = m_inputType->parseToNumber(option->value(), Decimal::nan()).toDouble();
            if (std::isnan(suggestion.value))
                continue;
            suggestion.localizedValue = localizeValue(option->value());
            suggestion.label = option->value() == option->label() ? String() : option->label();
            parameters.suggestions.append(suggestion);
        }
    }
    return true;
}

bool HTMLInputElement::supportsInputModeAttribute() const
{
    return m_inputType->supportsInputModeAttribute();
}

void HTMLInputElement::setShouldRevealPassword(bool value)
{
    if (m_shouldRevealPassword == value)
        return;
    m_shouldRevealPassword = value;
    lazyReattachIfAttached();
}

bool HTMLInputElement::isInteractiveContent() const
{
    return m_inputType->isInteractiveContent();
}

bool HTMLInputElement::supportsAutofocus() const
{
    return m_inputType->isInteractiveContent();
}

#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
PassRefPtr<RenderStyle> HTMLInputElement::customStyleForRenderer()
{
    return m_inputTypeView->customStyleForRenderer(originalStyleForRenderer());
}
#endif

} // namespace
