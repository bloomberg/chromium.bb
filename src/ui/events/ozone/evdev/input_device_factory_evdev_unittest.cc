// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ui/events/ozone/evdev/input_device_factory_evdev.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/devices/input_device.h"
#include "ui/events/ozone/evdev/device_event_dispatcher_evdev.h"
#include "ui/events/ozone/evdev/event_converter_test_util.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/event_device_test_util.h"
#include "ui/events/ozone/evdev/input_device_opener.h"
#include "ui/events/ozone/features.h"

namespace ui {

class FakeEventConverterEvdev : public EventConverterEvdev {
 public:
  explicit FakeEventConverterEvdev(int fd,
                                   base::FilePath path,
                                   int id,
                                   InputDeviceType type,
                                   const std::string& name,
                                   const std::string& phys,
                                   uint16_t vendor_id,
                                   uint16_t product_id,
                                   uint16_t version,
                                   bool has_mouse,
                                   bool has_keyboard)
      : EventConverterEvdev(fd,
                            path,
                            id,
                            type,
                            name,
                            phys,
                            vendor_id,
                            product_id,
                            version),

        has_mouse_(has_mouse),
        has_keyboard_(has_keyboard) {}

  bool HasMouse() const override { return has_mouse_; }

  bool HasKeyboard() const override { return has_keyboard_; }

  void OnFileCanReadWithoutBlocking(int fd) override {}
  void SetKeyFilter(bool enable_filter,
                    std::vector<DomCode> allowed_keys) override {}

 private:
  bool has_mouse_;
  bool has_keyboard_;
};

class StubDeviceEventDispatcherEvdev : public DeviceEventDispatcherEvdev {
 public:
  explicit StubDeviceEventDispatcherEvdev(
      base::RepeatingCallback<void(const std::vector<InputDevice>& devices)>
          callback)
      : callback_(callback) {}
  ~StubDeviceEventDispatcherEvdev() override = default;

  void DispatchKeyEvent(const KeyEventParams& params) override {}
  void DispatchMouseMoveEvent(const MouseMoveEventParams& params) override {}
  void DispatchMouseButtonEvent(const MouseButtonEventParams& params) override {
  }
  void DispatchMouseWheelEvent(const MouseWheelEventParams& params) override {}
  void DispatchPinchEvent(const PinchEventParams& params) override {}
  void DispatchScrollEvent(const ScrollEventParams& params) override {}
  void DispatchTouchEvent(const TouchEventParams& params) override {}
  void DispatchMicrophoneMuteSwitchValueChanged(bool muted) override {}

  void DispatchKeyboardDevicesUpdated(
      const std::vector<InputDevice>& devices,
      base::flat_map<int, std::vector<uint64_t>> key_bits_mapping) override {
    callback_.Run(devices);
  }
  void DispatchTouchscreenDevicesUpdated(
      const std::vector<TouchscreenDevice>& devices) override {}
  void DispatchMouseDevicesUpdated(const std::vector<InputDevice>& devices,
                                   bool has_mouse,
                                   bool has_pointing_stick) override {}
  void DispatchTouchpadDevicesUpdated(const std::vector<InputDevice>& devices,
                                      bool has_haptic_touchpad) override {}
  void DispatchUncategorizedDevicesUpdated(
      const std::vector<InputDevice>& devices) override {}
  void DispatchDeviceListsComplete() override {}
  void DispatchStylusStateChanged(StylusState stylus_state) override {}

  void DispatchGamepadEvent(const GamepadEvent& event) override {}

  void DispatchGamepadDevicesUpdated(
      const std::vector<GamepadDevice>& devices,
      base::flat_map<int, std::vector<uint64_t>> key_bits_mapping) override {}

 private:
  base::RepeatingCallback<void(const std::vector<InputDevice>& devices)>
      callback_;
};

class FakeInputDeviceOpenerEvdev : public InputDeviceOpener {
 public:
  explicit FakeInputDeviceOpenerEvdev(
      std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters)
      : converters_(std::move(converters)) {}

  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters_;

  std::unique_ptr<EventConverterEvdev> OpenInputDevice(
      const OpenInputDeviceParams& params) override {
    std::unique_ptr<FakeEventConverterEvdev> converter =
        std::move(converters_[0]);
    converters_.erase(converters_.begin());
    return std::move(converter);
  }
};

class InputDeviceFactoryEvdevTest : public testing::Test {
 public:
  InputDeviceFactoryEvdevTest() = default;

  std::vector<InputDevice> keyboards_;
  base::test::SingleThreadTaskEnvironment task_environment{
      base::test::TaskEnvironment::MainThreadType::UI};
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<ui::DeviceEventDispatcherEvdev> dispatcher_;

  void SetUp() override {
    dispatcher_ = std::make_unique<StubDeviceEventDispatcherEvdev>(
        base::BindRepeating(&InputDeviceFactoryEvdevTest::DispatchCallback,
                            base::Unretained(this)));
  }

 private:
  void DispatchCallback(const std::vector<InputDevice>& devices) {
    keyboards_ = devices;
  }
};

TEST_F(InputDeviceFactoryEvdevTest,
       AttachSingularKeyboardFakeKeyboardHeuristicEnabled) {
  scoped_feature_list_.InitAndEnableFeature(kEnableFakeKeyboardHeuristic);
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;
  std::unique_ptr<FakeEventConverterEvdev> keyboard_converter =
      std::make_unique<FakeEventConverterEvdev>(
          1, base::FilePath("path"), 1, InputDeviceType::INPUT_DEVICE_INTERNAL,
          "name", "phys_path", 1, 1, 1, false, true);

  converters.push_back(std::move(keyboard_converter));

  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)));
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(keyboards_.size(), std::size_t(1));
  EXPECT_FALSE(keyboards_.front().suspected_imposter);
}

TEST_F(InputDeviceFactoryEvdevTest, AttachSingularMouse) {
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;
  std::unique_ptr<FakeEventConverterEvdev> mouse_converter =
      std::make_unique<FakeEventConverterEvdev>(
          1, base::FilePath("path"), 1, InputDeviceType::INPUT_DEVICE_INTERNAL,
          "name", "phys_path", 1, 1, 1, true, false);

  converters.push_back(std::move(mouse_converter));

  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)));
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(keyboards_.size(), std::size_t(0));
}

TEST_F(InputDeviceFactoryEvdevTest,
       AttachMouseAndKeyboardDifferentPhysFakeKeyboardHeuristicEnabled) {
  scoped_feature_list_.InitAndEnableFeature(kEnableFakeKeyboardHeuristic);
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;
  std::unique_ptr<FakeEventConverterEvdev> mouse_converter =
      std::make_unique<FakeEventConverterEvdev>(
          1, base::FilePath("mouse_path"), 1,
          InputDeviceType::INPUT_DEVICE_INTERNAL, "mouse_name",
          "phys_path/mouse", 1, 1, 1, true, false);
  std::unique_ptr<FakeEventConverterEvdev> keyboard_converter =
      std::make_unique<FakeEventConverterEvdev>(
          2, base::FilePath("keyboard_path"), 2,
          InputDeviceType::INPUT_DEVICE_INTERNAL, "keyboard_name",
          "phys_path/keyboard", 2, 2, 2, false, true);

  converters.push_back(std::move(mouse_converter));
  converters.push_back(std::move(keyboard_converter));

  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)));
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));
  input_device_factory_->AddInputDevice(2, base::FilePath("unused_value"));
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(keyboards_.size(), std::size_t(1));
  EXPECT_FALSE(keyboards_.front().suspected_imposter);
}

TEST_F(InputDeviceFactoryEvdevTest,
       AttachMouseAndKeyboardSamePhysFakeKeyboardHeuristicEnabled) {
  scoped_feature_list_.InitAndEnableFeature(kEnableFakeKeyboardHeuristic);
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;
  std::unique_ptr<FakeEventConverterEvdev> mouse_converter =
      std::make_unique<FakeEventConverterEvdev>(
          1, base::FilePath("mouse_path"), 1,
          InputDeviceType::INPUT_DEVICE_INTERNAL, "mouse_name", "phys_path", 1,
          1, 1, true, false);
  std::unique_ptr<FakeEventConverterEvdev> keyboard_converter =
      std::make_unique<FakeEventConverterEvdev>(
          2, base::FilePath("keyboard_path"), 2,
          InputDeviceType::INPUT_DEVICE_INTERNAL, "keyboard_name", "phys_path",
          2, 2, 2, false, true);

  converters.push_back(std::move(mouse_converter));
  converters.push_back(std::move(keyboard_converter));

  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)));
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));
  input_device_factory_->AddInputDevice(2, base::FilePath("unused_value"));
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(keyboards_.size(), std::size_t(1));
  EXPECT_TRUE(keyboards_.front().suspected_imposter);
}

TEST_F(InputDeviceFactoryEvdevTest,
       AttachSingularKeyboardAndMouseFakeKeyboardHeuristicEnabled) {
  scoped_feature_list_.InitAndEnableFeature(kEnableFakeKeyboardHeuristic);
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;
  std::unique_ptr<FakeEventConverterEvdev> keyboard_and_mouse_converter =
      std::make_unique<FakeEventConverterEvdev>(
          1, base::FilePath("path"), 1, InputDeviceType::INPUT_DEVICE_INTERNAL,
          "name", "phys_path", 1, 1, 1, true, true);

  converters.push_back(std::move(keyboard_and_mouse_converter));

  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)));
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(keyboards_.size(), std::size_t(1));
  EXPECT_TRUE(keyboards_.front().suspected_imposter);
}

TEST_F(InputDeviceFactoryEvdevTest,
       AttachSingularKeyboardFakeKeyboardHeuristicDisabled) {
  scoped_feature_list_.InitAndDisableFeature(kEnableFakeKeyboardHeuristic);
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;
  std::unique_ptr<FakeEventConverterEvdev> keyboard_converter =
      std::make_unique<FakeEventConverterEvdev>(
          1, base::FilePath("path"), 1, InputDeviceType::INPUT_DEVICE_INTERNAL,
          "name", "phys_path", 1, 1, 1, false, true);

  converters.push_back(std::move(keyboard_converter));

  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)));
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));

  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(keyboards_.size(), std::size_t(1));
  EXPECT_FALSE(keyboards_.front().suspected_imposter);
}

TEST_F(InputDeviceFactoryEvdevTest,
       AttachMouseAndKeyboardDifferentPhysFakeKeyboardHeuristicDisabled) {
  scoped_feature_list_.InitAndDisableFeature(kEnableFakeKeyboardHeuristic);
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;
  std::unique_ptr<FakeEventConverterEvdev> mouse_converter =
      std::make_unique<FakeEventConverterEvdev>(
          1, base::FilePath("mouse_path"), 1,
          InputDeviceType::INPUT_DEVICE_INTERNAL, "mouse_name",
          "phys_path/mouse", 1, 1, 1, true, false);
  std::unique_ptr<FakeEventConverterEvdev> keyboard_converter =
      std::make_unique<FakeEventConverterEvdev>(
          2, base::FilePath("keyboard_path"), 2,
          InputDeviceType::INPUT_DEVICE_INTERNAL, "keyboard_name",
          "phys_path/keyboard", 2, 2, 2, false, true);

  converters.push_back(std::move(mouse_converter));
  converters.push_back(std::move(keyboard_converter));

  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)));
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));
  input_device_factory_->AddInputDevice(2, base::FilePath("unused_value"));
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(keyboards_.size(), std::size_t(1));
  EXPECT_FALSE(keyboards_.front().suspected_imposter);
}

TEST_F(InputDeviceFactoryEvdevTest,
       AttachMouseAndKeyboardSamePhysFakeKeyboardHeuristicDisabled) {
  scoped_feature_list_.InitAndDisableFeature(kEnableFakeKeyboardHeuristic);
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;
  std::unique_ptr<FakeEventConverterEvdev> mouse_converter =
      std::make_unique<FakeEventConverterEvdev>(
          1, base::FilePath("mouse_path"), 1,
          InputDeviceType::INPUT_DEVICE_INTERNAL, "mouse_name", "phys_path", 1,
          1, 1, true, false);
  std::unique_ptr<FakeEventConverterEvdev> keyboard_converter =
      std::make_unique<FakeEventConverterEvdev>(
          2, base::FilePath("keyboard_path"), 2,
          InputDeviceType::INPUT_DEVICE_INTERNAL, "keyboard_name", "phys_path",
          2, 2, 2, false, true);

  converters.push_back(std::move(mouse_converter));
  converters.push_back(std::move(keyboard_converter));

  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)));
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));
  input_device_factory_->AddInputDevice(2, base::FilePath("unused_value"));
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(keyboards_.size(), std::size_t(1));
  EXPECT_FALSE(keyboards_.front().suspected_imposter);
}

TEST_F(InputDeviceFactoryEvdevTest,
       AttachSingularKeyboardAndMouseFakeKeyboardHeuristicDisabled) {
  scoped_feature_list_.InitAndDisableFeature(kEnableFakeKeyboardHeuristic);
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;
  std::unique_ptr<FakeEventConverterEvdev> keyboard_and_mouse_converter =
      std::make_unique<FakeEventConverterEvdev>(
          1, base::FilePath("path"), 1, InputDeviceType::INPUT_DEVICE_INTERNAL,
          "name", "phys_path", 1, 1, 1, true, true);

  converters.push_back(std::move(keyboard_and_mouse_converter));

  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)));
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(keyboards_.size(), std::size_t(1));
  EXPECT_FALSE(keyboards_.front().suspected_imposter);
}

TEST_F(InputDeviceFactoryEvdevTest,
       AttachAndRemoveDeviceFakeKeyboardHeuristicEnabled) {
  scoped_feature_list_.InitAndEnableFeature(kEnableFakeKeyboardHeuristic);
  std::vector<std::unique_ptr<FakeEventConverterEvdev>> converters;
  base::RunLoop run_loop;
  base::FilePath mouse_path("mouse_path");
  std::unique_ptr<FakeEventConverterEvdev> mouse_converter =
      std::make_unique<FakeEventConverterEvdev>(
          1, mouse_path, 1, InputDeviceType::INPUT_DEVICE_INTERNAL,
          "mouse_name", "phys_path", 1, 1, 1, true, false);
  std::unique_ptr<FakeEventConverterEvdev> keyboard_converter =
      std::make_unique<FakeEventConverterEvdev>(
          2, base::FilePath("keyboard_path"), 2,
          InputDeviceType::INPUT_DEVICE_INTERNAL, "keyboard_name", "phys_path",
          2, 2, 2, false, true);

  converters.push_back(std::move(mouse_converter));
  converters.push_back(std::move(keyboard_converter));

  std::unique_ptr<InputDeviceFactoryEvdev> input_device_factory_ =
      std::make_unique<InputDeviceFactoryEvdev>(
          std::move(dispatcher_), nullptr,
          std::make_unique<FakeInputDeviceOpenerEvdev>(std::move(converters)));
  input_device_factory_->OnStartupScanComplete();
  input_device_factory_->AddInputDevice(1, base::FilePath("unused_value"));
  input_device_factory_->AddInputDevice(2, base::FilePath("unused_value"));
  base::ThreadTaskRunnerHandle::Get()->PostTask(FROM_HERE,
                                                run_loop.QuitClosure());
  run_loop.Run();
  EXPECT_EQ(keyboards_.size(), std::size_t(1));
  EXPECT_TRUE(keyboards_.front().suspected_imposter);

  input_device_factory_->RemoveInputDevice(mouse_path);
  EXPECT_EQ(keyboards_.size(), std::size_t(1));
  EXPECT_FALSE(keyboards_.front().suspected_imposter);
}

}  // namespace ui
