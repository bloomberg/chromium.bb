// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "bindings/core/v8/ExceptionState.h"
#include "bindings/core/v8/V8BindingForTesting.h"
#include "core/testing/NullExecutionContext.h"
#include "modules/mediastream/MediaDevices.h"
#include "modules/mediastream/MediaStreamConstraints.h"
#include "mojo/public/cpp/bindings/binding.h"
#include "platform/testing/TestingPlatformSupport.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using blink::mojom::blink::MediaDeviceInfoPtr;
using blink::mojom::blink::MediaDeviceType;

namespace blink {

const char kFakeAudioInputDeviceId1[] = "fake_audio_input 1";
const char kFakeAudioInputDeviceId2[] = "fake_audio_input 2";
const char kFakeVideoInputDeviceId1[] = "fake_video_input 1";
const char kFakeVideoInputDeviceId2[] = "fake_video_input 2";
const char kFakeAudioOutputDeviceId1[] = "fake_audio_output 1";

class MockMediaDevicesDispatcherHost
    : public mojom::blink::MediaDevicesDispatcherHost {
 public:
  MockMediaDevicesDispatcherHost() : binding_(this) {}

  void EnumerateDevices(bool request_audio_input,
                        bool request_video_input,
                        bool request_audio_output,
                        EnumerateDevicesCallback callback) override {
    Vector<Vector<mojom::blink::MediaDeviceInfoPtr>> enumeration(
        static_cast<size_t>(MediaDeviceType::NUM_MEDIA_DEVICE_TYPES));
    MediaDeviceInfoPtr device_info;
    if (request_audio_input) {
      device_info = mojom::blink::MediaDeviceInfo::New();
      device_info->device_id = kFakeAudioInputDeviceId1;
      device_info->label = "Fake Audio Input 1";
      device_info->group_id = "fake_group 1";
      enumeration[static_cast<size_t>(MediaDeviceType::MEDIA_AUDIO_INPUT)]
          .push_back(std::move(device_info));

      device_info = mojom::blink::MediaDeviceInfo::New();
      device_info->device_id = kFakeAudioInputDeviceId2;
      device_info->label = "Fake Audio Input 2";
      device_info->group_id = "fake_group 2";
      enumeration[static_cast<size_t>(MediaDeviceType::MEDIA_AUDIO_INPUT)]
          .push_back(std::move(device_info));
    }
    if (request_video_input) {
      device_info = mojom::blink::MediaDeviceInfo::New();
      device_info->device_id = kFakeVideoInputDeviceId1;
      device_info->label = "Fake Video Input 1";
      device_info->group_id = "";
      enumeration[static_cast<size_t>(MediaDeviceType::MEDIA_VIDEO_INPUT)]
          .push_back(std::move(device_info));

      device_info = mojom::blink::MediaDeviceInfo::New();
      device_info->device_id = kFakeVideoInputDeviceId2;
      device_info->label = "Fake Video Input 2";
      device_info->group_id = "";
      enumeration[static_cast<size_t>(MediaDeviceType::MEDIA_VIDEO_INPUT)]
          .push_back(std::move(device_info));
    }
    if (request_audio_output) {
      device_info = mojom::blink::MediaDeviceInfo::New();
      device_info->device_id = kFakeAudioOutputDeviceId1;
      device_info->label = "Fake Audio Input 1";
      device_info->group_id = "fake_group 1";
      enumeration[static_cast<size_t>(MediaDeviceType::MEDIA_AUDIO_OUTPUT)]
          .push_back(std::move(device_info));
    }
    std::move(callback).Run(std::move(enumeration));
  }

  void GetVideoInputCapabilities(GetVideoInputCapabilitiesCallback) override {
    NOTREACHED();
  }

  void GetAllVideoInputDeviceFormats(
      const String&,
      GetAllVideoInputDeviceFormatsCallback) override {
    NOTREACHED();
  }

  void GetAvailableVideoInputDeviceFormats(
      const String&,
      GetAvailableVideoInputDeviceFormatsCallback) override {
    NOTREACHED();
  }

  void GetAudioInputCapabilities(GetAudioInputCapabilitiesCallback) override {
    NOTREACHED();
  }

  MOCK_METHOD2(SubscribeDeviceChangeNotifications,
               void(MediaDeviceType type, uint32_t subscription_id));
  MOCK_METHOD2(UnsubscribeDeviceChangeNotifications,
               void(MediaDeviceType type, uint32_t subscription_id));

  mojom::blink::MediaDevicesDispatcherHostPtr CreateInterfacePtrAndBind() {
    mojom::blink::MediaDevicesDispatcherHostPtr ptr;
    binding_.Bind(mojo::MakeRequest(&ptr));
    return ptr;
  }

  void CloseBinding() { binding_.Close(); }

 private:
  mojo::Binding<mojom::blink::MediaDevicesDispatcherHost> binding_;
};

class PromiseObserver {
 public:
  PromiseObserver(ScriptState* script_state, ScriptPromise promise)
      : is_rejected_(false), is_fulfilled_(false) {
    v8::Local<v8::Function> on_fulfilled = MyScriptFunction::CreateFunction(
        script_state, &is_fulfilled_, &saved_arg_);
    v8::Local<v8::Function> on_rejected = MyScriptFunction::CreateFunction(
        script_state, &is_rejected_, &saved_arg_);
    promise.Then(on_fulfilled, on_rejected);
  }

  bool isDecided() { return is_rejected_ || is_fulfilled_; }

  bool isFulfilled() { return is_fulfilled_; }
  bool isRejected() { return is_rejected_; }
  ScriptValue argument() { return saved_arg_; }

 private:
  class MyScriptFunction : public ScriptFunction {
   public:
    static v8::Local<v8::Function> CreateFunction(ScriptState* script_state,
                                                  bool* flag_to_set,
                                                  ScriptValue* arg_to_set) {
      MyScriptFunction* self =
          new MyScriptFunction(script_state, flag_to_set, arg_to_set);
      return self->BindToV8Function();
    }

   private:
    MyScriptFunction(ScriptState* script_state,
                     bool* flag_to_set,
                     ScriptValue* arg_to_set)
        : ScriptFunction(script_state),
          flag_to_set_(flag_to_set),
          arg_to_set_(arg_to_set) {}
    ScriptValue Call(ScriptValue arg) {
      *flag_to_set_ = true;
      *arg_to_set_ = arg;
      return arg;
    }
    bool* flag_to_set_;
    ScriptValue* arg_to_set_;
  };

  bool is_rejected_;
  bool is_fulfilled_;
  ScriptValue saved_arg_;
};

class MediaDevicesTest : public ::testing::Test {
 public:
  using MediaDeviceInfos = PersistentHeapVector<Member<MediaDeviceInfo>>;

  MediaDevicesTest() = default;

  MediaDevices* GetMediaDevices(ExecutionContext* context) {
    if (!media_devices_) {
      media_devices_ = MediaDevices::Create(context);
      media_devices_->SetDispatcherHostForTesting(
          dispatcher_host_.CreateInterfacePtrAndBind());
    }
    return media_devices_;
  }

  void DevicesEnumerated(const MediaDeviceInfoVector& device_infos) {
    devices_enumerated_ = true;
    for (size_t i = 0; i < device_infos.size(); i++) {
      device_infos_.push_back(MediaDeviceInfo::Create(
          device_infos[i]->deviceId(), device_infos[i]->label(),
          device_infos[i]->groupId(), device_infos[i]->DeviceType()));
    }
  }

  void OnDispatcherHostConnectionError() { connection_error_ = true; }

  void CloseBinding() { dispatcher_host_.CloseBinding(); }

  const MediaDeviceInfos& device_infos() const { return device_infos_; }

  bool devices_enumerated() const { return devices_enumerated_; }

  bool connection_error() const { return connection_error_; }

  ScopedTestingPlatformSupport<TestingPlatformSupport>& platform() {
    return platform_;
  }

 private:
  ScopedTestingPlatformSupport<TestingPlatformSupport> platform_;
  MockMediaDevicesDispatcherHost dispatcher_host_;
  MediaDeviceInfos device_infos_;
  bool devices_enumerated_ = false;
  bool connection_error_ = false;
  Persistent<MediaDevices> media_devices_;
};

TEST_F(MediaDevicesTest, GetUserMediaCanBeCalled) {
  V8TestingScope scope;
  MediaStreamConstraints constraints;
  ScriptPromise promise =
      GetMediaDevices(scope.GetExecutionContext())
          ->getUserMedia(scope.GetScriptState(), constraints,
                         scope.GetExceptionState());
  ASSERT_FALSE(promise.IsEmpty());
  PromiseObserver promise_observer(scope.GetScriptState(), promise);
  EXPECT_FALSE(promise_observer.isDecided());
  v8::MicrotasksScope::PerformCheckpoint(scope.GetIsolate());
  EXPECT_TRUE(promise_observer.isDecided());
  // In the default test environment, we expect a DOM rejection because
  // the script state's execution context's document's frame doesn't
  // have an UserMediaController.
  EXPECT_TRUE(promise_observer.isRejected());
  // TODO(hta): Check that the correct error ("not supported") is returned.
  EXPECT_FALSE(promise_observer.argument().IsNull());
  // This log statement is included as a demonstration of how to get the string
  // value of the argument.
  VLOG(1) << "Argument is"
          << ToCoreString(promise_observer.argument()
                              .V8Value()
                              ->ToString(scope.GetContext())
                              .ToLocalChecked());
}

TEST_F(MediaDevicesTest, EnumerateDevices) {
  V8TestingScope scope;
  auto media_devices = GetMediaDevices(scope.GetExecutionContext());
  media_devices->SetEnumerateDevicesCallbackForTesting(
      WTF::Bind(&MediaDevicesTest::DevicesEnumerated, WTF::Unretained(this)));
  ScriptPromise promise =
      media_devices->enumerateDevices(scope.GetScriptState());
  platform()->RunUntilIdle();
  ASSERT_FALSE(promise.IsEmpty());

  EXPECT_TRUE(devices_enumerated());
  EXPECT_EQ(5u, device_infos().size());

  // Audio input device with matched output ID.
  Member<MediaDeviceInfo> device = device_infos()[0];
  EXPECT_FALSE(device->deviceId().IsEmpty());
  EXPECT_EQ("audioinput", device->kind());
  EXPECT_FALSE(device->label().IsEmpty());
  EXPECT_FALSE(device->groupId().IsEmpty());

  // Audio input device without matched output ID.
  device = device_infos()[1];
  EXPECT_FALSE(device->deviceId().IsEmpty());
  EXPECT_EQ("audioinput", device->kind());
  EXPECT_FALSE(device->label().IsEmpty());
  EXPECT_FALSE(device->groupId().IsEmpty());

  // Video input devices.
  device = device_infos()[2];
  EXPECT_FALSE(device->deviceId().IsEmpty());
  EXPECT_EQ("videoinput", device->kind());
  EXPECT_FALSE(device->label().IsEmpty());
  EXPECT_TRUE(device->groupId().IsEmpty());

  device = device_infos()[3];
  EXPECT_FALSE(device->deviceId().IsEmpty());
  EXPECT_EQ("videoinput", device->kind());
  EXPECT_FALSE(device->label().IsEmpty());
  EXPECT_TRUE(device->groupId().IsEmpty());

  // Audio output device.
  device = device_infos()[4];
  EXPECT_FALSE(device->deviceId().IsEmpty());
  EXPECT_EQ("audiooutput", device->kind());
  EXPECT_FALSE(device->label().IsEmpty());
  EXPECT_FALSE(device->groupId().IsEmpty());

  // Verfify group IDs.
  EXPECT_EQ(device_infos()[0]->groupId(), device_infos()[4]->groupId());
  EXPECT_NE(device_infos()[1]->groupId(), device_infos()[4]->groupId());
}

TEST_F(MediaDevicesTest, EnumerateDevicesAfterConnectionError) {
  V8TestingScope scope;
  auto media_devices = GetMediaDevices(scope.GetExecutionContext());
  media_devices->SetEnumerateDevicesCallbackForTesting(
      WTF::Bind(&MediaDevicesTest::DevicesEnumerated, WTF::Unretained(this)));
  media_devices->SetConnectionErrorCallbackForTesting(
      WTF::Bind(&MediaDevicesTest::OnDispatcherHostConnectionError,
                WTF::Unretained(this)));
  EXPECT_FALSE(connection_error());

  // Simulate a connection error by closing the binding.
  CloseBinding();
  platform()->RunUntilIdle();

  ScriptPromise promise =
      media_devices->enumerateDevices(scope.GetScriptState());
  platform()->RunUntilIdle();
  ASSERT_FALSE(promise.IsEmpty());
  EXPECT_TRUE(connection_error());
  EXPECT_FALSE(devices_enumerated());
}

TEST_F(MediaDevicesTest, EnumerateDevicesBeforeConnectionError) {
  V8TestingScope scope;
  auto media_devices = GetMediaDevices(scope.GetExecutionContext());
  media_devices->SetEnumerateDevicesCallbackForTesting(
      WTF::Bind(&MediaDevicesTest::DevicesEnumerated, WTF::Unretained(this)));
  media_devices->SetConnectionErrorCallbackForTesting(
      WTF::Bind(&MediaDevicesTest::OnDispatcherHostConnectionError,
                WTF::Unretained(this)));
  EXPECT_FALSE(connection_error());

  ScriptPromise promise =
      media_devices->enumerateDevices(scope.GetScriptState());
  platform()->RunUntilIdle();
  ASSERT_FALSE(promise.IsEmpty());

  // Simulate a connection error by closing the binding.
  CloseBinding();
  platform()->RunUntilIdle();

  EXPECT_TRUE(connection_error());
  EXPECT_TRUE(devices_enumerated());
}

}  // namespace blink
