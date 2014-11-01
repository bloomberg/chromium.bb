/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2010 Apple Inc. All rights reserved.
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

#ifndef HTMLInputElement_h
#define HTMLInputElement_h

#include "core/html/HTMLTextFormControlElement.h"
#include "core/html/forms/StepRange.h"
#include "platform/FileChooser.h"

namespace blink {

class AXObject;
class DragData;
class ExceptionState;
class FileList;
class HTMLDataListElement;
class HTMLImageLoader;
class InputType;
class InputTypeView;
class KURL;
class ListAttributeTargetObserver;
class RadioButtonGroupScope;
struct DateTimeChooserParameters;

class HTMLInputElement : public HTMLTextFormControlElement {
    DEFINE_WRAPPERTYPEINFO();
public:
    static PassRefPtrWillBeRawPtr<HTMLInputElement> create(Document&, HTMLFormElement*, bool createdByParser);
    virtual ~HTMLInputElement();
    virtual void trace(Visitor*) override;

    DEFINE_ATTRIBUTE_EVENT_LISTENER(webkitspeechchange);

    virtual bool shouldAutocomplete() const override final;

    // For ValidityState
    virtual bool hasBadInput() const override final;
    virtual bool patternMismatch() const override final;
    virtual bool rangeUnderflow() const override final;
    virtual bool rangeOverflow() const override final;
    virtual bool stepMismatch() const override final;
    virtual bool tooLong() const override final;
    virtual bool tooShort() const override final;
    virtual bool typeMismatch() const override final;
    virtual bool valueMissing() const override final;
    virtual String validationMessage() const override final;

    // Returns the minimum value for type=date, number, or range.  Don't call this for other types.
    double minimum() const;
    // Returns the maximum value for type=date, number, or range.  Don't call this for other types.
    // This always returns a value which is >= minimum().
    double maximum() const;
    // Sets the "allowed value step" defined in the HTML spec to the specified double pointer.
    // Returns false if there is no "allowed value step."
    bool getAllowedValueStep(Decimal*) const;
    StepRange createStepRange(AnyStepHandling) const;

    Decimal findClosestTickMarkValue(const Decimal&);

    // Implementations of HTMLInputElement::stepUp() and stepDown().
    void stepUp(int, ExceptionState&);
    void stepDown(int, ExceptionState&);
    // stepUp()/stepDown() for user-interaction.
    bool isSteppable() const;

    // Returns true if the type is button, reset, or submit.
    bool isTextButton() const;
    // Returns true if the type is email, number, password, search, tel, text,
    // or url.
    bool isTextField() const;

    bool checked() const { return m_isChecked; }
    void setChecked(bool, TextFieldEventBehavior = DispatchNoEvent);

    // 'indeterminate' is a state independent of the checked state that causes the control to draw in a way that hides the actual state.
    bool indeterminate() const { return m_isIndeterminate; }
    void setIndeterminate(bool);
    // shouldAppearChecked is used by the rendering tree/CSS while checked() is used by JS to determine checked state
    bool shouldAppearChecked() const;
    virtual bool shouldAppearIndeterminate() const override;

    int size() const;
    bool sizeShouldIncludeDecoration(int& preferredSize) const;

    void setType(const AtomicString&);

    virtual String value() const override;
    void setValue(const String&, ExceptionState&, TextFieldEventBehavior = DispatchNoEvent);
    void setValue(const String&, TextFieldEventBehavior = DispatchNoEvent);
    void setValueForUser(const String&);
    // Checks if the specified string would be a valid value.
    // We should not call this for types with no string value such as CHECKBOX and RADIO.
    bool isValidValue(const String&) const;
    bool hasDirtyValue() const { return !m_valueIfDirty.isNull(); };

    String sanitizeValue(const String&) const;

    String localizeValue(const String&) const;

    const String& suggestedValue() const;
    void setSuggestedValue(const String&);

    void setEditingValue(const String&);

    double valueAsDate(bool& isNull) const;
    void setValueAsDate(double, ExceptionState&);

    double valueAsNumber() const;
    void setValueAsNumber(double, ExceptionState&, TextFieldEventBehavior = DispatchNoEvent);

    String valueWithDefault() const;

    void setValueFromRenderer(const String&);

    int selectionStartForBinding(ExceptionState&) const;
    int selectionEndForBinding(ExceptionState&) const;
    String selectionDirectionForBinding(ExceptionState&) const;
    void setSelectionStartForBinding(int, ExceptionState&);
    void setSelectionEndForBinding(int, ExceptionState&);
    void setSelectionDirectionForBinding(const String&, ExceptionState&);
    void setSelectionRangeForBinding(int start, int end, ExceptionState&);
    void setSelectionRangeForBinding(int start, int end, const String& direction, ExceptionState&);

    virtual bool rendererIsNeeded(const RenderStyle&) override final;
    virtual RenderObject* createRenderer(RenderStyle*) override;
    virtual void detach(const AttachContext& = AttachContext()) override final;
    virtual void updateFocusAppearance(bool restorePreviousSelection) override final;

    // FIXME: For isActivatedSubmit and setActivatedSubmit, we should use the NVI-idiom here by making
    // it private virtual in all classes and expose a public method in HTMLFormControlElement to call
    // the private virtual method.
    virtual bool isActivatedSubmit() const override final;
    virtual void setActivatedSubmit(bool flag) override final;

    String altText() const;

    int maxResults() const { return m_maxResults; }

    const AtomicString& defaultValue() const;

    Vector<String> acceptMIMETypes();
    Vector<String> acceptFileExtensions();
    const AtomicString& alt() const;

    void setSize(unsigned);
    void setSize(unsigned, ExceptionState&);

    KURL src() const;

    int maxLength() const;
    int minLength() const;
    void setMaxLength(int, ExceptionState&);
    void setMinLength(int, ExceptionState&);

    bool multiple() const;

    FileList* files();
    void setFiles(FileList*);

    // Returns true if the given DragData has more than one dropped files.
    bool receiveDroppedFiles(const DragData*);

    String droppedFileSystemId();

    // These functions are used for rendering the input active during a
    // drag-and-drop operation.
    bool canReceiveDroppedFiles() const;
    void setCanReceiveDroppedFiles(bool);

    void onSearch();

    void updateClearButtonVisibility();

    virtual bool willRespondToMouseClickEvents() override;

    HTMLElement* list() const;
    HTMLDataListElement* dataList() const;
    bool hasValidDataListOptions() const;
    void listAttributeTargetChanged();

    HTMLInputElement* checkedRadioButtonForGroup();
    bool isInRequiredRadioButtonGroup();

    // Functions for InputType classes.
    void setValueInternal(const String&, TextFieldEventBehavior);
    bool valueAttributeWasUpdatedAfterParsing() const { return m_valueAttributeWasUpdatedAfterParsing; }
    void updateView();
    bool needsToUpdateViewValue() const { return m_needsToUpdateViewValue; }
    virtual void setInnerEditorValue(const String&) override;

    void cacheSelectionInResponseToSetValue(int caretOffset) { cacheSelection(caretOffset, caretOffset, SelectionHasNoDirection); }

    // For test purposes.
    void selectColorInColorChooser(const Color&);
    void endColorChooser();

    String defaultToolTip() const;

    static const int maximumLength;

    unsigned height() const;
    unsigned width() const;
    void setHeight(unsigned);
    void setWidth(unsigned);

    virtual void blur() override final;
    void defaultBlur();

    virtual const AtomicString& name() const override final;

    void beginEditing();
    void endEditing();

    static Vector<FileChooserFileInfo> filesFromFileInputFormControlState(const FormControlState&);

    virtual bool matchesReadOnlyPseudoClass() const override final;
    virtual bool matchesReadWritePseudoClass() const override final;
    virtual void setRangeText(const String& replacement, ExceptionState&) override final;
    virtual void setRangeText(const String& replacement, unsigned start, unsigned end, const String& selectionMode, ExceptionState&) override final;

    bool hasImageLoader() const { return m_imageLoader; }
    HTMLImageLoader* imageLoader();

    bool setupDateTimeChooserParameters(DateTimeChooserParameters&);

    bool supportsInputModeAttribute() const;

    void setShouldRevealPassword(bool value);
    bool shouldRevealPassword() const { return m_shouldRevealPassword; }
    AXObject* popupRootAXObject();
    virtual void didNotifySubtreeInsertionsToDocument() override;

protected:
    HTMLInputElement(Document&, HTMLFormElement*, bool createdByParser);

    virtual void defaultEventHandler(Event*) override;

private:
    enum AutoCompleteSetting { Uninitialized, On, Off };

    virtual void didAddUserAgentShadowRoot(ShadowRoot&) override final;
    virtual void willAddFirstAuthorShadowRoot() override final;

    virtual void willChangeForm() override final;
    virtual void didChangeForm() override final;
    virtual InsertionNotificationRequest insertedInto(ContainerNode*) override;
    virtual void removedFrom(ContainerNode*) override final;
    virtual void didMoveToNewDocument(Document& oldDocument) override final;
    virtual void removeAllEventListeners() override final;

    virtual bool hasCustomFocusLogic() const override final;
    virtual bool isKeyboardFocusable() const override final;
    virtual bool shouldShowFocusRingOnMouseFocus() const override final;
    virtual bool isEnumeratable() const override final;
    virtual bool isInteractiveContent() const override final;
    virtual bool supportLabels() const override final;

    virtual bool isTextFormControl() const override final { return isTextField(); }

    virtual bool canTriggerImplicitSubmission() const override final { return isTextField(); }

    virtual const AtomicString& formControlType() const override final;

    virtual bool shouldSaveAndRestoreFormControlState() const override final;
    virtual FormControlState saveFormControlState() const override final;
    virtual void restoreFormControlState(const FormControlState&) override final;

    virtual bool canStartSelection() const override final;

    virtual void accessKeyAction(bool sendMouseEvents) override final;

    virtual void attributeWillChange(const QualifiedName&, const AtomicString& oldValue, const AtomicString& newValue) override;
    virtual void parseAttribute(const QualifiedName&, const AtomicString&) override;
    virtual bool isPresentationAttribute(const QualifiedName&) const override final;
    virtual void collectStyleForPresentationAttribute(const QualifiedName&, const AtomicString&, MutableStylePropertySet*) override final;
    virtual void finishParsingChildren() override final;

    virtual void copyNonAttributePropertiesFromElement(const Element&) override final;

    virtual void attach(const AttachContext& = AttachContext()) override final;

    virtual bool appendFormData(FormDataList&, bool) override final;
    virtual String resultForDialogSubmit() override final;

    virtual bool canBeSuccessfulSubmitButton() const override final;

    virtual void resetImpl() override final;
    virtual bool supportsAutofocus() const override final;

    virtual void* preDispatchEventHandler(Event*) override final;
    virtual void postDispatchEventHandler(Event*, void* dataFromPreDispatch) override final;

    virtual bool isURLAttribute(const Attribute&) const override final;
    virtual bool hasLegalLinkAttribute(const QualifiedName&) const override final;
    virtual const QualifiedName& subResourceAttributeName() const override final;
    virtual bool isInRange() const override final;
    virtual bool isOutOfRange() const override final;

    bool tooLong(const String&, NeedsToCheckDirtyFlag) const;
    bool tooShort(const String&, NeedsToCheckDirtyFlag) const;

    virtual bool supportsPlaceholder() const override final;
    virtual void updatePlaceholderText() override final;
    virtual bool isEmptyValue() const override final { return innerEditorValue().isEmpty(); }
    virtual bool isEmptySuggestedValue() const override final { return suggestedValue().isEmpty(); }
    virtual void handleFocusEvent(Element* oldFocusedElement, FocusType) override final;
    virtual void handleBlurEvent() override final;
    virtual void dispatchFocusInEvent(const AtomicString& eventType, Element* oldFocusedElement, FocusType) override final;

    virtual bool isOptionalFormControl() const override final { return !isRequiredFormControl(); }
    virtual bool isRequiredFormControl() const override final;
    virtual bool recalcWillValidate() const override final;
    virtual void requiredAttributeChanged() override final;

    void updateType();

    virtual void subtreeHasChanged() override final;

    void setListAttributeTargetObserver(PassOwnPtrWillBeRawPtr<ListAttributeTargetObserver>);
    void resetListAttributeTargetObserver();
    void parseMaxLengthAttribute(const AtomicString&);
    void parseMinLengthAttribute(const AtomicString&);
    void updateValueIfNeeded();

    // Returns null if this isn't associated with any radio button group.
    RadioButtonGroupScope* radioButtonGroupScope() const;
    void addToRadioButtonGroup();
    void removeFromRadioButtonGroup();
#if ENABLE(INPUT_MULTIPLE_FIELDS_UI)
    virtual PassRefPtr<RenderStyle> customStyleForRenderer() override;
#endif

    virtual bool shouldDispatchFormControlChangeEvent(String&, String&) override;

    AtomicString m_name;
    String m_valueIfDirty;
    String m_suggestedValue;
    int m_size;
    int m_maxLength;
    int m_minLength;
    short m_maxResults;
    bool m_isChecked : 1;
    bool m_reflectsCheckedAttribute : 1;
    bool m_isIndeterminate : 1;
    bool m_isActivatedSubmit : 1;
    unsigned m_autocomplete : 2; // AutoCompleteSetting
    bool m_hasNonEmptyList : 1;
    bool m_stateRestored : 1;
    bool m_parsingInProgress : 1;
    bool m_valueAttributeWasUpdatedAfterParsing : 1;
    bool m_canReceiveDroppedFiles : 1;
    bool m_hasTouchEventHandler : 1;
    bool m_shouldRevealPassword : 1;
    bool m_needsToUpdateViewValue : 1;
    RefPtrWillBeMember<InputType> m_inputType;
    RefPtrWillBeMember<InputTypeView> m_inputTypeView;
    // The ImageLoader must be owned by this element because the loader code assumes
    // that it lives as long as its owning element lives. If we move the loader into
    // the ImageInput object we may delete the loader while this element lives on.
    OwnPtrWillBeMember<HTMLImageLoader> m_imageLoader;
    OwnPtrWillBeMember<ListAttributeTargetObserver> m_listAttributeTargetObserver;
};

} // namespace blink

#endif // HTMLInputElement_h
