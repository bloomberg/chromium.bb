// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/serialization/V8ScriptValueSerializer.h"

#include "bindings/core/v8/ExceptionStatePlaceholder.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/core/v8/V8DOMException.h"
#include "bindings/core/v8/V8ImageBitmap.h"
#include "bindings/core/v8/V8ImageData.h"
#include "bindings/core/v8/V8MessagePort.h"
#include "bindings/core/v8/V8StringResource.h"
#include "bindings/core/v8/serialization/V8ScriptValueDeserializer.h"
#include "core/dom/MessagePort.h"
#include "core/frame/LocalFrame.h"
#include "core/html/ImageData.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "public/platform/WebMessagePortChannel.h"
#include "public/platform/WebMessagePortChannelClient.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

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

v8::Local<v8::Value> roundTrip(v8::Local<v8::Value> value, V8TestingScope& scope, ExceptionState* overrideExceptionState = nullptr, Transferables* transferables = nullptr)
{
    RefPtr<ScriptState> scriptState = scope.getScriptState();
    ExceptionState& exceptionState = overrideExceptionState ? *overrideExceptionState : scope.getExceptionState();

    // Extract message ports and disentangle them.
    std::unique_ptr<MessagePortChannelArray> channels;
    if (transferables) {
        channels = MessagePort::disentanglePorts(scope.getExecutionContext(), transferables->messagePorts, exceptionState);
        if (exceptionState.hadException())
            return v8::Local<v8::Value>();
    }

    RefPtr<SerializedScriptValue> serializedScriptValue =
        V8ScriptValueSerializer(scriptState).serialize(value, transferables, exceptionState);
    DCHECK_EQ(!serializedScriptValue, exceptionState.hadException());
    if (!serializedScriptValue)
        return v8::Local<v8::Value>();

    // If there are message ports, make new ones and entangle them.
    MessagePortArray* transferredMessagePorts = MessagePort::entanglePorts(*scope.getExecutionContext(), std::move(channels));

    V8ScriptValueDeserializer deserializer(scriptState, serializedScriptValue);
    deserializer.setTransferredMessagePorts(transferredMessagePorts);
    return deserializer.deserialize();
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

TEST(V8ScriptValueSerializerTest, NeuteringHappensAfterSerialization)
{
    // This object will throw an exception before the [[Transfer]] step.
    // As a result, the ArrayBuffer will not be transferred.
    ScopedEnableV8BasedStructuredClone enable;
    V8TestingScope scope;
    ExceptionState exceptionState(scope.isolate(), ExceptionState::ExecutionContext, "Window", "postMessage");

    DOMArrayBuffer* arrayBuffer = DOMArrayBuffer::create(1, 1);
    ASSERT_FALSE(arrayBuffer->isNeutered());
    v8::Local<v8::Value> object = eval("({ get a() { throw 'party'; }})", scope);
    Transferables transferables;
    transferables.arrayBuffers.append(arrayBuffer);

    roundTrip(object, scope, &exceptionState, &transferables);
    ASSERT_TRUE(exceptionState.hadException());
    EXPECT_FALSE(hadDOMException("DataCloneError", scope.getScriptState(), exceptionState));
    EXPECT_FALSE(arrayBuffer->isNeutered());
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

class WebMessagePortChannelImpl final : public WebMessagePortChannel {
public:
    // WebMessagePortChannel
    void setClient(WebMessagePortChannelClient* client) override {}
    void destroy() override { delete this; }
    void postMessage(const WebString&, WebMessagePortChannelArray*) { NOTIMPLEMENTED(); }
    bool tryGetMessage(WebString*, WebMessagePortChannelArray&) { return false; }
};

MessagePort* makeMessagePort(ExecutionContext* executionContext, WebMessagePortChannel** unownedChannelOut = nullptr)
{
    auto* unownedChannel = new WebMessagePortChannelImpl();
    MessagePort* port = MessagePort::create(*executionContext);
    port->entangle(WebMessagePortChannelUniquePtr(unownedChannel));
    EXPECT_TRUE(port->isEntangled());
    EXPECT_EQ(unownedChannel, port->entangledChannelForTesting());
    if (unownedChannelOut)
        *unownedChannelOut = unownedChannel;
    return port;
}

TEST(V8ScriptValueSerializerTest, RoundTripMessagePort)
{
    ScopedEnableV8BasedStructuredClone enable;
    V8TestingScope scope;

    WebMessagePortChannel* unownedChannel;
    MessagePort* port = makeMessagePort(scope.getExecutionContext(), &unownedChannel);
    v8::Local<v8::Value> wrapper = toV8(port, scope.getScriptState());
    Transferables transferables;
    transferables.messagePorts.append(port);

    v8::Local<v8::Value> result = roundTrip(wrapper, scope, nullptr, &transferables);
    ASSERT_TRUE(V8MessagePort::hasInstance(result, scope.isolate()));
    MessagePort* newPort = V8MessagePort::toImpl(result.As<v8::Object>());
    EXPECT_FALSE(port->isEntangled());
    EXPECT_TRUE(newPort->isEntangled());
    EXPECT_EQ(unownedChannel, newPort->entangledChannelForTesting());
}

TEST(V8ScriptValueSerializerTest, NeuteredMessagePortThrowsDataCloneError)
{
    ScopedEnableV8BasedStructuredClone enable;
    V8TestingScope scope;
    ExceptionState exceptionState(scope.isolate(), ExceptionState::ExecutionContext, "Window", "postMessage");

    MessagePort* port = MessagePort::create(*scope.getExecutionContext());
    EXPECT_TRUE(port->isNeutered());
    v8::Local<v8::Value> wrapper = toV8(port, scope.getScriptState());
    Transferables transferables;
    transferables.messagePorts.append(port);

    roundTrip(wrapper, scope, &exceptionState, &transferables);
    ASSERT_TRUE(hadDOMException("DataCloneError", scope.getScriptState(), exceptionState));
}

TEST(V8ScriptValueSerializerTest, UntransferredMessagePortThrowsDataCloneError)
{
    ScopedEnableV8BasedStructuredClone enable;
    V8TestingScope scope;
    ExceptionState exceptionState(scope.isolate(), ExceptionState::ExecutionContext, "Window", "postMessage");

    WebMessagePortChannel* unownedChannel;
    MessagePort* port = makeMessagePort(scope.getExecutionContext(), &unownedChannel);
    v8::Local<v8::Value> wrapper = toV8(port, scope.getScriptState());
    Transferables transferables;

    roundTrip(wrapper, scope, &exceptionState, &transferables);
    ASSERT_TRUE(hadDOMException("DataCloneError", scope.getScriptState(), exceptionState));
}

TEST(V8ScriptValueSerializerTest, OutOfRangeMessagePortIndex)
{
    ScopedEnableV8BasedStructuredClone enable;
    V8TestingScope scope;
    ScriptState* scriptState = scope.getScriptState();
    RefPtr<SerializedScriptValue> input = serializedValue({
        0xff, 0x09, 0x3f, 0x00, 0x4d, 0x01});
    MessagePort* port1 = makeMessagePort(scope.getExecutionContext());
    MessagePort* port2 = makeMessagePort(scope.getExecutionContext());
    {
        V8ScriptValueDeserializer deserializer(scriptState, input);
        ASSERT_TRUE(deserializer.deserialize()->IsNull());
    }
    {
        V8ScriptValueDeserializer deserializer(scriptState, input);
        deserializer.setTransferredMessagePorts(new MessagePortArray);
        ASSERT_TRUE(deserializer.deserialize()->IsNull());
    }
    {
        MessagePortArray* ports = new MessagePortArray;
        ports->append(port1);
        V8ScriptValueDeserializer deserializer(scriptState, input);
        deserializer.setTransferredMessagePorts(ports);
        ASSERT_TRUE(deserializer.deserialize()->IsNull());
    }
    {
        MessagePortArray* ports = new MessagePortArray;
        ports->append(port1);
        ports->append(port2);
        V8ScriptValueDeserializer deserializer(scriptState, input);
        deserializer.setTransferredMessagePorts(ports);
        v8::Local<v8::Value> result = deserializer.deserialize();
        ASSERT_TRUE(V8MessagePort::hasInstance(result, scope.isolate()));
        EXPECT_EQ(port2, V8MessagePort::toImpl(result.As<v8::Object>()));
    }
}

// Decode tests for backward compatibility are not required for message ports
// because they cannot be persisted to disk.

// A more exhaustive set of ImageBitmap cases are covered by LayoutTests.
TEST(V8ScriptValueSerializerTest, RoundTripImageBitmap)
{
    ScopedEnableV8BasedStructuredClone enable;
    V8TestingScope scope;

    // Make a 10x7 red ImageBitmap.
    sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(10, 7);
    surface->getCanvas()->clear(SK_ColorRED);
    ImageBitmap* imageBitmap = ImageBitmap::create(StaticBitmapImage::create(surface->makeImageSnapshot()));

    // Serialize and deserialize it.
    v8::Local<v8::Value> wrapper = toV8(imageBitmap, scope.getScriptState());
    v8::Local<v8::Value> result = roundTrip(wrapper, scope);
    ASSERT_TRUE(V8ImageBitmap::hasInstance(result, scope.isolate()));
    ImageBitmap* newImageBitmap = V8ImageBitmap::toImpl(result.As<v8::Object>());
    ASSERT_EQ(IntSize(10, 7), newImageBitmap->size());

    // Check that the pixel at (3, 3) is red.
    uint8_t pixel[4] = {};
    ASSERT_TRUE(newImageBitmap->bitmapImage()->imageForCurrentFrame()->readPixels(
        SkImageInfo::Make(1, 1, kRGBA_8888_SkColorType, kPremul_SkAlphaType),
        &pixel, 4, 3, 3));
    ASSERT_THAT(pixel, ::testing::ElementsAre(255, 0, 0, 255));
}

TEST(V8ScriptValueSerializerTest, DecodeImageBitmap)
{
    // Backward compatibility with existing serialized ImageBitmap objects must be
    // maintained. Add more cases if the format changes; don't remove tests for
    // old versions.
    ScopedEnableV8BasedStructuredClone enable;
    V8TestingScope scope;
    ScriptState* scriptState = scope.getScriptState();

    // This is checked by platform instead of by SK_PMCOLOR_BYTE_ORDER because
    // this test intends to ensure that a platform can decode images it has
    // previously written. At format version 9, Android writes RGBA and every
    // other platform writes BGRA.
#if OS(ANDROID)
    RefPtr<SerializedScriptValue> input = serializedValue({
        0xff, 0x09, 0x3f, 0x00, 0x67, 0x01, 0x01, 0x02, 0x01,
        0x08, 0xff, 0x00, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff });
#else
    RefPtr<SerializedScriptValue> input = serializedValue({
        0xff, 0x09, 0x3f, 0x00, 0x67, 0x01, 0x01, 0x02, 0x01,
        0x08, 0x00, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0xff });
#endif

    v8::Local<v8::Value> result = V8ScriptValueDeserializer(scriptState, input).deserialize();
    ASSERT_TRUE(V8ImageBitmap::hasInstance(result, scope.isolate()));
    ImageBitmap* newImageBitmap = V8ImageBitmap::toImpl(result.As<v8::Object>());
    ASSERT_EQ(IntSize(2, 1), newImageBitmap->size());

    // Check that the pixels are opaque red and green, respectively.
    uint8_t pixels[8] = {};
    ASSERT_TRUE(newImageBitmap->bitmapImage()->imageForCurrentFrame()->readPixels(
        SkImageInfo::Make(2, 1, kRGBA_8888_SkColorType, kPremul_SkAlphaType),
        &pixels, 8, 0, 0));
    ASSERT_THAT(pixels, ::testing::ElementsAre(255, 0, 0, 255, 0, 255, 0, 255));
}

TEST(V8ScriptValueSerializerTest, InvalidImageBitmapDecode)
{
    ScopedEnableV8BasedStructuredClone enable;
    V8TestingScope scope;
    ScriptState* scriptState = scope.getScriptState();
    {
        // Too many bytes declared in pixel data.
        RefPtr<SerializedScriptValue> input = serializedValue({
            0xff, 0x09, 0x3f, 0x00, 0x67, 0x01, 0x01, 0x02, 0x01,
            0x09, 0x00, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0xff,
            0x00, 0x00});
        EXPECT_TRUE(V8ScriptValueDeserializer(scriptState, input).deserialize()->IsNull());
    }
    {
        // Too few bytes declared in pixel data.
        RefPtr<SerializedScriptValue> input = serializedValue({
            0xff, 0x09, 0x3f, 0x00, 0x67, 0x01, 0x01, 0x02, 0x01,
            0x07, 0x00, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0xff });
        EXPECT_TRUE(V8ScriptValueDeserializer(scriptState, input).deserialize()->IsNull());
    }
    {
        // Nonsense for origin clean data.
        RefPtr<SerializedScriptValue> input = serializedValue({
            0xff, 0x09, 0x3f, 0x00, 0x67, 0x02, 0x01, 0x02, 0x01,
            0x08, 0x00, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0xff });
        EXPECT_TRUE(V8ScriptValueDeserializer(scriptState, input).deserialize()->IsNull());
    }
    {
        // Nonsense for premultiplied bit.
        RefPtr<SerializedScriptValue> input = serializedValue({
            0xff, 0x09, 0x3f, 0x00, 0x67, 0x01, 0x02, 0x02, 0x01,
            0x08, 0x00, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0xff });
        EXPECT_TRUE(V8ScriptValueDeserializer(scriptState, input).deserialize()->IsNull());
    }
}

TEST(V8ScriptValueSerializerTest, TransferImageBitmap)
{
    // More thorough tests exist in LayoutTests/.
    ScopedEnableV8BasedStructuredClone enable;
    V8TestingScope scope;

    sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(10, 7);
    surface->getCanvas()->clear(SK_ColorRED);
    sk_sp<SkImage> image = surface->makeImageSnapshot();
    ImageBitmap* imageBitmap = ImageBitmap::create(StaticBitmapImage::create(image));

    v8::Local<v8::Value> wrapper = toV8(imageBitmap, scope.getScriptState());
    Transferables transferables;
    transferables.imageBitmaps.append(imageBitmap);
    v8::Local<v8::Value> result = roundTrip(wrapper, scope, nullptr, &transferables);
    ASSERT_TRUE(V8ImageBitmap::hasInstance(result, scope.isolate()));
    ImageBitmap* newImageBitmap = V8ImageBitmap::toImpl(result.As<v8::Object>());
    ASSERT_EQ(IntSize(10, 7), newImageBitmap->size());

    // Check that the pixel at (3, 3) is red.
    uint8_t pixel[4] = {};
    sk_sp<SkImage> newImage = newImageBitmap->bitmapImage()->imageForCurrentFrame();
    ASSERT_TRUE(newImage->readPixels(
        SkImageInfo::Make(1, 1, kRGBA_8888_SkColorType, kPremul_SkAlphaType),
        &pixel, 4, 3, 3));
    ASSERT_THAT(pixel, ::testing::ElementsAre(255, 0, 0, 255));

    // Check also that the underlying image contents were transferred.
    EXPECT_EQ(image, newImage);
    EXPECT_TRUE(imageBitmap->isNeutered());
}

} // namespace
} // namespace blink
