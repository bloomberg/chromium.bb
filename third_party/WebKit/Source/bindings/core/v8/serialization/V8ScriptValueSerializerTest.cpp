// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/serialization/V8ScriptValueSerializer.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/core/v8/V8DOMException.h"
#include "bindings/core/v8/V8ImageData.h"
#include "bindings/core/v8/V8StringResource.h"
#include "bindings/core/v8/serialization/V8ScriptValueDeserializer.h"
#include "core/frame/LocalFrame.h"
#include "core/html/ImageData.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {
namespace {

class ScopedEnableV8BasedStructuredClone {
public:
    ScopedEnableV8BasedStructuredClone()
        : m_wasEnabled(RuntimeEnabledFeatures::v8BasedStructuredCloneEnabled())
    {
        RuntimeEnabledFeatures::setV8BasedStructuredCloneEnabled(true);
    }
    ~ScopedEnableV8BasedStructuredClone()
    {
        RuntimeEnabledFeatures::setV8BasedStructuredCloneEnabled(m_wasEnabled);
    }
private:
    bool m_wasEnabled;
};

RefPtr<SerializedScriptValue> serializedValue(const Vector<uint8_t>& bytes)
{
    // TODO(jbroman): Fix this once SerializedScriptValue can take bytes without
    // endianness swapping.
    DCHECK_EQ(bytes.size() % 2, 0u);
    return SerializedScriptValue::create(String(reinterpret_cast<const UChar*>(&bytes[0]), bytes.size() / 2));
}

v8::Local<v8::Value> roundTrip(v8::Local<v8::Value> value, V8TestingScope& scope)
{
    RefPtr<ScriptState> scriptState = scope.getScriptState();
    ExceptionState& exceptionState = scope.getExceptionState();
    RefPtr<SerializedScriptValue> serializedScriptValue =
        V8ScriptValueSerializer(scriptState).serialize(value, nullptr, exceptionState);
    DCHECK_EQ(!serializedScriptValue, exceptionState.hadException());
    EXPECT_TRUE(serializedScriptValue);
    if (!serializedScriptValue)
        return v8::Local<v8::Value>();
    return V8ScriptValueDeserializer(scriptState, serializedScriptValue).deserialize();
}

v8::Local<v8::Value> eval(const String& source, V8TestingScope& scope)
{
    return scope.frame().script().executeScriptInMainWorldAndReturnValue(source);
}

String toJSON(v8::Local<v8::Object> object, const V8TestingScope& scope)
{
    return v8StringToWebCoreString<String>(
        v8::JSON::Stringify(scope.context(), object).ToLocalChecked(),
        DoNotExternalize);
}

// Checks for a DOM exception, including a rethrown one.
::testing::AssertionResult hadDOMException(const StringView& name, ScriptState* scriptState, ExceptionState& exceptionState)
{
    if (!exceptionState.hadException())
        return ::testing::AssertionFailure() << "no exception thrown";
    DOMException* domException = V8DOMException::toImplWithTypeCheck(scriptState->isolate(), exceptionState.getException());
    if (!domException)
        return ::testing::AssertionFailure() << "exception thrown was not a DOMException";
    if (domException->name() != name)
        return ::testing::AssertionFailure() << "was " << domException->name();
    return ::testing::AssertionSuccess();
}

TEST(V8ScriptValueSerializerTest, RoundTripJSONLikeValue)
{
    // Ensure that simple JavaScript objects work.
    // There are more exhaustive tests of JavaScript objects in V8.
    ScopedEnableV8BasedStructuredClone enable;
    V8TestingScope scope;
    v8::Local<v8::Value> object = eval("({ foo: [1, 2, 3], bar: 'baz' })", scope);
    DCHECK(object->IsObject());
    v8::Local<v8::Value> result = roundTrip(object, scope);
    ASSERT_TRUE(result->IsObject());
    EXPECT_NE(object, result);
    EXPECT_EQ(toJSON(object.As<v8::Object>(), scope), toJSON(result.As<v8::Object>(), scope));
}

TEST(V8ScriptValueSerializerTest, ThrowsDataCloneError)
{
    // Ensure that a proper DataCloneError DOMException is thrown when issues
    // are encountered in V8 (for example, cloning a symbol). It should be an
    // instance of DOMException, and it should have a proper descriptive
    // message.
    ScopedEnableV8BasedStructuredClone enable;
    V8TestingScope scope;
    ScriptState* scriptState = scope.getScriptState();
    ExceptionState exceptionState(scope.isolate(), ExceptionState::ExecutionContext, "Window", "postMessage");
    v8::Local<v8::Value> symbol = eval("Symbol()", scope);
    DCHECK(symbol->IsSymbol());
    ASSERT_FALSE(V8ScriptValueSerializer(scriptState).serialize(symbol, nullptr, exceptionState));
    ASSERT_TRUE(hadDOMException("DataCloneError", scriptState, exceptionState));
    DOMException* domException = V8DOMException::toImpl(exceptionState.getException().As<v8::Object>());
    EXPECT_TRUE(domException->toString().contains("postMessage"));
}

TEST(V8ScriptValueSerializerTest, RethrowsScriptError)
{
    // Ensure that other exceptions, like those thrown by script, are properly
    // rethrown.
    ScopedEnableV8BasedStructuredClone enable;
    V8TestingScope scope;
    ScriptState* scriptState = scope.getScriptState();
    ExceptionState exceptionState(scope.isolate(), ExceptionState::ExecutionContext, "Window", "postMessage");
    v8::Local<v8::Value> exception = eval("myException=new Error()", scope);
    v8::Local<v8::Value> object = eval("({ get a() { throw myException; }})", scope);
    DCHECK(object->IsObject());
    ASSERT_FALSE(V8ScriptValueSerializer(scriptState).serialize(object, nullptr, exceptionState));
    ASSERT_TRUE(exceptionState.hadException());
    EXPECT_EQ(exception, exceptionState.getException());
}

TEST(V8ScriptValueSerializerTest, DeserializationErrorReturnsNull)
{
    // If there's a problem during deserialization, it results in null, but no
    // exception.
    ScopedEnableV8BasedStructuredClone enable;
    V8TestingScope scope;
    ScriptState* scriptState = scope.getScriptState();
    RefPtr<SerializedScriptValue> invalid = SerializedScriptValue::create("invalid data");
    v8::Local<v8::Value> result = V8ScriptValueDeserializer(scriptState, invalid).deserialize();
    EXPECT_TRUE(result->IsNull());
    EXPECT_FALSE(scope.getExceptionState().hadException());
}

TEST(V8ScriptValueSerializerTest, RoundTripImageData)
{
    // ImageData objects should serialize and deserialize correctly.
    ScopedEnableV8BasedStructuredClone enable;
    V8TestingScope scope;
    ImageData* imageData = ImageData::create(2, 1, ASSERT_NO_EXCEPTION);
    imageData->data()->data()[0] = 200;
    v8::Local<v8::Value> wrapper = toV8(imageData, scope.context()->Global(), scope.isolate());
    v8::Local<v8::Value> result = roundTrip(wrapper, scope);
    ASSERT_TRUE(V8ImageData::hasInstance(result, scope.isolate()));
    ImageData* newImageData = V8ImageData::toImpl(result.As<v8::Object>());
    EXPECT_NE(imageData, newImageData);
    EXPECT_EQ(imageData->size(), newImageData->size());
    EXPECT_EQ(imageData->data()->length(), newImageData->data()->length());
    EXPECT_EQ(200, newImageData->data()->data()[0]);
}

TEST(V8ScriptValueSerializerTest, DecodeImageData)
{
    // Backward compatibility with existing serialized ImageData objects must be
    // maintained. Add more cases if the format changes; don't remove tests for
    // old versions.
    ScopedEnableV8BasedStructuredClone enable;
    V8TestingScope scope;
    ScriptState* scriptState = scope.getScriptState();
    RefPtr<SerializedScriptValue> input = serializedValue({
        0xff, 0x09, 0x3f, 0x00, 0x23, 0x02, 0x01, 0x08,
        0xc8, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
    v8::Local<v8::Value> result = V8ScriptValueDeserializer(scriptState, input).deserialize();
    ASSERT_TRUE(V8ImageData::hasInstance(result, scope.isolate()));
    ImageData* newImageData = V8ImageData::toImpl(result.As<v8::Object>());
    EXPECT_EQ(IntSize(2, 1), newImageData->size());
    EXPECT_EQ(8u, newImageData->data()->length());
    EXPECT_EQ(200, newImageData->data()->data()[0]);
}

} // namespace
} // namespace blink
