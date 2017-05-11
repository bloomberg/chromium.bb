// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/serialization/V8ScriptValueSerializer.h"

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/ScriptController.h"
#include "bindings/core/v8/ScriptSourceCode.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "bindings/core/v8/V8Blob.h"
#include "bindings/core/v8/V8CompositorProxy.h"
#include "bindings/core/v8/V8DOMException.h"
#include "bindings/core/v8/V8File.h"
#include "bindings/core/v8/V8FileList.h"
#include "bindings/core/v8/V8ImageBitmap.h"
#include "bindings/core/v8/V8ImageData.h"
#include "bindings/core/v8/V8MessagePort.h"
#include "bindings/core/v8/V8OffscreenCanvas.h"
#include "bindings/core/v8/V8StringResource.h"
#include "bindings/core/v8/serialization/V8ScriptValueDeserializer.h"
#include "core/dom/CompositorProxy.h"
#include "core/dom/MessagePort.h"
#include "core/fileapi/Blob.h"
#include "core/fileapi/File.h"
#include "core/fileapi/FileList.h"
#include "core/frame/LocalFrame.h"
#include "core/html/ImageData.h"
#include "core/offscreencanvas/OffscreenCanvas.h"
#include "platform/RuntimeEnabledFeatures.h"
#include "platform/graphics/CompositorMutableProperties.h"
#include "platform/graphics/StaticBitmapImage.h"
#include "platform/wtf/CurrentTime.h"
#include "platform/wtf/DateMath.h"
#include "public/platform/WebBlobInfo.h"
#include "public/platform/WebMessagePortChannel.h"
#include "public/platform/WebMessagePortChannelClient.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "third_party/skia/include/core/SkSurface.h"

namespace blink {
namespace {

RefPtr<SerializedScriptValue> SerializedValue(const Vector<uint8_t>& bytes) {
  // TODO(jbroman): Fix this once SerializedScriptValue can take bytes without
  // endianness swapping.
  DCHECK_EQ(bytes.size() % 2, 0u);
  return SerializedScriptValue::Create(
      String(reinterpret_cast<const UChar*>(&bytes[0]), bytes.size() / 2));
}

v8::Local<v8::Value> RoundTrip(
    v8::Local<v8::Value> value,
    V8TestingScope& scope,
    ExceptionState* override_exception_state = nullptr,
    Transferables* transferables = nullptr,
    WebBlobInfoArray* blob_info = nullptr) {
  RefPtr<ScriptState> script_state = scope.GetScriptState();
  ExceptionState& exception_state = override_exception_state
                                        ? *override_exception_state
                                        : scope.GetExceptionState();

  // Extract message ports and disentangle them.
  MessagePortChannelArray channels;
  if (transferables) {
    channels = MessagePort::DisentanglePorts(scope.GetExecutionContext(),
                                             transferables->message_ports,
                                             exception_state);
    if (exception_state.HadException())
      return v8::Local<v8::Value>();
  }

  V8ScriptValueSerializer::Options serialize_options;
  serialize_options.transferables = transferables;
  serialize_options.blob_info = blob_info;
  V8ScriptValueSerializer serializer(script_state, serialize_options);
  RefPtr<SerializedScriptValue> serialized_script_value =
      serializer.Serialize(value, exception_state);
  DCHECK_EQ(!serialized_script_value, exception_state.HadException());
  if (!serialized_script_value)
    return v8::Local<v8::Value>();

  // If there are message ports, make new ones and entangle them.
  MessagePortArray* transferred_message_ports = MessagePort::EntanglePorts(
      *scope.GetExecutionContext(), std::move(channels));

  V8ScriptValueDeserializer::Options deserialize_options;
  deserialize_options.message_ports = transferred_message_ports;
  deserialize_options.blob_info = blob_info;
  V8ScriptValueDeserializer deserializer(script_state, serialized_script_value,
                                         deserialize_options);
  return deserializer.Deserialize();
}

v8::Local<v8::Value> Eval(const String& source, V8TestingScope& scope) {
  return scope.GetFrame()
      .GetScriptController()
      .ExecuteScriptInMainWorldAndReturnValue(source);
}

String ToJSON(v8::Local<v8::Object> object, const V8TestingScope& scope) {
  return V8StringToWebCoreString<String>(
      v8::JSON::Stringify(scope.GetContext(), object).ToLocalChecked(),
      kDoNotExternalize);
}

// Checks for a DOM exception, including a rethrown one.
::testing::AssertionResult HadDOMException(const StringView& name,
                                           ScriptState* script_state,
                                           ExceptionState& exception_state) {
  if (!exception_state.HadException())
    return ::testing::AssertionFailure() << "no exception thrown";
  DOMException* dom_exception = V8DOMException::toImplWithTypeCheck(
      script_state->GetIsolate(), exception_state.GetException());
  if (!dom_exception)
    return ::testing::AssertionFailure()
           << "exception thrown was not a DOMException";
  if (dom_exception->name() != name)
    return ::testing::AssertionFailure() << "was " << dom_exception->name();
  return ::testing::AssertionSuccess();
}

TEST(V8ScriptValueSerializerTest, RoundTripJSONLikeValue) {
  // Ensure that simple JavaScript objects work.
  // There are more exhaustive tests of JavaScript objects in V8.
  V8TestingScope scope;
  v8::Local<v8::Value> object = Eval("({ foo: [1, 2, 3], bar: 'baz' })", scope);
  DCHECK(object->IsObject());
  v8::Local<v8::Value> result = RoundTrip(object, scope);
  ASSERT_TRUE(result->IsObject());
  EXPECT_NE(object, result);
  EXPECT_EQ(ToJSON(object.As<v8::Object>(), scope),
            ToJSON(result.As<v8::Object>(), scope));
}

TEST(V8ScriptValueSerializerTest, ThrowsDataCloneError) {
  // Ensure that a proper DataCloneError DOMException is thrown when issues
  // are encountered in V8 (for example, cloning a symbol). It should be an
  // instance of DOMException, and it should have a proper descriptive
  // message.
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  ExceptionState exception_state(scope.GetIsolate(),
                                 ExceptionState::kExecutionContext, "Window",
                                 "postMessage");
  v8::Local<v8::Value> symbol = Eval("Symbol()", scope);
  DCHECK(symbol->IsSymbol());
  ASSERT_FALSE(
      V8ScriptValueSerializer(script_state).Serialize(symbol, exception_state));
  ASSERT_TRUE(HadDOMException("DataCloneError", script_state, exception_state));
  DOMException* dom_exception =
      V8DOMException::toImpl(exception_state.GetException().As<v8::Object>());
  EXPECT_TRUE(dom_exception->toString().Contains("postMessage"));
}

TEST(V8ScriptValueSerializerTest, RethrowsScriptError) {
  // Ensure that other exceptions, like those thrown by script, are properly
  // rethrown.
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  ExceptionState exception_state(scope.GetIsolate(),
                                 ExceptionState::kExecutionContext, "Window",
                                 "postMessage");
  v8::Local<v8::Value> exception = Eval("myException=new Error()", scope);
  v8::Local<v8::Value> object =
      Eval("({ get a() { throw myException; }})", scope);
  DCHECK(object->IsObject());
  ASSERT_FALSE(
      V8ScriptValueSerializer(script_state).Serialize(object, exception_state));
  ASSERT_TRUE(exception_state.HadException());
  EXPECT_EQ(exception, exception_state.GetException());
}

TEST(V8ScriptValueSerializerTest, DeserializationErrorReturnsNull) {
  // If there's a problem during deserialization, it results in null, but no
  // exception.
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  RefPtr<SerializedScriptValue> invalid =
      SerializedScriptValue::Create("invalid data");
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(script_state, invalid).Deserialize();
  EXPECT_TRUE(result->IsNull());
  EXPECT_FALSE(scope.GetExceptionState().HadException());
}

TEST(V8ScriptValueSerializerTest, NeuteringHappensAfterSerialization) {
  // This object will throw an exception before the [[Transfer]] step.
  // As a result, the ArrayBuffer will not be transferred.
  V8TestingScope scope;
  ExceptionState exception_state(scope.GetIsolate(),
                                 ExceptionState::kExecutionContext, "Window",
                                 "postMessage");

  DOMArrayBuffer* array_buffer = DOMArrayBuffer::Create(1, 1);
  ASSERT_FALSE(array_buffer->IsNeutered());
  v8::Local<v8::Value> object = Eval("({ get a() { throw 'party'; }})", scope);
  Transferables transferables;
  transferables.array_buffers.push_back(array_buffer);

  RoundTrip(object, scope, &exception_state, &transferables);
  ASSERT_TRUE(exception_state.HadException());
  EXPECT_FALSE(HadDOMException("DataCloneError", scope.GetScriptState(),
                               exception_state));
  EXPECT_FALSE(array_buffer->IsNeutered());
}

TEST(V8ScriptValueSerializerTest, RoundTripImageData) {
  // ImageData objects should serialize and deserialize correctly.
  V8TestingScope scope;
  ImageData* image_data = ImageData::Create(2, 1, ASSERT_NO_EXCEPTION);
  image_data->data()->Data()[0] = 200;
  v8::Local<v8::Value> wrapper =
      ToV8(image_data, scope.GetContext()->Global(), scope.GetIsolate());
  v8::Local<v8::Value> result = RoundTrip(wrapper, scope);
  ASSERT_TRUE(V8ImageData::hasInstance(result, scope.GetIsolate()));
  ImageData* new_image_data = V8ImageData::toImpl(result.As<v8::Object>());
  EXPECT_NE(image_data, new_image_data);
  EXPECT_EQ(image_data->Size(), new_image_data->Size());
  EXPECT_EQ(image_data->data()->length(), new_image_data->data()->length());
  EXPECT_EQ(200, new_image_data->data()->Data()[0]);
}

TEST(V8ScriptValueSerializerTest, DecodeImageDataV9) {
  // Backward compatibility with existing serialized ImageData objects must be
  // maintained. Add more cases if the format changes; don't remove tests for
  // old versions.
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  RefPtr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x23, 0x02, 0x01, 0x08, 0xc8,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(script_state, input).Deserialize();
  ASSERT_TRUE(V8ImageData::hasInstance(result, scope.GetIsolate()));
  ImageData* new_image_data = V8ImageData::toImpl(result.As<v8::Object>());
  EXPECT_EQ(IntSize(2, 1), new_image_data->Size());
  EXPECT_EQ(8u, new_image_data->data()->length());
  EXPECT_EQ(200, new_image_data->data()->Data()[0]);
}

TEST(V8ScriptValueSerializerTest, DecodeImageDataV16) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  RefPtr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x10, 0xff, 0x0c, 0x23, 0x02, 0x01, 0x08, 0xc8,
                       0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(script_state, input).Deserialize();
  ASSERT_TRUE(V8ImageData::hasInstance(result, scope.GetIsolate()));
  ImageData* new_image_data = V8ImageData::toImpl(result.As<v8::Object>());
  EXPECT_EQ(IntSize(2, 1), new_image_data->Size());
  EXPECT_EQ(8u, new_image_data->data()->length());
  EXPECT_EQ(200, new_image_data->data()->Data()[0]);
}

class WebMessagePortChannelImpl final : public WebMessagePortChannel {
 public:
  // WebMessagePortChannel
  void SetClient(WebMessagePortChannelClient* client) override {}
  void PostMessage(const WebString&, WebMessagePortChannelArray) {
    NOTIMPLEMENTED();
  }
  bool TryGetMessage(WebString*, WebMessagePortChannelArray&) { return false; }
};

MessagePort* MakeMessagePort(
    ExecutionContext* execution_context,
    WebMessagePortChannel** unowned_channel_out = nullptr) {
  std::unique_ptr<WebMessagePortChannelImpl> channel =
      WTF::MakeUnique<WebMessagePortChannelImpl>();
  auto* unowned_channel_ptr = channel.get();
  MessagePort* port = MessagePort::Create(*execution_context);
  port->Entangle(std::move(channel));
  EXPECT_TRUE(port->IsEntangled());
  EXPECT_EQ(unowned_channel_ptr, port->EntangledChannelForTesting());
  if (unowned_channel_out)
    *unowned_channel_out = unowned_channel_ptr;
  return port;
}

TEST(V8ScriptValueSerializerTest, RoundTripMessagePort) {
  V8TestingScope scope;

  WebMessagePortChannel* unowned_channel;
  MessagePort* port =
      MakeMessagePort(scope.GetExecutionContext(), &unowned_channel);
  v8::Local<v8::Value> wrapper = ToV8(port, scope.GetScriptState());
  Transferables transferables;
  transferables.message_ports.push_back(port);

  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, nullptr, &transferables);
  ASSERT_TRUE(V8MessagePort::hasInstance(result, scope.GetIsolate()));
  MessagePort* new_port = V8MessagePort::toImpl(result.As<v8::Object>());
  EXPECT_FALSE(port->IsEntangled());
  EXPECT_TRUE(new_port->IsEntangled());
  EXPECT_EQ(unowned_channel, new_port->EntangledChannelForTesting());
}

TEST(V8ScriptValueSerializerTest, NeuteredMessagePortThrowsDataCloneError) {
  V8TestingScope scope;
  ExceptionState exception_state(scope.GetIsolate(),
                                 ExceptionState::kExecutionContext, "Window",
                                 "postMessage");

  MessagePort* port = MessagePort::Create(*scope.GetExecutionContext());
  EXPECT_TRUE(port->IsNeutered());
  v8::Local<v8::Value> wrapper = ToV8(port, scope.GetScriptState());
  Transferables transferables;
  transferables.message_ports.push_back(port);

  RoundTrip(wrapper, scope, &exception_state, &transferables);
  ASSERT_TRUE(HadDOMException("DataCloneError", scope.GetScriptState(),
                              exception_state));
}

TEST(V8ScriptValueSerializerTest,
     UntransferredMessagePortThrowsDataCloneError) {
  V8TestingScope scope;
  ExceptionState exception_state(scope.GetIsolate(),
                                 ExceptionState::kExecutionContext, "Window",
                                 "postMessage");

  WebMessagePortChannel* unowned_channel;
  MessagePort* port =
      MakeMessagePort(scope.GetExecutionContext(), &unowned_channel);
  v8::Local<v8::Value> wrapper = ToV8(port, scope.GetScriptState());
  Transferables transferables;

  RoundTrip(wrapper, scope, &exception_state, &transferables);
  ASSERT_TRUE(HadDOMException("DataCloneError", scope.GetScriptState(),
                              exception_state));
}

TEST(V8ScriptValueSerializerTest, OutOfRangeMessagePortIndex) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  RefPtr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x4d, 0x01});
  MessagePort* port1 = MakeMessagePort(scope.GetExecutionContext());
  MessagePort* port2 = MakeMessagePort(scope.GetExecutionContext());
  {
    V8ScriptValueDeserializer deserializer(script_state, input);
    ASSERT_TRUE(deserializer.Deserialize()->IsNull());
  }
  {
    V8ScriptValueDeserializer::Options options;
    options.message_ports = new MessagePortArray;
    V8ScriptValueDeserializer deserializer(script_state, input, options);
    ASSERT_TRUE(deserializer.Deserialize()->IsNull());
  }
  {
    V8ScriptValueDeserializer::Options options;
    options.message_ports = new MessagePortArray;
    options.message_ports->push_back(port1);
    V8ScriptValueDeserializer deserializer(script_state, input, options);
    ASSERT_TRUE(deserializer.Deserialize()->IsNull());
  }
  {
    V8ScriptValueDeserializer::Options options;
    options.message_ports = new MessagePortArray;
    options.message_ports->push_back(port1);
    options.message_ports->push_back(port2);
    V8ScriptValueDeserializer deserializer(script_state, input, options);
    v8::Local<v8::Value> result = deserializer.Deserialize();
    ASSERT_TRUE(V8MessagePort::hasInstance(result, scope.GetIsolate()));
    EXPECT_EQ(port2, V8MessagePort::toImpl(result.As<v8::Object>()));
  }
}

// Decode tests for backward compatibility are not required for message ports
// because they cannot be persisted to disk.

// A more exhaustive set of ImageBitmap cases are covered by LayoutTests.
TEST(V8ScriptValueSerializerTest, RoundTripImageBitmap) {
  V8TestingScope scope;

  // Make a 10x7 red ImageBitmap.
  sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(10, 7);
  surface->getCanvas()->clear(SK_ColorRED);
  ImageBitmap* image_bitmap = ImageBitmap::Create(
      StaticBitmapImage::Create(surface->makeImageSnapshot()));

  // Serialize and deserialize it.
  v8::Local<v8::Value> wrapper = ToV8(image_bitmap, scope.GetScriptState());
  v8::Local<v8::Value> result = RoundTrip(wrapper, scope);
  ASSERT_TRUE(V8ImageBitmap::hasInstance(result, scope.GetIsolate()));
  ImageBitmap* new_image_bitmap =
      V8ImageBitmap::toImpl(result.As<v8::Object>());
  ASSERT_EQ(IntSize(10, 7), new_image_bitmap->Size());

  // Check that the pixel at (3, 3) is red.
  uint8_t pixel[4] = {};
  ASSERT_TRUE(
      new_image_bitmap->BitmapImage()->ImageForCurrentFrame()->readPixels(
          SkImageInfo::Make(1, 1, kRGBA_8888_SkColorType, kPremul_SkAlphaType),
          &pixel, 4, 3, 3));
  ASSERT_THAT(pixel, ::testing::ElementsAre(255, 0, 0, 255));
}

TEST(V8ScriptValueSerializerTest, DecodeImageBitmap) {
  // Backward compatibility with existing serialized ImageBitmap objects must be
  // maintained. Add more cases if the format changes; don't remove tests for
  // old versions.
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();

// This is checked by platform instead of by SK_PMCOLOR_BYTE_ORDER because
// this test intends to ensure that a platform can decode images it has
// previously written. At format version 9, Android writes RGBA and every
// other platform writes BGRA.
#if OS(ANDROID)
  RefPtr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x67, 0x01, 0x01, 0x02, 0x01,
                       0x08, 0xff, 0x00, 0x00, 0xff, 0x00, 0xff, 0x00, 0xff});
#else
  RefPtr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x67, 0x01, 0x01, 0x02, 0x01,
                       0x08, 0x00, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0xff});
#endif

  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(script_state, input).Deserialize();
  ASSERT_TRUE(V8ImageBitmap::hasInstance(result, scope.GetIsolate()));
  ImageBitmap* new_image_bitmap =
      V8ImageBitmap::toImpl(result.As<v8::Object>());
  ASSERT_EQ(IntSize(2, 1), new_image_bitmap->Size());

  // Check that the pixels are opaque red and green, respectively.
  uint8_t pixels[8] = {};
  ASSERT_TRUE(
      new_image_bitmap->BitmapImage()->ImageForCurrentFrame()->readPixels(
          SkImageInfo::Make(2, 1, kRGBA_8888_SkColorType, kPremul_SkAlphaType),
          &pixels, 8, 0, 0));
  ASSERT_THAT(pixels, ::testing::ElementsAre(255, 0, 0, 255, 0, 255, 0, 255));
}

TEST(V8ScriptValueSerializerTest, InvalidImageBitmapDecode) {
  V8TestingScope scope;
  ScriptState* script_state = scope.GetScriptState();
  {
    // Too many bytes declared in pixel data.
    RefPtr<SerializedScriptValue> input = SerializedValue(
        {0xff, 0x09, 0x3f, 0x00, 0x67, 0x01, 0x01, 0x02, 0x01, 0x09,
         0x00, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0xff, 0x00, 0x00});
    EXPECT_TRUE(
        V8ScriptValueDeserializer(script_state, input).Deserialize()->IsNull());
  }
  {
    // Too few bytes declared in pixel data.
    RefPtr<SerializedScriptValue> input =
        SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x67, 0x01, 0x01, 0x02, 0x01,
                         0x07, 0x00, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0xff});
    EXPECT_TRUE(
        V8ScriptValueDeserializer(script_state, input).Deserialize()->IsNull());
  }
  {
    // Nonsense for origin clean data.
    RefPtr<SerializedScriptValue> input =
        SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x67, 0x02, 0x01, 0x02, 0x01,
                         0x08, 0x00, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0xff});
    EXPECT_TRUE(
        V8ScriptValueDeserializer(script_state, input).Deserialize()->IsNull());
  }
  {
    // Nonsense for premultiplied bit.
    RefPtr<SerializedScriptValue> input =
        SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x67, 0x01, 0x02, 0x02, 0x01,
                         0x08, 0x00, 0x00, 0xff, 0xff, 0x00, 0xff, 0x00, 0xff});
    EXPECT_TRUE(
        V8ScriptValueDeserializer(script_state, input).Deserialize()->IsNull());
  }
}

TEST(V8ScriptValueSerializerTest, TransferImageBitmap) {
  // More thorough tests exist in LayoutTests/.
  V8TestingScope scope;

  sk_sp<SkSurface> surface = SkSurface::MakeRasterN32Premul(10, 7);
  surface->getCanvas()->clear(SK_ColorRED);
  sk_sp<SkImage> image = surface->makeImageSnapshot();
  ImageBitmap* image_bitmap =
      ImageBitmap::Create(StaticBitmapImage::Create(image));

  v8::Local<v8::Value> wrapper = ToV8(image_bitmap, scope.GetScriptState());
  Transferables transferables;
  transferables.image_bitmaps.push_back(image_bitmap);
  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, nullptr, &transferables);
  ASSERT_TRUE(V8ImageBitmap::hasInstance(result, scope.GetIsolate()));
  ImageBitmap* new_image_bitmap =
      V8ImageBitmap::toImpl(result.As<v8::Object>());
  ASSERT_EQ(IntSize(10, 7), new_image_bitmap->Size());

  // Check that the pixel at (3, 3) is red.
  uint8_t pixel[4] = {};
  sk_sp<SkImage> new_image =
      new_image_bitmap->BitmapImage()->ImageForCurrentFrame();
  ASSERT_TRUE(new_image->readPixels(
      SkImageInfo::Make(1, 1, kRGBA_8888_SkColorType, kPremul_SkAlphaType),
      &pixel, 4, 3, 3));
  ASSERT_THAT(pixel, ::testing::ElementsAre(255, 0, 0, 255));

  // Check also that the underlying image contents were transferred.
  EXPECT_EQ(image, new_image);
  EXPECT_TRUE(image_bitmap->IsNeutered());
}

TEST(V8ScriptValueSerializerTest, TransferOffscreenCanvas) {
  // More exhaustive tests in LayoutTests/. This is a sanity check.
  V8TestingScope scope;
  OffscreenCanvas* canvas = OffscreenCanvas::Create(10, 7);
  canvas->SetPlaceholderCanvasId(519);
  v8::Local<v8::Value> wrapper = ToV8(canvas, scope.GetScriptState());
  Transferables transferables;
  transferables.offscreen_canvases.push_back(canvas);
  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, nullptr, &transferables);
  ASSERT_TRUE(V8OffscreenCanvas::hasInstance(result, scope.GetIsolate()));
  OffscreenCanvas* new_canvas =
      V8OffscreenCanvas::toImpl(result.As<v8::Object>());
  EXPECT_EQ(IntSize(10, 7), new_canvas->Size());
  EXPECT_EQ(519u, new_canvas->PlaceholderCanvasId());
  EXPECT_TRUE(canvas->IsNeutered());
  EXPECT_FALSE(new_canvas->IsNeutered());
}

TEST(V8ScriptValueSerializerTest, RoundTripBlob) {
  V8TestingScope scope;
  const char kHelloWorld[] = "Hello world!";
  Blob* blob =
      Blob::Create(reinterpret_cast<const unsigned char*>(&kHelloWorld),
                   sizeof(kHelloWorld), "text/plain");
  String uuid = blob->Uuid();
  EXPECT_FALSE(uuid.IsEmpty());
  v8::Local<v8::Value> wrapper = ToV8(blob, scope.GetScriptState());
  v8::Local<v8::Value> result = RoundTrip(wrapper, scope);
  ASSERT_TRUE(V8Blob::hasInstance(result, scope.GetIsolate()));
  Blob* new_blob = V8Blob::toImpl(result.As<v8::Object>());
  EXPECT_EQ("text/plain", new_blob->type());
  EXPECT_EQ(sizeof(kHelloWorld), new_blob->size());
  EXPECT_EQ(uuid, new_blob->Uuid());
}

TEST(V8ScriptValueSerializerTest, DecodeBlob) {
  V8TestingScope scope;
  RefPtr<SerializedScriptValue> input = SerializedValue(
      {0xff, 0x09, 0x3f, 0x00, 0x62, 0x24, 0x64, 0x38, 0x37, 0x35, 0x64,
       0x66, 0x63, 0x32, 0x2d, 0x34, 0x35, 0x30, 0x35, 0x2d, 0x34, 0x36,
       0x31, 0x62, 0x2d, 0x39, 0x38, 0x66, 0x65, 0x2d, 0x30, 0x63, 0x66,
       0x36, 0x63, 0x63, 0x35, 0x65, 0x61, 0x66, 0x34, 0x34, 0x0a, 0x74,
       0x65, 0x78, 0x74, 0x2f, 0x70, 0x6c, 0x61, 0x69, 0x6e, 0x0c});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(scope.GetScriptState(), input).Deserialize();
  ASSERT_TRUE(V8Blob::hasInstance(result, scope.GetIsolate()));
  Blob* new_blob = V8Blob::toImpl(result.As<v8::Object>());
  EXPECT_EQ("d875dfc2-4505-461b-98fe-0cf6cc5eaf44", new_blob->Uuid());
  EXPECT_EQ("text/plain", new_blob->type());
  EXPECT_EQ(12u, new_blob->size());
}

TEST(V8ScriptValueSerializerTest, RoundTripBlobIndex) {
  V8TestingScope scope;
  const char kHelloWorld[] = "Hello world!";
  Blob* blob =
      Blob::Create(reinterpret_cast<const unsigned char*>(&kHelloWorld),
                   sizeof(kHelloWorld), "text/plain");
  String uuid = blob->Uuid();
  EXPECT_FALSE(uuid.IsEmpty());
  v8::Local<v8::Value> wrapper = ToV8(blob, scope.GetScriptState());
  WebBlobInfoArray blob_info_array;
  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, nullptr, nullptr, &blob_info_array);

  // As before, the resulting blob should be correct.
  ASSERT_TRUE(V8Blob::hasInstance(result, scope.GetIsolate()));
  Blob* new_blob = V8Blob::toImpl(result.As<v8::Object>());
  EXPECT_EQ("text/plain", new_blob->type());
  EXPECT_EQ(sizeof(kHelloWorld), new_blob->size());
  EXPECT_EQ(uuid, new_blob->Uuid());

  // The blob info array should also contain the blob details since it was
  // serialized by index into this array.
  ASSERT_EQ(1u, blob_info_array.size());
  const WebBlobInfo& info = blob_info_array[0];
  EXPECT_FALSE(info.IsFile());
  EXPECT_EQ(uuid, String(info.Uuid()));
  EXPECT_EQ("text/plain", info.GetType());
  EXPECT_EQ(sizeof(kHelloWorld), static_cast<size_t>(info.size()));
}

TEST(V8ScriptValueSerializerTest, DecodeBlobIndex) {
  V8TestingScope scope;
  RefPtr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x69, 0x00});
  WebBlobInfoArray blob_info_array;
  blob_info_array.emplace_back("d875dfc2-4505-461b-98fe-0cf6cc5eaf44",
                               "text/plain", 12);
  V8ScriptValueDeserializer::Options options;
  options.blob_info = &blob_info_array;
  V8ScriptValueDeserializer deserializer(scope.GetScriptState(), input,
                                         options);
  v8::Local<v8::Value> result = deserializer.Deserialize();
  ASSERT_TRUE(V8Blob::hasInstance(result, scope.GetIsolate()));
  Blob* new_blob = V8Blob::toImpl(result.As<v8::Object>());
  EXPECT_EQ("d875dfc2-4505-461b-98fe-0cf6cc5eaf44", new_blob->Uuid());
  EXPECT_EQ("text/plain", new_blob->type());
  EXPECT_EQ(12u, new_blob->size());
}

TEST(V8ScriptValueSerializerTest, DecodeBlobIndexOutOfRange) {
  V8TestingScope scope;
  RefPtr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x69, 0x01});
  {
    V8ScriptValueDeserializer deserializer(scope.GetScriptState(), input);
    ASSERT_TRUE(deserializer.Deserialize()->IsNull());
  }
  {
    WebBlobInfoArray blob_info_array;
    blob_info_array.emplace_back("d875dfc2-4505-461b-98fe-0cf6cc5eaf44",
                                 "text/plain", 12);
    V8ScriptValueDeserializer::Options options;
    options.blob_info = &blob_info_array;
    V8ScriptValueDeserializer deserializer(scope.GetScriptState(), input,
                                           options);
    ASSERT_TRUE(deserializer.Deserialize()->IsNull());
  }
}

TEST(V8ScriptValueSerializerTest, RoundTripFileNative) {
  V8TestingScope scope;
  File* file = File::Create("/native/path");
  v8::Local<v8::Value> wrapper = ToV8(file, scope.GetScriptState());
  v8::Local<v8::Value> result = RoundTrip(wrapper, scope);
  ASSERT_TRUE(V8File::hasInstance(result, scope.GetIsolate()));
  File* new_file = V8File::toImpl(result.As<v8::Object>());
  EXPECT_TRUE(new_file->HasBackingFile());
  EXPECT_EQ("/native/path", new_file->GetPath());
  EXPECT_TRUE(new_file->FileSystemURL().IsEmpty());
}

TEST(V8ScriptValueSerializerTest, RoundTripFileBackedByBlob) {
  V8TestingScope scope;
  const double kModificationTime = 0.0;
  RefPtr<BlobDataHandle> blob_data_handle = BlobDataHandle::Create();
  File* file =
      File::Create("/native/path", kModificationTime, blob_data_handle);
  v8::Local<v8::Value> wrapper = ToV8(file, scope.GetScriptState());
  v8::Local<v8::Value> result = RoundTrip(wrapper, scope);
  ASSERT_TRUE(V8File::hasInstance(result, scope.GetIsolate()));
  File* new_file = V8File::toImpl(result.As<v8::Object>());
  EXPECT_FALSE(new_file->HasBackingFile());
  EXPECT_TRUE(file->GetPath().IsEmpty());
  EXPECT_TRUE(new_file->FileSystemURL().IsEmpty());
}

TEST(V8ScriptValueSerializerTest, RoundTripFileNativeSnapshot) {
  V8TestingScope scope;
  FileMetadata metadata;
  metadata.platform_path = "/native/snapshot";
  File* file =
      File::CreateForFileSystemFile("name", metadata, File::kIsUserVisible);
  v8::Local<v8::Value> wrapper = ToV8(file, scope.GetScriptState());
  v8::Local<v8::Value> result = RoundTrip(wrapper, scope);
  ASSERT_TRUE(V8File::hasInstance(result, scope.GetIsolate()));
  File* new_file = V8File::toImpl(result.As<v8::Object>());
  EXPECT_TRUE(new_file->HasBackingFile());
  EXPECT_EQ("/native/snapshot", new_file->GetPath());
  EXPECT_TRUE(new_file->FileSystemURL().IsEmpty());
}

TEST(V8ScriptValueSerializerTest, RoundTripFileNonNativeSnapshot) {
  // Preserving behavior, filesystem URL is not preserved across cloning.
  V8TestingScope scope;
  KURL url(kParsedURLString,
           "filesystem:http://example.com/isolated/hash/non-native-file");
  File* file =
      File::CreateForFileSystemFile(url, FileMetadata(), File::kIsUserVisible);
  v8::Local<v8::Value> wrapper = ToV8(file, scope.GetScriptState());
  v8::Local<v8::Value> result = RoundTrip(wrapper, scope);
  ASSERT_TRUE(V8File::hasInstance(result, scope.GetIsolate()));
  File* new_file = V8File::toImpl(result.As<v8::Object>());
  EXPECT_FALSE(new_file->HasBackingFile());
  EXPECT_TRUE(file->GetPath().IsEmpty());
  EXPECT_TRUE(new_file->FileSystemURL().IsEmpty());
}

// Used for checking that times provided are between now and the current time
// when the checker was constructed, according to WTF::currentTime.
class TimeIntervalChecker {
 public:
  TimeIntervalChecker() : start_time_(WTF::CurrentTime()) {}
  bool WasAliveAt(double time_in_milliseconds) {
    double time = time_in_milliseconds / kMsPerSecond;
    return start_time_ <= time && time <= WTF::CurrentTime();
  }

 private:
  const double start_time_;
};

TEST(V8ScriptValueSerializerTest, DecodeFileV3) {
  V8TestingScope scope;
  TimeIntervalChecker time_interval_checker;
  RefPtr<SerializedScriptValue> input = SerializedValue(
      {0xff, 0x03, 0x3f, 0x00, 0x66, 0x04, 'p', 'a', 't', 'h', 0x24, 'f',
       '4',  'a',  '6',  'e',  'd',  'd',  '5', '-', '6', '5', 'a',  'd',
       '-',  '4',  'd',  'c',  '3',  '-',  'b', '6', '7', 'c', '-',  'a',
       '7',  '7',  '9',  'c',  '0',  '2',  'f', '0', 'f', 'a', '3',  0x0a,
       't',  'e',  'x',  't',  '/',  'p',  'l', 'a', 'i', 'n'});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(scope.GetScriptState(), input).Deserialize();
  ASSERT_TRUE(V8File::hasInstance(result, scope.GetIsolate()));
  File* new_file = V8File::toImpl(result.As<v8::Object>());
  EXPECT_EQ("path", new_file->GetPath());
  EXPECT_EQ("f4a6edd5-65ad-4dc3-b67c-a779c02f0fa3", new_file->Uuid());
  EXPECT_EQ("text/plain", new_file->type());
  EXPECT_FALSE(new_file->HasValidSnapshotMetadata());
  EXPECT_EQ(0u, new_file->size());
  EXPECT_TRUE(time_interval_checker.WasAliveAt(new_file->lastModifiedDate()));
  EXPECT_EQ(File::kIsUserVisible, new_file->GetUserVisibility());
}

TEST(V8ScriptValueSerializerTest, DecodeFileV4) {
  V8TestingScope scope;
  TimeIntervalChecker time_interval_checker;
  RefPtr<SerializedScriptValue> input = SerializedValue(
      {0xff, 0x04, 0x3f, 0x00, 0x66, 0x04, 'p', 'a',  't',  'h', 0x04, 'n',
       'a',  'm',  'e',  0x03, 'r',  'e',  'l', 0x24, 'f',  '4', 'a',  '6',
       'e',  'd',  'd',  '5',  '-',  '6',  '5', 'a',  'd',  '-', '4',  'd',
       'c',  '3',  '-',  'b',  '6',  '7',  'c', '-',  'a',  '7', '7',  '9',
       'c',  '0',  '2',  'f',  '0',  'f',  'a', '3',  0x0a, 't', 'e',  'x',
       't',  '/',  'p',  'l',  'a',  'i',  'n', 0x00});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(scope.GetScriptState(), input).Deserialize();
  ASSERT_TRUE(V8File::hasInstance(result, scope.GetIsolate()));
  File* new_file = V8File::toImpl(result.As<v8::Object>());
  EXPECT_EQ("path", new_file->GetPath());
  EXPECT_EQ("name", new_file->name());
  EXPECT_EQ("rel", new_file->webkitRelativePath());
  EXPECT_EQ("f4a6edd5-65ad-4dc3-b67c-a779c02f0fa3", new_file->Uuid());
  EXPECT_EQ("text/plain", new_file->type());
  EXPECT_FALSE(new_file->HasValidSnapshotMetadata());
  EXPECT_EQ(0u, new_file->size());
  EXPECT_TRUE(time_interval_checker.WasAliveAt(new_file->lastModifiedDate()));
  EXPECT_EQ(File::kIsUserVisible, new_file->GetUserVisibility());
}

TEST(V8ScriptValueSerializerTest, DecodeFileV4WithSnapshot) {
  V8TestingScope scope;
  RefPtr<SerializedScriptValue> input = SerializedValue(
      {0xff, 0x04, 0x3f, 0x00, 0x66, 0x04, 'p', 'a',  't',  'h',  0x04, 'n',
       'a',  'm',  'e',  0x03, 'r',  'e',  'l', 0x24, 'f',  '4',  'a',  '6',
       'e',  'd',  'd',  '5',  '-',  '6',  '5', 'a',  'd',  '-',  '4',  'd',
       'c',  '3',  '-',  'b',  '6',  '7',  'c', '-',  'a',  '7',  '7',  '9',
       'c',  '0',  '2',  'f',  '0',  'f',  'a', '3',  0x0a, 't',  'e',  'x',
       't',  '/',  'p',  'l',  'a',  'i',  'n', 0x01, 0x80, 0x04, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0xd0, 0xbf});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(scope.GetScriptState(), input).Deserialize();
  ASSERT_TRUE(V8File::hasInstance(result, scope.GetIsolate()));
  File* new_file = V8File::toImpl(result.As<v8::Object>());
  EXPECT_EQ("path", new_file->GetPath());
  EXPECT_EQ("name", new_file->name());
  EXPECT_EQ("rel", new_file->webkitRelativePath());
  EXPECT_EQ("f4a6edd5-65ad-4dc3-b67c-a779c02f0fa3", new_file->Uuid());
  EXPECT_EQ("text/plain", new_file->type());
  EXPECT_TRUE(new_file->HasValidSnapshotMetadata());
  EXPECT_EQ(512u, new_file->size());
  // From v4 to v7, the last modified time is written in seconds.
  // So -0.25 represents 250 ms before the Unix epoch.
  EXPECT_EQ(-250.0, new_file->lastModifiedDate());
}

TEST(V8ScriptValueSerializerTest, DecodeFileV7) {
  V8TestingScope scope;
  TimeIntervalChecker time_interval_checker;
  RefPtr<SerializedScriptValue> input = SerializedValue(
      {0xff, 0x07, 0x3f, 0x00, 0x66, 0x04, 'p', 'a',  't',  'h', 0x04, 'n',
       'a',  'm',  'e',  0x03, 'r',  'e',  'l', 0x24, 'f',  '4', 'a',  '6',
       'e',  'd',  'd',  '5',  '-',  '6',  '5', 'a',  'd',  '-', '4',  'd',
       'c',  '3',  '-',  'b',  '6',  '7',  'c', '-',  'a',  '7', '7',  '9',
       'c',  '0',  '2',  'f',  '0',  'f',  'a', '3',  0x0a, 't', 'e',  'x',
       't',  '/',  'p',  'l',  'a',  'i',  'n', 0x00, 0x00, 0x00});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(scope.GetScriptState(), input).Deserialize();
  ASSERT_TRUE(V8File::hasInstance(result, scope.GetIsolate()));
  File* new_file = V8File::toImpl(result.As<v8::Object>());
  EXPECT_EQ("path", new_file->GetPath());
  EXPECT_EQ("name", new_file->name());
  EXPECT_EQ("rel", new_file->webkitRelativePath());
  EXPECT_EQ("f4a6edd5-65ad-4dc3-b67c-a779c02f0fa3", new_file->Uuid());
  EXPECT_EQ("text/plain", new_file->type());
  EXPECT_FALSE(new_file->HasValidSnapshotMetadata());
  EXPECT_EQ(0u, new_file->size());
  EXPECT_TRUE(time_interval_checker.WasAliveAt(new_file->lastModifiedDate()));
  EXPECT_EQ(File::kIsNotUserVisible, new_file->GetUserVisibility());
}

TEST(V8ScriptValueSerializerTest, DecodeFileV8WithSnapshot) {
  V8TestingScope scope;
  RefPtr<SerializedScriptValue> input = SerializedValue(
      {0xff, 0x08, 0x3f, 0x00, 0x66, 0x04, 'p',  'a',  't',  'h',  0x04, 'n',
       'a',  'm',  'e',  0x03, 'r',  'e',  'l',  0x24, 'f',  '4',  'a',  '6',
       'e',  'd',  'd',  '5',  '-',  '6',  '5',  'a',  'd',  '-',  '4',  'd',
       'c',  '3',  '-',  'b',  '6',  '7',  'c',  '-',  'a',  '7',  '7',  '9',
       'c',  '0',  '2',  'f',  '0',  'f',  'a',  '3',  0x0a, 't',  'e',  'x',
       't',  '/',  'p',  'l',  'a',  'i',  'n',  0x01, 0x80, 0x04, 0x00, 0x00,
       0x00, 0x00, 0x00, 0x00, 0xd0, 0xbf, 0x01, 0x00});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(scope.GetScriptState(), input).Deserialize();
  ASSERT_TRUE(V8File::hasInstance(result, scope.GetIsolate()));
  File* new_file = V8File::toImpl(result.As<v8::Object>());
  EXPECT_EQ("path", new_file->GetPath());
  EXPECT_EQ("name", new_file->name());
  EXPECT_EQ("rel", new_file->webkitRelativePath());
  EXPECT_EQ("f4a6edd5-65ad-4dc3-b67c-a779c02f0fa3", new_file->Uuid());
  EXPECT_EQ("text/plain", new_file->type());
  EXPECT_TRUE(new_file->HasValidSnapshotMetadata());
  EXPECT_EQ(512u, new_file->size());
  // From v8, the last modified time is written in milliseconds.
  // So -0.25 represents 0.25 ms before the Unix epoch.
  EXPECT_EQ(-0.25, new_file->lastModifiedDate());
  EXPECT_EQ(File::kIsUserVisible, new_file->GetUserVisibility());
}

TEST(V8ScriptValueSerializerTest, RoundTripFileIndex) {
  V8TestingScope scope;
  File* file = File::Create("/native/path");
  v8::Local<v8::Value> wrapper = ToV8(file, scope.GetScriptState());
  WebBlobInfoArray blob_info_array;
  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, nullptr, nullptr, &blob_info_array);

  // As above, the resulting blob should be correct.
  ASSERT_TRUE(V8File::hasInstance(result, scope.GetIsolate()));
  File* new_file = V8File::toImpl(result.As<v8::Object>());
  EXPECT_TRUE(new_file->HasBackingFile());
  EXPECT_EQ("/native/path", new_file->GetPath());
  EXPECT_TRUE(new_file->FileSystemURL().IsEmpty());

  // The blob info array should also contain the details since it was serialized
  // by index into this array.
  ASSERT_EQ(1u, blob_info_array.size());
  const WebBlobInfo& info = blob_info_array[0];
  EXPECT_TRUE(info.IsFile());
  EXPECT_EQ("/native/path", info.FilePath());
  EXPECT_EQ(file->Uuid(), String(info.Uuid()));
}

TEST(V8ScriptValueSerializerTest, DecodeFileIndex) {
  V8TestingScope scope;
  RefPtr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x65, 0x00});
  WebBlobInfoArray blob_info_array;
  blob_info_array.emplace_back("d875dfc2-4505-461b-98fe-0cf6cc5eaf44",
                               "/native/path", "path", "text/plain");
  V8ScriptValueDeserializer::Options options;
  options.blob_info = &blob_info_array;
  V8ScriptValueDeserializer deserializer(scope.GetScriptState(), input,
                                         options);
  v8::Local<v8::Value> result = deserializer.Deserialize();
  ASSERT_TRUE(V8File::hasInstance(result, scope.GetIsolate()));
  File* new_file = V8File::toImpl(result.As<v8::Object>());
  EXPECT_EQ("d875dfc2-4505-461b-98fe-0cf6cc5eaf44", new_file->Uuid());
  EXPECT_EQ("text/plain", new_file->type());
  EXPECT_EQ("/native/path", new_file->GetPath());
  EXPECT_EQ("path", new_file->name());
}

TEST(V8ScriptValueSerializerTest, DecodeFileIndexOutOfRange) {
  V8TestingScope scope;
  RefPtr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x65, 0x01});
  {
    V8ScriptValueDeserializer deserializer(scope.GetScriptState(), input);
    ASSERT_TRUE(deserializer.Deserialize()->IsNull());
  }
  {
    WebBlobInfoArray blob_info_array;
    blob_info_array.emplace_back("d875dfc2-4505-461b-98fe-0cf6cc5eaf44",
                                 "/native/path", "path", "text/plain");
    V8ScriptValueDeserializer::Options options;
    options.blob_info = &blob_info_array;
    V8ScriptValueDeserializer deserializer(scope.GetScriptState(), input,
                                           options);
    ASSERT_TRUE(deserializer.Deserialize()->IsNull());
  }
}

// Most of the logic for FileList is shared with File, so the tests here are
// fairly basic.

TEST(V8ScriptValueSerializerTest, RoundTripFileList) {
  V8TestingScope scope;
  FileList* file_list = FileList::Create();
  file_list->Append(File::Create("/native/path"));
  file_list->Append(File::Create("/native/path2"));
  v8::Local<v8::Value> wrapper = ToV8(file_list, scope.GetScriptState());
  v8::Local<v8::Value> result = RoundTrip(wrapper, scope);
  ASSERT_TRUE(V8FileList::hasInstance(result, scope.GetIsolate()));
  FileList* new_file_list = V8FileList::toImpl(result.As<v8::Object>());
  ASSERT_EQ(2u, new_file_list->length());
  EXPECT_EQ("/native/path", new_file_list->item(0)->GetPath());
  EXPECT_EQ("/native/path2", new_file_list->item(1)->GetPath());
}

TEST(V8ScriptValueSerializerTest, DecodeEmptyFileList) {
  V8TestingScope scope;
  RefPtr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x6c, 0x00});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(scope.GetScriptState(), input).Deserialize();
  ASSERT_TRUE(V8FileList::hasInstance(result, scope.GetIsolate()));
  FileList* new_file_list = V8FileList::toImpl(result.As<v8::Object>());
  EXPECT_EQ(0u, new_file_list->length());
}

TEST(V8ScriptValueSerializerTest, DecodeFileListWithInvalidLength) {
  V8TestingScope scope;
  RefPtr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x6c, 0x01});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(scope.GetScriptState(), input).Deserialize();
  EXPECT_TRUE(result->IsNull());
}

TEST(V8ScriptValueSerializerTest, DecodeFileListV8WithoutSnapshot) {
  V8TestingScope scope;
  TimeIntervalChecker time_interval_checker;
  RefPtr<SerializedScriptValue> input = SerializedValue(
      {0xff, 0x08, 0x3f, 0x00, 0x6c, 0x01, 0x04, 'p', 'a',  't',  'h', 0x04,
       'n',  'a',  'm',  'e',  0x03, 'r',  'e',  'l', 0x24, 'f',  '4', 'a',
       '6',  'e',  'd',  'd',  '5',  '-',  '6',  '5', 'a',  'd',  '-', '4',
       'd',  'c',  '3',  '-',  'b',  '6',  '7',  'c', '-',  'a',  '7', '7',
       '9',  'c',  '0',  '2',  'f',  '0',  'f',  'a', '3',  0x0a, 't', 'e',
       'x',  't',  '/',  'p',  'l',  'a',  'i',  'n', 0x00, 0x00});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(scope.GetScriptState(), input).Deserialize();
  ASSERT_TRUE(V8FileList::hasInstance(result, scope.GetIsolate()));
  FileList* new_file_list = V8FileList::toImpl(result.As<v8::Object>());
  EXPECT_EQ(1u, new_file_list->length());
  File* new_file = new_file_list->item(0);
  EXPECT_EQ("path", new_file->GetPath());
  EXPECT_EQ("name", new_file->name());
  EXPECT_EQ("rel", new_file->webkitRelativePath());
  EXPECT_EQ("f4a6edd5-65ad-4dc3-b67c-a779c02f0fa3", new_file->Uuid());
  EXPECT_EQ("text/plain", new_file->type());
  EXPECT_FALSE(new_file->HasValidSnapshotMetadata());
  EXPECT_EQ(0u, new_file->size());
  EXPECT_TRUE(time_interval_checker.WasAliveAt(new_file->lastModifiedDate()));
  EXPECT_EQ(File::kIsNotUserVisible, new_file->GetUserVisibility());
}

TEST(V8ScriptValueSerializerTest, RoundTripFileListIndex) {
  V8TestingScope scope;
  FileList* file_list = FileList::Create();
  file_list->Append(File::Create("/native/path"));
  file_list->Append(File::Create("/native/path2"));
  v8::Local<v8::Value> wrapper = ToV8(file_list, scope.GetScriptState());
  WebBlobInfoArray blob_info_array;
  v8::Local<v8::Value> result =
      RoundTrip(wrapper, scope, nullptr, nullptr, &blob_info_array);

  // FileList should be produced correctly.
  ASSERT_TRUE(V8FileList::hasInstance(result, scope.GetIsolate()));
  FileList* new_file_list = V8FileList::toImpl(result.As<v8::Object>());
  ASSERT_EQ(2u, new_file_list->length());
  EXPECT_EQ("/native/path", new_file_list->item(0)->GetPath());
  EXPECT_EQ("/native/path2", new_file_list->item(1)->GetPath());

  // And the blob info array should be populated.
  ASSERT_EQ(2u, blob_info_array.size());
  EXPECT_TRUE(blob_info_array[0].IsFile());
  EXPECT_EQ("/native/path", blob_info_array[0].FilePath());
  EXPECT_TRUE(blob_info_array[1].IsFile());
  EXPECT_EQ("/native/path2", blob_info_array[1].FilePath());
}

TEST(V8ScriptValueSerializerTest, DecodeEmptyFileListIndex) {
  V8TestingScope scope;
  RefPtr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x4c, 0x00});
  WebBlobInfoArray blob_info_array;
  V8ScriptValueDeserializer::Options options;
  options.blob_info = &blob_info_array;
  V8ScriptValueDeserializer deserializer(scope.GetScriptState(), input,
                                         options);
  v8::Local<v8::Value> result = deserializer.Deserialize();
  ASSERT_TRUE(V8FileList::hasInstance(result, scope.GetIsolate()));
  FileList* new_file_list = V8FileList::toImpl(result.As<v8::Object>());
  EXPECT_EQ(0u, new_file_list->length());
}

TEST(V8ScriptValueSerializerTest, DecodeFileListIndexWithInvalidLength) {
  V8TestingScope scope;
  RefPtr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x4c, 0x02});
  WebBlobInfoArray blob_info_array;
  V8ScriptValueDeserializer::Options options;
  options.blob_info = &blob_info_array;
  V8ScriptValueDeserializer deserializer(scope.GetScriptState(), input,
                                         options);
  v8::Local<v8::Value> result = deserializer.Deserialize();
  EXPECT_TRUE(result->IsNull());
}

TEST(V8ScriptValueSerializerTest, DecodeFileListIndex) {
  V8TestingScope scope;
  RefPtr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x09, 0x3f, 0x00, 0x4c, 0x01, 0x00, 0x00});
  WebBlobInfoArray blob_info_array;
  blob_info_array.emplace_back("d875dfc2-4505-461b-98fe-0cf6cc5eaf44",
                               "/native/path", "name", "text/plain");
  V8ScriptValueDeserializer::Options options;
  options.blob_info = &blob_info_array;
  V8ScriptValueDeserializer deserializer(scope.GetScriptState(), input,
                                         options);
  v8::Local<v8::Value> result = deserializer.Deserialize();
  FileList* new_file_list = V8FileList::toImpl(result.As<v8::Object>());
  EXPECT_EQ(1u, new_file_list->length());
  File* new_file = new_file_list->item(0);
  EXPECT_EQ("/native/path", new_file->GetPath());
  EXPECT_EQ("name", new_file->name());
  EXPECT_EQ("d875dfc2-4505-461b-98fe-0cf6cc5eaf44", new_file->Uuid());
  EXPECT_EQ("text/plain", new_file->type());
}

class ScopedEnableCompositorWorker {
 public:
  ScopedEnableCompositorWorker()
      : was_enabled_(RuntimeEnabledFeatures::compositorWorkerEnabled()) {
    RuntimeEnabledFeatures::setCompositorWorkerEnabled(true);
  }
  ~ScopedEnableCompositorWorker() {
    RuntimeEnabledFeatures::setCompositorWorkerEnabled(was_enabled_);
  }

 private:
  bool was_enabled_;
};

TEST(V8ScriptValueSerializerTest, RoundTripCompositorProxy) {
  ScopedEnableCompositorWorker enable_compositor_worker;
  V8TestingScope scope;
  HTMLElement* element = scope.GetDocument().body();
  Vector<String> properties{"transform"};
  CompositorProxy* proxy = CompositorProxy::Create(
      scope.GetExecutionContext(), element, properties, ASSERT_NO_EXCEPTION);
  uint64_t element_id = proxy->ElementId();

  v8::Local<v8::Value> wrapper = ToV8(proxy, scope.GetScriptState());
  v8::Local<v8::Value> result = RoundTrip(wrapper, scope);
  ASSERT_TRUE(V8CompositorProxy::hasInstance(result, scope.GetIsolate()));
  CompositorProxy* new_proxy =
      V8CompositorProxy::toImpl(result.As<v8::Object>());
  EXPECT_EQ(element_id, new_proxy->ElementId());
  EXPECT_EQ(CompositorMutableProperty::kTransform,
            new_proxy->CompositorMutableProperties());
}

TEST(V8ScriptValueSerializerTest, CompositorProxyBadElementDeserialization) {
  ScopedEnableCompositorWorker enable_compositor_worker;
  V8TestingScope scope;

  // Normally a CompositorProxy will not be constructed with an element id of 0,
  // but we force it here to test the deserialization code.
  uint8_t element_id = 0;
  uint8_t mutable_properties = CompositorMutableProperty::kOpacity;
  RefPtr<SerializedScriptValue> input = SerializedValue(
      {0xff, 0x09, 0x3f, 0x00, 0x43, element_id, mutable_properties, 0x00});
  v8::Local<v8::Value> result =
      V8ScriptValueDeserializer(scope.GetScriptState(), input).Deserialize();

  EXPECT_TRUE(result->IsNull());
}

// Decode tests aren't included here because they're slightly non-trivial (an
// element with the right ID must actually exist) and this feature is both
// unshipped and likely to not use this mechanism when it does.
// TODO(jbroman): Update this if that turns out not to be the case.

TEST(V8ScriptValueSerializerTest, DecodeHardcodedNullValue) {
  V8TestingScope scope;
  EXPECT_TRUE(V8ScriptValueDeserializer(scope.GetScriptState(),
                                        SerializedScriptValue::NullValue())
                  .Deserialize()
                  ->IsNull());
}

// This is not the most efficient way to write a small version, but it's
// technically admissible. We should handle this in a consistent way to avoid
// DCHECK failure. Thus this is "true" encoded slightly strangely.
TEST(V8ScriptValueSerializerTest, DecodeWithInefficientVersionEnvelope) {
  V8TestingScope scope;
  RefPtr<SerializedScriptValue> input =
      SerializedValue({0xff, 0x80, 0x09, 0xff, 0x09, 0x54});
  EXPECT_TRUE(
      V8ScriptValueDeserializer(scope.GetScriptState(), std::move(input))
          .Deserialize()
          ->IsTrue());
}

}  // namespace
}  // namespace blink
