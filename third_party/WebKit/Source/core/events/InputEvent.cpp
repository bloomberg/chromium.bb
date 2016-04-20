// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/events/InputEvent.h"

#include "core/events/EventDispatcher.h"
#include "public/platform/WebEditingCommandType.h"

namespace blink {

namespace {

const struct {
    InputEvent::InputType inputType;
    const char* stringName;
} kInputTypeStringNameMap[] = {
    {InputEvent::InputType::None, ""},
    {InputEvent::InputType::InsertText, "insertText"},
    {InputEvent::InputType::ReplaceContent, "replaceContent"},
    {InputEvent::InputType::DeleteContent, "deleteContent"},
    {InputEvent::InputType::DeleteComposedCharacter, "deleteComposedCharacter"},
    {InputEvent::InputType::Undo, "undo"},
    {InputEvent::InputType::Redo, "redo"},
};

static_assert(arraysize(kInputTypeStringNameMap) == static_cast<size_t>(InputEvent::InputType::NumberOfInputTypes),
    "must handle all InputEvent::InputType");

String convertInputTypeToString(InputEvent::InputType inputType)
{
    const auto& it = std::begin(kInputTypeStringNameMap) + static_cast<size_t>(inputType);
    if (it >= std::begin(kInputTypeStringNameMap) && it < std::end(kInputTypeStringNameMap))
        return AtomicString(it->stringName);
    return emptyString();
}

InputEvent::InputType convertStringToInputType(const String& stringName)
{
    // TODO(chongz): Use binary search if the map goes larger.
    for (const auto& entry : kInputTypeStringNameMap) {
        if (stringName == entry.stringName)
            return entry.inputType;
    }
    return InputEvent::InputType::None;
}

} // anonymous namespace

InputEvent::InputEvent()
{
}

InputEvent::InputEvent(const AtomicString& type, const InputEventInit& initializer)
    : UIEvent(type, initializer)
{
    // TODO(ojan): We should find a way to prevent conversion like String->enum->String just in order to use initializer.
    // See InputEvent::createBeforeInput() for the first conversion.
    if (initializer.hasInputType())
        m_inputType = convertStringToInputType(initializer.inputType());
    if (initializer.hasData())
        m_data = initializer.data();
}

/* static */
InputEvent* InputEvent::createBeforeInput(InputType inputType, const String& data, EventCancelable cancelable)
{
    InputEventInit inputEventInit;

    inputEventInit.setBubbles(true);
    inputEventInit.setCancelable(cancelable == IsCancelable);
    // TODO(ojan): We should find a way to prevent conversion like String->enum->String just in order to use initializer.
    // See InputEvent::InputEvent() for the second conversion.
    inputEventInit.setInputType(convertInputTypeToString(inputType));
    inputEventInit.setData(data);

    return InputEvent::create(EventTypeNames::beforeinput, inputEventInit);
}

String InputEvent::inputType() const
{
    return convertInputTypeToString(m_inputType);
}

bool InputEvent::isInputEvent() const
{
    return true;
}

DEFINE_TRACE(InputEvent)
{
    UIEvent::trace(visitor);
}

} // namespace blink
