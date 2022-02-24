// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/diagnostics_ui/backend/input_data_provider.h"

#include <iostream>
#include <map>
#include <vector>

#include "base/bind.h"
#include "base/command_line.h"
#include "base/containers/flat_map.h"
#include "base/message_loop/message_pump_for_ui.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/system/fake_statistics_provider.h"
#include "chromeos/system/statistics_provider.h"
#include "content/public/test/browser_task_environment.h"
#include "device/udev_linux/fake_udev_loader.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/chromeos/events/event_rewriter_chromeos.h"
#include "ui/events/keycodes/dom/dom_code.h"
#include "ui/events/keycodes/dom/dom_key.h"
#include "ui/events/ozone/device/device_event_observer.h"
#include "ui/events/ozone/device/device_manager.h"
#include "ui/events/ozone/evdev/event_device_test_util.h"
#include "ui/views/test/views_test_base.h"
#include "ui/views/widget/widget.h"

// Note: this is not a recommended pattern, but works and allows cleanly
// formatted invocations for this test set.
#define EXPECT_KEY_EVENTS(observerptr, id, ...)    \
  do {                                             \
    SCOPED_TRACE("EXPECT_KEY_EVENTS invocation");  \
    ExpectKeyEvents(observerptr, id, __VA_ARGS__); \
  } while (0);

namespace ash {
namespace diagnostics {

namespace {

constexpr mojom::TopRowKey kClassicTopRowKeys[] = {
    mojom::TopRowKey::kBack,
    mojom::TopRowKey::kForward,
    mojom::TopRowKey::kRefresh,
    mojom::TopRowKey::kFullscreen,
    mojom::TopRowKey::kOverview,
    mojom::TopRowKey::kScreenBrightnessDown,
    mojom::TopRowKey::kScreenBrightnessUp,
    mojom::TopRowKey::kVolumeMute,
    mojom::TopRowKey::kVolumeDown,
    mojom::TopRowKey::kVolumeUp};

const base::flat_map<uint32_t, ui::EventRewriterChromeOS::MutableKeyState>
    kInternalJinlonScanCodeMap = {
        {0xEA, {ui::EF_NONE, ui::DomCode::F1, ui::DomKey::F1, ui::VKEY_F1}},
        {0xE7, {ui::EF_NONE, ui::DomCode::F2, ui::DomKey::F2, ui::VKEY_F2}},
        {0x91, {ui::EF_NONE, ui::DomCode::F3, ui::DomKey::F3, ui::VKEY_F3}},
        {0x92, {ui::EF_NONE, ui::DomCode::F4, ui::DomKey::F4, ui::VKEY_F4}},
        {0x93, {ui::EF_NONE, ui::DomCode::F5, ui::DomKey::F5, ui::VKEY_F5}},
        {0x94, {ui::EF_NONE, ui::DomCode::F6, ui::DomKey::F6, ui::VKEY_F6}},
        {0x95, {ui::EF_NONE, ui::DomCode::F7, ui::DomKey::F7, ui::VKEY_F7}},
        {0x96, {ui::EF_NONE, ui::DomCode::F8, ui::DomKey::F8, ui::VKEY_F8}},
        {0x97, {ui::EF_NONE, ui::DomCode::F9, ui::DomKey::F9, ui::VKEY_F9}},
        {0x98, {ui::EF_NONE, ui::DomCode::F10, ui::DomKey::F10, ui::VKEY_F10}},
        {0xA0, {ui::EF_NONE, ui::DomCode::F11, ui::DomKey::F11, ui::VKEY_F11}},
        {0xAE, {ui::EF_NONE, ui::DomCode::F12, ui::DomKey::F12, ui::VKEY_F12}},
        {0xB0, {ui::EF_NONE, ui::DomCode::F13, ui::DomKey::F13, ui::VKEY_F13}},
};

constexpr mojom::TopRowKey kInternalJinlonTopRowKeys[] = {
    mojom::TopRowKey::kBack,
    mojom::TopRowKey::kRefresh,
    mojom::TopRowKey::kFullscreen,
    mojom::TopRowKey::kOverview,
    mojom::TopRowKey::kScreenshot,
    mojom::TopRowKey::kScreenBrightnessDown,
    mojom::TopRowKey::kScreenBrightnessUp,
    mojom::TopRowKey::kPrivacyScreenToggle,
    mojom::TopRowKey::kKeyboardBacklightDown,
    mojom::TopRowKey::kKeyboardBacklightUp,
    mojom::TopRowKey::kVolumeMute,
    mojom::TopRowKey::kVolumeDown,
    mojom::TopRowKey::kVolumeUp};

// One possible variant of a Dell configuration
constexpr mojom::TopRowKey kInternalDellTopRowKeys[] = {
    mojom::TopRowKey::kBack,
    mojom::TopRowKey::kRefresh,
    mojom::TopRowKey::kFullscreen,
    mojom::TopRowKey::kOverview,
    mojom::TopRowKey::kScreenBrightnessDown,
    mojom::TopRowKey::kScreenBrightnessUp,
    mojom::TopRowKey::kVolumeMute,
    mojom::TopRowKey::kVolumeDown,
    mojom::TopRowKey::kVolumeUp,
    mojom::TopRowKey::kNone,
    mojom::TopRowKey::kNone,
    mojom::TopRowKey::kScreenMirror,
    mojom::TopRowKey::kDelete};

constexpr char kKbdTopRowPropertyName[] = "CROS_KEYBOARD_TOP_ROW_LAYOUT";
constexpr char kKbdTopRowLayoutAttributeName[] = "function_row_physmap";

constexpr char kSillyDeviceName[] = "eventWithoutANumber";

constexpr char kInvalidMechnicalLayout[] = "Not ANSI, JIS, or ISO";

// Privacy Screen replaced with unknown 0xC4 scancode.
constexpr char kModifiedJinlonDescriptor[] =
    "EA E7 91 92 93 94 95 C4 97 98 A0 AE B0";
constexpr uint32_t kUnknownScancode = 0xC4;
constexpr int kUnknownScancodeIndex = 7;

struct KeyDefinition {
  uint32_t key_code;
  uint32_t at_scan_code;
  uint32_t usb_scan_code;
};

// TODO(b/211780758): we should acquire these tuples from dom_code_data.inc,
// where feasible.
constexpr KeyDefinition kKeyA = {KEY_A, 0x1E, 0x70004};
constexpr KeyDefinition kKeyB = {KEY_B, 0x30, 0x70005};
constexpr KeyDefinition kKeyEsc = {KEY_ESC, 0x30, 0x70005};
constexpr KeyDefinition kKeyF1 = {KEY_F1, 0x3B, 0x7003A};
constexpr KeyDefinition kKeyF8 = {KEY_F8, 0x42, 0x70041};
constexpr KeyDefinition kKeyF10 = {KEY_F10, 0x44, 0x70043};
// Drallion AT codes for F11-F12; not standardized
constexpr KeyDefinition kKeyF11 = {KEY_F11, 0x57, 0x700044};
constexpr KeyDefinition kKeyF12 = {KEY_F12, 0xD7, 0x700045};
constexpr KeyDefinition kKeyDelete = {KEY_DELETE, 0xD3, 0x7004C};
// Eve AT code; unknown if this is standard
constexpr KeyDefinition kKeyMenu = {KEY_CONTROLPANEL, 0x5D, 0};
// Jinlon AT code; unknown if this is standard
constexpr KeyDefinition kKeySleep = {KEY_SLEEP, 0x5D, 0};
constexpr KeyDefinition kKeyActionBack = {KEY_BACK, 0xEA, 0x0C0224};
constexpr KeyDefinition kKeyActionRefresh = {KEY_REFRESH, 0xE7, 0x0C0227};
constexpr KeyDefinition kKeyActionFullscreen = {KEY_ZOOM, 0x91, 0x0C0232};
constexpr KeyDefinition kKeyActionOverview = {KEY_SCALE, 0x92, 0x0C029F};
constexpr KeyDefinition kKeyActionScreenshot = {KEY_SYSRQ, 0x93, 0x070046};
constexpr KeyDefinition kKeyActionScreenBrightnessDown = {KEY_BRIGHTNESSDOWN,
                                                          0x94, 0x0C0070};
constexpr KeyDefinition kKeyActionScreenBrightnessUp = {KEY_BRIGHTNESSUP, 0x95,
                                                        0x0C006F};
constexpr KeyDefinition kKeyActionKeyboardBrightnessDown = {KEY_KBDILLUMDOWN,
                                                            0x97, 0x0C007A};
constexpr KeyDefinition kKeyActionKeyboardBrightnessUp = {KEY_KBDILLUMUP, 0x98,
                                                          0x0C0079};
constexpr KeyDefinition kKeyActionKeyboardVolumeMute = {KEY_MUTE, 0xA0,
                                                        0x0C00E2};
constexpr KeyDefinition kKeyActionKeyboardVolumeDown = {KEY_VOLUMEDOWN, 0xAE,
                                                        0x0C00EA};
constexpr KeyDefinition kKeyActionKeyboardVolumeUp = {KEY_VOLUMEUP, 0xB0,
                                                      0x0C00E9};
#if 0
// TODO(b/208729519): Not useful until we can test Drallion keyboards.
// Drallion, no HID equivalent
constexpr KeyDefinition kKeySwitchVideoMode = {KEY_SWITCHVIDEOMODE, 0x8B, 0};
constexpr KeyDefinition kKeyActionPrivacyScreenToggle =
   {KEY_PRIVACY_SCREEN_TOGGLE, 0x96, 0x0C02D0};
#endif

// NOTE: This is only creates a simple ui::InputDevice based on a device
// capabilities report; it is not suitable for subclasses of ui::InputDevice.
ui::InputDevice InputDeviceFromCapabilities(
    int device_id,
    const ui::DeviceCapabilities& capabilities) {
  ui::EventDeviceInfo device_info = {};
  ui::CapabilitiesToDeviceInfo(capabilities, &device_info);

  const std::string sys_path =
      base::StringPrintf("/dev/input/event%d-%s", device_id, capabilities.path);

  return ui::InputDevice(device_id, device_info.device_type(),
                         device_info.name(), device_info.phys(),
                         base::FilePath(sys_path), device_info.vendor_id(),
                         device_info.product_id(), device_info.version());
}

}  // namespace

namespace mojom {

std::ostream& operator<<(std::ostream& os, const KeyEvent& event) {
  os << "KeyEvent{ id=" << event.id << ", ";
  os << "type=" << event.type << ", ";
  os << "key_code=" << event.key_code << ", ";
  os << "scan_code=" << event.scan_code << ", ";
  os << "top_row_position=" << event.top_row_position;
  os << "}";
  return os;
}

}  // namespace mojom

// Fake device manager that lets us control the input devices that
// an InputDataProvider can see.
class FakeDeviceManager : public ui::DeviceManager {
 public:
  FakeDeviceManager() {}
  FakeDeviceManager(const FakeDeviceManager&) = delete;
  FakeDeviceManager& operator=(const FakeDeviceManager&) = delete;
  ~FakeDeviceManager() override {}

  // DeviceManager:
  void ScanDevices(ui::DeviceEventObserver* observer) override {}
  void AddObserver(ui::DeviceEventObserver* observer) override {}
  void RemoveObserver(ui::DeviceEventObserver* observer) override {}
};

class FakeInputDataEventWatcher;
typedef std::map<uint32_t, FakeInputDataEventWatcher*> watchers_t;

// Fake evdev watcher class that lets us manually post input
// events into an InputDataProvider; this keeps an external
// map of watchers updated so that instances can easily be found.
class FakeInputDataEventWatcher : public InputDataEventWatcher {
 public:
  FakeInputDataEventWatcher(
      uint32_t id,
      base::WeakPtr<InputDataEventWatcher::Dispatcher> dispatcher,
      watchers_t& watchers)
      : id_(id), dispatcher_(dispatcher), watchers_(watchers) {
    EXPECT_EQ(0u, watchers_.count(id_));
    watchers_[id_] = this;
  }
  ~FakeInputDataEventWatcher() override {
    EXPECT_EQ(watchers_[id_], this);
    watchers_.erase(id_);
  }

  void PostKeyEvent(bool down, uint32_t evdev_code, uint32_t scan_code) {
    if (dispatcher_)
      dispatcher_->SendInputKeyEvent(id_, evdev_code, scan_code, down);
  }

 private:
  uint32_t id_;
  base::WeakPtr<InputDataEventWatcher::Dispatcher> dispatcher_;
  watchers_t& watchers_;
};

// Utility to construct FakeInputDataEventWatcher for InputDataProvider.
class FakeInputDataEventWatcherFactory : public InputDataEventWatcher::Factory {
 public:
  FakeInputDataEventWatcherFactory(watchers_t& watchers)
      : watchers_(watchers) {}
  FakeInputDataEventWatcherFactory(const FakeInputDataEventWatcherFactory&) =
      delete;
  FakeInputDataEventWatcherFactory& operator=(
      const FakeInputDataEventWatcherFactory&) = delete;
  ~FakeInputDataEventWatcherFactory() override = default;

  std::unique_ptr<InputDataEventWatcher> MakeWatcher(
      uint32_t id,
      base::WeakPtr<InputDataEventWatcher::Dispatcher> dispatcher) override {
    return std::make_unique<FakeInputDataEventWatcher>(
        id, std::move(dispatcher), watchers_);
  }

 private:
  watchers_t& watchers_;
};

// A mock observer that records device change events emitted from an
// InputDataProvider.
class FakeConnectedDevicesObserver : public mojom::ConnectedDevicesObserver {
 public:
  // mojom::ConnectedDevicesObserver:
  void OnTouchDeviceConnected(
      mojom::TouchDeviceInfoPtr new_touch_device) override {
    touch_devices_connected.push_back(std::move(new_touch_device));
  }
  void OnTouchDeviceDisconnected(uint32_t id) override {
    touch_devices_disconnected.push_back(id);
  }
  void OnKeyboardConnected(mojom::KeyboardInfoPtr new_keyboard) override {
    keyboards_connected.push_back(std::move(new_keyboard));
  }
  void OnKeyboardDisconnected(uint32_t id) override {
    keyboards_disconnected.push_back(id);
  }

  std::vector<mojom::TouchDeviceInfoPtr> touch_devices_connected;
  std::vector<uint32_t> touch_devices_disconnected;
  std::vector<mojom::KeyboardInfoPtr> keyboards_connected;
  std::vector<uint32_t> keyboards_disconnected;

  mojo::Receiver<mojom::ConnectedDevicesObserver> receiver{this};
};

// A mock observer that records key event events emitted from an
// InputDataProvider.
class FakeKeyboardObserver : public mojom::KeyboardObserver {
 public:
  enum EventType {
    kEvent = 1,
    kPause = 2,
    kResume = 3,
  };

  // mojom::KeyboardObserver:
  void OnKeyEvent(mojom::KeyEventPtr key_event) override {
    events_.push_back({kEvent, std::move(key_event)});
  }
  void OnKeyEventsPaused() override { events_.push_back({kPause, nullptr}); }
  void OnKeyEventsResumed() override { events_.push_back({kResume, nullptr}); }

  std::vector<std::pair<EventType, mojom::KeyEventPtr>> events_;

  mojo::Receiver<mojom::KeyboardObserver> receiver{this};
};

// A utility class that fakes obtaining information about an evdev.
class FakeInputDeviceInfoHelper : public InputDeviceInfoHelper {
 public:
  FakeInputDeviceInfoHelper() {}

  ~FakeInputDeviceInfoHelper() override {}

  std::unique_ptr<InputDeviceInformation> GetDeviceInfo(
      int id,
      base::FilePath path) override {
    ui::DeviceCapabilities device_caps;
    const std::string base_name = path.BaseName().value();
    auto info = std::make_unique<InputDeviceInformation>();

    if (base_name == "event0") {
      device_caps = ui::kLinkKeyboard;
      info->keyboard_type =
          ui::EventRewriterChromeOS::DeviceType::kDeviceInternalKeyboard;
      info->keyboard_top_row_layout =
          ui::EventRewriterChromeOS::KeyboardTopRowLayout::kKbdTopRowLayout1;
      EXPECT_EQ(0, id);
    } else if (base_name == "event1") {
      device_caps = ui::kLinkTouchpad;
      EXPECT_EQ(1, id);
    } else if (base_name == "event2") {
      device_caps = ui::kKohakuTouchscreen;
      EXPECT_EQ(2, id);
    } else if (base_name == "event3") {
      device_caps = ui::kKohakuStylus;
      EXPECT_EQ(3, id);
    } else if (base_name == "event4") {
      device_caps = ui::kHpUsbKeyboard;
      info->keyboard_type =
          ui::EventRewriterChromeOS::DeviceType::kDeviceExternalGenericKeyboard;
      info->keyboard_top_row_layout = ui::EventRewriterChromeOS::
          KeyboardTopRowLayout::kKbdTopRowLayoutDefault;
      EXPECT_EQ(4, id);
    } else if (base_name == "event5") {
      device_caps = ui::kSarienKeyboard;  // Wilco
      info->keyboard_type =
          ui::EventRewriterChromeOS::DeviceType::kDeviceInternalKeyboard;
      info->keyboard_top_row_layout = ui::EventRewriterChromeOS::
          KeyboardTopRowLayout::kKbdTopRowLayoutWilco;
      EXPECT_EQ(5, id);
    } else if (base_name == "event6") {
      device_caps = ui::kEveKeyboard;
      info->keyboard_type =
          ui::EventRewriterChromeOS::DeviceType::kDeviceInternalKeyboard;
      info->keyboard_top_row_layout =
          ui::EventRewriterChromeOS::KeyboardTopRowLayout::kKbdTopRowLayout2;
      EXPECT_EQ(6, id);
    } else if (base_name == "event7") {
      device_caps = ui::kJinlonKeyboard;
      info->keyboard_type =
          ui::EventRewriterChromeOS::DeviceType::kDeviceInternalKeyboard;
      info->keyboard_top_row_layout = ui::EventRewriterChromeOS::
          KeyboardTopRowLayout::kKbdTopRowLayoutCustom;
      info->keyboard_scan_code_map = kInternalJinlonScanCodeMap;
      EXPECT_EQ(7, id);
    } else if (base_name == "event8") {
      device_caps = ui::kMicrosoftBluetoothNumberPad;
      info->keyboard_type =
          ui::EventRewriterChromeOS::DeviceType::kDeviceExternalGenericKeyboard;
      info->keyboard_top_row_layout = ui::EventRewriterChromeOS::
          KeyboardTopRowLayout::kKbdTopRowLayoutDefault;
      EXPECT_EQ(8, id);
    } else if (base_name == "event9") {
      device_caps = ui::kLogitechTouchKeyboardK400;
      info->keyboard_type =
          ui::EventRewriterChromeOS::DeviceType::kDeviceExternalGenericKeyboard;
      info->keyboard_top_row_layout = ui::EventRewriterChromeOS::
          KeyboardTopRowLayout::kKbdTopRowLayoutDefault;
      EXPECT_EQ(9, id);
    } else if (base_name == "event10") {
      device_caps = ui::kDrallionKeyboard;
      EXPECT_EQ(10, id);
    } else if (base_name == "event11") {
      // Used for customized top row layout.
      device_caps = ui::kJinlonKeyboard;
      device_caps.kbd_function_row_physmap = kModifiedJinlonDescriptor;
      info->keyboard_type =
          ui::EventRewriterChromeOS::DeviceType::kDeviceInternalKeyboard;
      info->keyboard_top_row_layout = ui::EventRewriterChromeOS::
          KeyboardTopRowLayout::kKbdTopRowLayoutCustom;
      info->keyboard_scan_code_map = kInternalJinlonScanCodeMap;
      info->keyboard_scan_code_map.erase(0x96);
      info->keyboard_scan_code_map[0xC4] = {ui::EF_NONE, ui::DomCode::F8,
                                            ui::DomKey::F8, ui::VKEY_F8};
      EXPECT_EQ(11, id);
    } else if (base_name == kSillyDeviceName) {
      // Simulate a device that is properly described, but has a malformed
      // device name.
      EXPECT_EQ(98, id);
      device_caps = ui::kLinkKeyboard;
    } else if (base_name == "event99") {
      EXPECT_EQ(99, id);
      // Simulate a device that couldn't be opened or have its info determined
      // for whatever reason.
      return nullptr;
    }

    EXPECT_TRUE(
        ui::CapabilitiesToDeviceInfo(device_caps, &info->event_device_info));
    info->evdev_id = id;
    info->path = path;
    info->input_device =
        InputDeviceFromCapabilities(info->evdev_id, device_caps);
    info->connection_type =
        InputDataProvider::ConnectionTypeFromInputDeviceType(
            info->event_device_info.device_type());

    return info;
  }
};

// Our modifications to InputDataProvider that carries around its own
// widget (representing the window that needs to be visible for key events
// to be observed), the needed factories for our fake utilities, and a
// reference to the current event watchers.
class TestInputDataProvider : public InputDataProvider {
 public:
  TestInputDataProvider(views::Widget* widget, watchers_t& watchers)
      : InputDataProvider(
            widget->GetNativeWindow(),
            std::make_unique<FakeDeviceManager>(),
            std::make_unique<FakeInputDataEventWatcherFactory>(watchers)),
        attached_widget_(widget),
        watchers_(watchers) {
    info_helper_ = base::SequenceBound<FakeInputDeviceInfoHelper>(
        base::ThreadPool::CreateSequencedTaskRunner({base::MayBlock()}));
  }
  explicit TestInputDataProvider(const TestInputDataProvider&) = delete;
  TestInputDataProvider& operator=(const TestInputDataProvider&) = delete;

  // The widget represents the tab that input diagnostics would normally be
  // shown in. This is allocated outside this class so it won't
  // be destroyed early. (See next item.)
  views::Widget* attached_widget_;
  // Keep a list of watchers for each evdev in the provider. This is a
  // reference to an instance outside of this class, as the lifetime of the list
  // needs to exceed the destruction of this test class, and can only be cleaned
  // up once all watchers have been destroyed by the base InputDataProvider,
  // which occurs after our destruction.
  watchers_t& watchers_;
};

class InputDataProviderTest : public views::ViewsTestBase {
 public:
  InputDataProviderTest()
      : views::ViewsTestBase(std::unique_ptr<base::test::TaskEnvironment>(
            std::make_unique<content::BrowserTaskEnvironment>(
                content::BrowserTaskEnvironment::MainThreadType::UI,
                content::BrowserTaskEnvironment::TimeSource::MOCK_TIME))) {}

  void SetUp() override {
    views::ViewsTestBase::SetUp();

    // Note: some init for creating widgets is performed in base SetUp
    // instead of the constructor, so our init must also be delayed until SetUp,
    // so we can safely invoke CreateTestWidget().

    statistics_provider_.SetMachineStatistic(
        chromeos::system::kKeyboardMechanicalLayoutKey, "ANSI");
    chromeos::system::StatisticsProvider::SetTestProvider(
        &statistics_provider_);

    fake_udev_ = std::make_unique<testing::FakeUdevLoader>();
    widget_ = CreateTestWidget();
    provider_ =
        std::make_unique<TestInputDataProvider>(widget_.get(), watchers_);

    // Apply these early, in SetUp; delaying until
    // FakeInputDeviceInfoHelper::GetDeviceInfo() is not appropriate, as
    // fake_udev is not thread safe. (If multiple devices are constructed in a
    // row, then GetDeviceInfo() invocation can overlap with
    // ProcessInputDataProvider::ProcessDeviceInfo() which reads from udev).
    UdevAddFakeDeviceCapabilities("/dev/input/event5", ui::kSarienKeyboard);
    UdevAddFakeDeviceCapabilities("/dev/input/event6", ui::kEveKeyboard);
    UdevAddFakeDeviceCapabilities("/dev/input/event7", ui::kJinlonKeyboard);
    UdevAddFakeDeviceCapabilities("/dev/input/event10", ui::kDrallionKeyboard);
    // Tweak top row keys for event11.
    auto device_caps = ui::kJinlonKeyboard;
    device_caps.kbd_function_row_physmap = kModifiedJinlonDescriptor;
    UdevAddFakeDeviceCapabilities("/dev/input/event11", device_caps);
  }

  InputDataProviderTest(const InputDataProviderTest&) = delete;
  InputDataProviderTest& operator=(const InputDataProviderTest&) = delete;
  ~InputDataProviderTest() override {
    provider_.reset();
    base::RunLoop().RunUntilIdle();
  }

 protected:
  struct ExpectedKeyEvent {
    KeyDefinition key;
    int position;
    bool down = true;
  };

  void ExpectKeyEvents(FakeKeyboardObserver* fake_observer,
                       uint32_t id,
                       std::initializer_list<ExpectedKeyEvent> list) {
    // Make sure the test does something...
    EXPECT_TRUE(std::size(list) > 0);

    size_t i;

    i = 0;
    for (auto* iter = list.begin(); iter != list.end(); iter++, i++) {
      provider_->watchers_[id]->PostKeyEvent(iter->down, iter->key.key_code,
                                             iter->key.at_scan_code);
    }
    base::RunLoop().RunUntilIdle();

    ASSERT_EQ(std::size(list), fake_observer->events_.size());

    i = 0;
    for (auto* iter = list.begin(); iter != list.end(); iter++, i++) {
      EXPECT_EQ(*fake_observer->events_[i].second,
                mojom::KeyEvent(/*id=*/id, /*type=*/iter->down
                                               ? mojom::KeyEventType::kPress
                                               : mojom::KeyEventType::kRelease,
                                /*key_code=*/iter->key.key_code,
                                /*scan_code=*/iter->key.at_scan_code,
                                /*top_row_position=*/iter->position))
          << " which is EXPECT_KEY_EVENTS item #" << i;
    }
  }

  void UdevAddFakeDeviceCapabilities(
      const std::string& device_name,
      const ui::DeviceCapabilities& device_caps) {
    std::map<std::string, std::string>
        sysfs_properties;  // Old style numeric tags.
    std::map<std::string, std::string>
        sysfs_attributes;  // New style vivaldi scancode layouts.

    if (device_caps.kbd_function_row_physmap &&
        strlen(device_caps.kbd_function_row_physmap) > 0) {
      sysfs_attributes[kKbdTopRowLayoutAttributeName] =
          device_caps.kbd_function_row_physmap;
    }

    if (device_caps.kbd_top_row_layout &&
        strlen(device_caps.kbd_top_row_layout) > 0) {
      sysfs_properties[kKbdTopRowPropertyName] = device_caps.kbd_top_row_layout;
    }

    // Each device needs a unique sys path; many of the ones embedded in
    // capabilities are the same, so uniquify them with the event device name.
    // These aren't actual valid paths, but nothing in the testing logic needs
    // them to be real.
    const std::string sys_path = device_name + "-" + device_caps.path;

    fake_udev_->AddFakeDevice(device_caps.name, sys_path.c_str(),
                              /*subsystem=*/"input", /*devnode=*/absl::nullopt,
                              /*devtype=*/absl::nullopt,
                              std::move(sysfs_attributes),
                              std::move(sysfs_properties));
  }

  std::unique_ptr<testing::FakeUdevLoader> fake_udev_;
  chromeos::system::FakeStatisticsProvider statistics_provider_;
  std::unique_ptr<views::Widget> widget_;
  // All evdev watchers in use by provider_.
  watchers_t watchers_;
  std::unique_ptr<TestInputDataProvider> provider_;
};

TEST_F(InputDataProviderTest, GetConnectedDevices_DeviceInfoMapping) {
  ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event0"));
  ui::DeviceEvent event1(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event1"));
  ui::DeviceEvent event2(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event2"));
  ui::DeviceEvent event3(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event3"));
  provider_->OnDeviceEvent(event0);
  provider_->OnDeviceEvent(event1);
  provider_->OnDeviceEvent(event2);
  provider_->OnDeviceEvent(event3);
  base::RunLoop().RunUntilIdle();

  base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                         std::vector<mojom::TouchDeviceInfoPtr>>
      future;
  provider_->GetConnectedDevices(future.GetCallback());

  const auto& keyboards = future.Get<0>();
  const auto& touch_devices = future.Get<1>();

  ASSERT_EQ(1ul, keyboards.size());
  // The stylus device should be filtered out, hence only 2 touch devices.
  ASSERT_EQ(2ul, touch_devices.size());

  const mojom::KeyboardInfoPtr& keyboard = keyboards[0];
  EXPECT_EQ(0u, keyboard->id);
  EXPECT_EQ(mojom::ConnectionType::kInternal, keyboard->connection_type);
  EXPECT_EQ("AT Translated Set 2 keyboard", keyboard->name);

  const mojom::TouchDeviceInfoPtr& touchpad = touch_devices[0];
  EXPECT_EQ(1u, touchpad->id);
  EXPECT_EQ(mojom::ConnectionType::kInternal, touchpad->connection_type);
  EXPECT_EQ(mojom::TouchDeviceType::kPointer, touchpad->type);
  EXPECT_EQ("Atmel maXTouch Touchpad", touchpad->name);

  const mojom::TouchDeviceInfoPtr& touchscreen = touch_devices[1];
  EXPECT_EQ(2u, touchscreen->id);
  EXPECT_EQ(mojom::ConnectionType::kInternal, touchscreen->connection_type);
  EXPECT_EQ(mojom::TouchDeviceType::kDirect, touchscreen->type);
  EXPECT_EQ("Atmel maXTouch Touchscreen", touchscreen->name);
}

TEST_F(InputDataProviderTest, GetConnectedDevices_AddEventAfterFirstCall) {
  {
    base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                           std::vector<mojom::TouchDeviceInfoPtr>>
        future;
    provider_->GetConnectedDevices(future.GetCallback());

    const auto& keyboards = future.Get<0>();
    const auto& touch_devices = future.Get<1>();
    ASSERT_EQ(0ul, keyboards.size());
    ASSERT_EQ(0ul, touch_devices.size());
  }

  ui::DeviceEvent event(ui::DeviceEvent::DeviceType::INPUT,
                        ui::DeviceEvent::ActionType::ADD,
                        base::FilePath("/dev/input/event4"));
  provider_->OnDeviceEvent(event);
  base::RunLoop().RunUntilIdle();

  {
    base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                           std::vector<mojom::TouchDeviceInfoPtr>>
        future;
    provider_->GetConnectedDevices(future.GetCallback());

    const auto& keyboards = future.Get<0>();
    const auto& touch_devices = future.Get<1>();

    ASSERT_EQ(1ul, keyboards.size());
    const mojom::KeyboardInfoPtr& keyboard = keyboards[0];
    EXPECT_EQ(4u, keyboard->id);
    EXPECT_EQ(mojom::ConnectionType::kUsb, keyboard->connection_type);
    EXPECT_EQ("Chicony HP Elite USB Keyboard", keyboard->name);

    EXPECT_EQ(0ul, touch_devices.size());
  }
}

TEST_F(InputDataProviderTest, GetConnectedDevices_AddUnusualDevices) {
  // Add two devices with unusual bus types, and verify connection types.

  ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event8"));
  ui::DeviceEvent event1(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event9"));
  provider_->OnDeviceEvent(event0);
  provider_->OnDeviceEvent(event1);
  base::RunLoop().RunUntilIdle();

  base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                         std::vector<mojom::TouchDeviceInfoPtr>>
      future;
  provider_->GetConnectedDevices(future.GetCallback());

  const auto& keyboards = future.Get<0>();
  const auto& touch_devices = future.Get<1>();

  ASSERT_EQ(2ul, keyboards.size());
  ASSERT_EQ(0ul, touch_devices.size());

  const mojom::KeyboardInfoPtr& keyboard1 = keyboards[0];
  EXPECT_EQ(8u, keyboard1->id);
  EXPECT_EQ(mojom::ConnectionType::kBluetooth, keyboard1->connection_type);
  EXPECT_EQ(ui::kMicrosoftBluetoothNumberPad.name, keyboard1->name);

  const mojom::KeyboardInfoPtr& keyboard2 = keyboards[1];
  EXPECT_EQ(9u, keyboard2->id);
  EXPECT_EQ(mojom::ConnectionType::kUnknown, keyboard2->connection_type);
  EXPECT_EQ(ui::kLogitechTouchKeyboardK400.name, keyboard2->name);
}

TEST_F(InputDataProviderTest, GetConnectedDevices_Remove) {
  ui::DeviceEvent add_touch_event(ui::DeviceEvent::DeviceType::INPUT,
                                  ui::DeviceEvent::ActionType::ADD,
                                  base::FilePath("/dev/input/event1"));
  provider_->OnDeviceEvent(add_touch_event);
  ui::DeviceEvent add_kbd_event(ui::DeviceEvent::DeviceType::INPUT,
                                ui::DeviceEvent::ActionType::ADD,
                                base::FilePath("/dev/input/event4"));
  provider_->OnDeviceEvent(add_kbd_event);
  base::RunLoop().RunUntilIdle();

  {
    base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                           std::vector<mojom::TouchDeviceInfoPtr>>
        future;
    provider_->GetConnectedDevices(future.GetCallback());

    const auto& keyboards = future.Get<0>();
    const auto& touch_devices = future.Get<1>();

    ASSERT_EQ(1ul, keyboards.size());
    EXPECT_EQ(4u, keyboards[0]->id);

    ASSERT_EQ(1ul, touch_devices.size());
    EXPECT_EQ(1u, touch_devices[0]->id);
  }

  ui::DeviceEvent remove_touch_event(ui::DeviceEvent::DeviceType::INPUT,
                                     ui::DeviceEvent::ActionType::REMOVE,
                                     base::FilePath("/dev/input/event1"));
  provider_->OnDeviceEvent(remove_touch_event);
  ui::DeviceEvent remove_kbd_event(ui::DeviceEvent::DeviceType::INPUT,
                                   ui::DeviceEvent::ActionType::REMOVE,
                                   base::FilePath("/dev/input/event4"));
  provider_->OnDeviceEvent(remove_kbd_event);
  base::RunLoop().RunUntilIdle();

  {
    base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                           std::vector<mojom::TouchDeviceInfoPtr>>
        future;
    provider_->GetConnectedDevices(future.GetCallback());

    const auto& keyboards = future.Get<0>();
    const auto& touch_devices = future.Get<1>();

    EXPECT_EQ(0ul, keyboards.size());
    EXPECT_EQ(0ul, touch_devices.size());
  }
}

TEST_F(InputDataProviderTest, KeyboardPhysicalLayoutDetection) {
  statistics_provider_.SetMachineStatistic(
      chromeos::system::kKeyboardMechanicalLayoutKey, "ISO");

  ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event0"));
  ui::DeviceEvent event1(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event4"));
  ui::DeviceEvent event2(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event5"));
  ui::DeviceEvent event3(ui::DeviceEvent::DeviceType::INPUT,
                         ui::DeviceEvent::ActionType::ADD,
                         base::FilePath("/dev/input/event7"));
  provider_->OnDeviceEvent(event0);
  provider_->OnDeviceEvent(event1);
  provider_->OnDeviceEvent(event2);
  provider_->OnDeviceEvent(event3);
  base::RunLoop().RunUntilIdle();

  base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                         std::vector<mojom::TouchDeviceInfoPtr>>
      future;
  provider_->GetConnectedDevices(future.GetCallback());

  const auto& keyboards = future.Get<0>();

  ASSERT_EQ(4ul, keyboards.size());

  const mojom::KeyboardInfoPtr& builtin_keyboard = keyboards[0];
  EXPECT_EQ(0u, builtin_keyboard->id);
  EXPECT_EQ(mojom::PhysicalLayout::kChromeOS,
            builtin_keyboard->physical_layout);
  EXPECT_EQ(mojom::MechanicalLayout::kIso, builtin_keyboard->mechanical_layout);
  EXPECT_EQ(mojom::NumberPadPresence::kNotPresent,
            builtin_keyboard->number_pad_present);
  EXPECT_EQ(
      std::vector(std::begin(kClassicTopRowKeys), std::end(kClassicTopRowKeys)),
      builtin_keyboard->top_row_keys);

  const mojom::KeyboardInfoPtr& external_keyboard = keyboards[1];
  EXPECT_EQ(4u, external_keyboard->id);
  EXPECT_EQ(mojom::PhysicalLayout::kUnknown,
            external_keyboard->physical_layout);
  EXPECT_EQ(mojom::MechanicalLayout::kUnknown,
            external_keyboard->mechanical_layout);
  EXPECT_EQ(mojom::NumberPadPresence::kUnknown,
            external_keyboard->number_pad_present);
  EXPECT_EQ(
      std::vector(std::begin(kClassicTopRowKeys), std::end(kClassicTopRowKeys)),
      external_keyboard->top_row_keys);

  const mojom::KeyboardInfoPtr& dell_internal_keyboard = keyboards[2];
  EXPECT_EQ(5u, dell_internal_keyboard->id);
  EXPECT_EQ(mojom::PhysicalLayout::kChromeOSDellEnterpriseWilco,
            dell_internal_keyboard->physical_layout);
  EXPECT_EQ(mojom::MechanicalLayout::kIso,
            dell_internal_keyboard->mechanical_layout);
  EXPECT_EQ(mojom::NumberPadPresence::kNotPresent,
            dell_internal_keyboard->number_pad_present);
  EXPECT_EQ(std::vector(std::begin(kInternalDellTopRowKeys),
                        std::end(kInternalDellTopRowKeys)),
            dell_internal_keyboard->top_row_keys);

  const mojom::KeyboardInfoPtr& jinlon_internal_keyboard = keyboards[3];
  EXPECT_EQ(7u, jinlon_internal_keyboard->id);
  EXPECT_EQ(mojom::PhysicalLayout::kChromeOS,
            jinlon_internal_keyboard->physical_layout);
  EXPECT_EQ(mojom::MechanicalLayout::kIso,
            jinlon_internal_keyboard->mechanical_layout);
  EXPECT_EQ(mojom::NumberPadPresence::kNotPresent,
            jinlon_internal_keyboard->number_pad_present);
  EXPECT_EQ(std::vector(std::begin(kInternalJinlonTopRowKeys),
                        std::end(kInternalJinlonTopRowKeys)),
            jinlon_internal_keyboard->top_row_keys);

  // TODO(b/208729519): We should check a Drallion keyboard, however that
  // invokes a check through the global Shell that does not operate in
  // this test.
}

TEST_F(InputDataProviderTest, KeyboardRegionDetection) {
  statistics_provider_.SetMachineStatistic(chromeos::system::kRegionKey, "jp");

  ui::DeviceEvent event_internal(ui::DeviceEvent::DeviceType::INPUT,
                                 ui::DeviceEvent::ActionType::ADD,
                                 base::FilePath("/dev/input/event0"));
  ui::DeviceEvent event_external(ui::DeviceEvent::DeviceType::INPUT,
                                 ui::DeviceEvent::ActionType::ADD,
                                 base::FilePath("/dev/input/event4"));
  provider_->OnDeviceEvent(event_internal);
  provider_->OnDeviceEvent(event_external);
  base::RunLoop().RunUntilIdle();

  base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                         std::vector<mojom::TouchDeviceInfoPtr>>
      future;
  provider_->GetConnectedDevices(future.GetCallback());

  const auto& keyboards = future.Get<0>();

  ASSERT_EQ(2ul, keyboards.size());

  const mojom::KeyboardInfoPtr& internal_keyboard = keyboards[0];
  EXPECT_EQ("jp", internal_keyboard->region_code);

  const mojom::KeyboardInfoPtr& external_keyboard = keyboards[1];
  EXPECT_EQ(absl::nullopt, external_keyboard->region_code);
}

TEST_F(InputDataProviderTest, KeyboardRegionDetection_Failure) {
  statistics_provider_.ClearMachineStatistic(chromeos::system::kRegionKey);

  ui::DeviceEvent event_internal(ui::DeviceEvent::DeviceType::INPUT,
                                 ui::DeviceEvent::ActionType::ADD,
                                 base::FilePath("/dev/input/event0"));
  provider_->OnDeviceEvent(event_internal);
  base::RunLoop().RunUntilIdle();

  base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                         std::vector<mojom::TouchDeviceInfoPtr>>
      future;
  provider_->GetConnectedDevices(future.GetCallback());

  const auto& keyboards = future.Get<0>();

  ASSERT_EQ(1ul, keyboards.size());

  const mojom::KeyboardInfoPtr& internal_keyboard = keyboards[0];
  EXPECT_EQ(absl::nullopt, internal_keyboard->region_code);
}

TEST_F(InputDataProviderTest, KeyboardAssistantKeyDetection) {
  ui::DeviceEvent link_event(ui::DeviceEvent::DeviceType::INPUT,
                             ui::DeviceEvent::ActionType::ADD,
                             base::FilePath("/dev/input/event0"));
  ui::DeviceEvent eve_event(ui::DeviceEvent::DeviceType::INPUT,
                            ui::DeviceEvent::ActionType::ADD,
                            base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(link_event);
  provider_->OnDeviceEvent(eve_event);
  base::RunLoop().RunUntilIdle();

  base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                         std::vector<mojom::TouchDeviceInfoPtr>>
      future;
  provider_->GetConnectedDevices(future.GetCallback());
  const auto& keyboards = future.Get<0>();

  ASSERT_EQ(2ul, keyboards.size());

  const mojom::KeyboardInfoPtr& link_keyboard = keyboards[0];
  EXPECT_EQ(0u, link_keyboard->id);
  EXPECT_FALSE(link_keyboard->has_assistant_key);
  const mojom::KeyboardInfoPtr& eve_keyboard = keyboards[1];
  EXPECT_EQ(6u, eve_keyboard->id);
  EXPECT_TRUE(eve_keyboard->has_assistant_key);
}

TEST_F(InputDataProviderTest, KeyboardNumberPadDetectionInternal) {
  // Detection of internal number pad depends on command-line
  // argument, and is not a property of the keyboard device.

  base::CommandLine::ForCurrentProcess()->InitFromArgv(
      {"", "--has-number-pad"});
  ui::DeviceEvent link_event(ui::DeviceEvent::DeviceType::INPUT,
                             ui::DeviceEvent::ActionType::ADD,
                             base::FilePath("/dev/input/event0"));
  provider_->OnDeviceEvent(link_event);
  base::RunLoop().RunUntilIdle();

  base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                         std::vector<mojom::TouchDeviceInfoPtr>>
      future;
  provider_->GetConnectedDevices(future.GetCallback());
  const auto& keyboards = future.Get<0>();

  ASSERT_EQ(1ul, keyboards.size());

  const mojom::KeyboardInfoPtr& builtin_keyboard = keyboards[0];
  EXPECT_EQ(0u, builtin_keyboard->id);
  EXPECT_EQ(mojom::NumberPadPresence::kPresent,
            builtin_keyboard->number_pad_present);
}

TEST_F(InputDataProviderTest, ObserveConnectedDevices_Keyboards) {
  FakeConnectedDevicesObserver fake_observer;
  provider_->ObserveConnectedDevices(
      fake_observer.receiver.BindNewPipeAndPassRemote());

  ui::DeviceEvent add_keyboard_event(ui::DeviceEvent::DeviceType::INPUT,
                                     ui::DeviceEvent::ActionType::ADD,
                                     base::FilePath("/dev/input/event4"));
  provider_->OnDeviceEvent(add_keyboard_event);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1ul, fake_observer.keyboards_connected.size());
  EXPECT_EQ(4u, fake_observer.keyboards_connected[0]->id);

  ui::DeviceEvent remove_keyboard_event(ui::DeviceEvent::DeviceType::INPUT,
                                        ui::DeviceEvent::ActionType::REMOVE,
                                        base::FilePath("/dev/input/event4"));
  provider_->OnDeviceEvent(remove_keyboard_event);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1ul, fake_observer.keyboards_disconnected.size());
  EXPECT_EQ(4u, fake_observer.keyboards_disconnected[0]);
}

TEST_F(InputDataProviderTest, ObserveConnectedDevices_TouchDevices) {
  FakeConnectedDevicesObserver fake_observer;
  provider_->ObserveConnectedDevices(
      fake_observer.receiver.BindNewPipeAndPassRemote());

  ui::DeviceEvent add_touch_event(ui::DeviceEvent::DeviceType::INPUT,
                                  ui::DeviceEvent::ActionType::ADD,
                                  base::FilePath("/dev/input/event1"));
  provider_->OnDeviceEvent(add_touch_event);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1ul, fake_observer.touch_devices_connected.size());
  EXPECT_EQ(1u, fake_observer.touch_devices_connected[0]->id);

  ui::DeviceEvent remove_touch_event(ui::DeviceEvent::DeviceType::INPUT,
                                     ui::DeviceEvent::ActionType::REMOVE,
                                     base::FilePath("/dev/input/event1"));
  provider_->OnDeviceEvent(remove_touch_event);
  base::RunLoop().RunUntilIdle();
  ASSERT_EQ(1ul, fake_observer.touch_devices_disconnected.size());
  EXPECT_EQ(1u, fake_observer.touch_devices_disconnected[0]);
}

TEST_F(InputDataProviderTest, ChangeDeviceDoesNotCrash) {
  ui::DeviceEvent add_device_event(ui::DeviceEvent::DeviceType::INPUT,
                                   ui::DeviceEvent::ActionType::ADD,
                                   base::FilePath("/dev/input/event1"));
  ui::DeviceEvent change_device_event(ui::DeviceEvent::DeviceType::INPUT,
                                      ui::DeviceEvent::ActionType::CHANGE,
                                      base::FilePath("/dev/input/event1"));
  provider_->OnDeviceEvent(add_device_event);
  base::RunLoop().RunUntilIdle();
  provider_->OnDeviceEvent(change_device_event);
  base::RunLoop().RunUntilIdle();
}

TEST_F(InputDataProviderTest, BadDeviceDoesNotCrash) {
  // Try a device that specifically fails to be processed.
  ui::DeviceEvent add_bad_device_event(ui::DeviceEvent::DeviceType::INPUT,
                                       ui::DeviceEvent::ActionType::ADD,
                                       base::FilePath("/dev/input/event99"));
  provider_->OnDeviceEvent(add_bad_device_event);
  base::RunLoop().RunUntilIdle();
}

TEST_F(InputDataProviderTest, SillyDeviceDoesNotCrash) {
  // Try a device that has data, but has a non-parseable name.
  ui::DeviceEvent add_silly_device_event(ui::DeviceEvent::DeviceType::INPUT,
                                         ui::DeviceEvent::ActionType::ADD,
                                         base::FilePath(kSillyDeviceName));
  provider_->OnDeviceEvent(add_silly_device_event);
  base::RunLoop().RunUntilIdle();
}

TEST_F(InputDataProviderTest, GetKeyboardMechanicalLayout_Unknown1) {
  statistics_provider_.ClearMachineStatistic(
      chromeos::system::kKeyboardMechanicalLayoutKey);

  ui::DeviceEvent add_keyboard_event(ui::DeviceEvent::DeviceType::INPUT,
                                     ui::DeviceEvent::ActionType::ADD,
                                     base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(add_keyboard_event);
  base::RunLoop().RunUntilIdle();

  {
    base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                           std::vector<mojom::TouchDeviceInfoPtr>>
        future;
    provider_->GetConnectedDevices(future.GetCallback());

    const auto& keyboards = future.Get<0>();

    ASSERT_EQ(1ul, keyboards.size());

    const mojom::KeyboardInfoPtr& builtin_keyboard = keyboards[0];
    EXPECT_EQ(6u, builtin_keyboard->id);
    EXPECT_EQ(mojom::PhysicalLayout::kChromeOS,
              builtin_keyboard->physical_layout);
    EXPECT_EQ(mojom::MechanicalLayout::kUnknown,
              builtin_keyboard->mechanical_layout);
    EXPECT_EQ(mojom::NumberPadPresence::kNotPresent,
              builtin_keyboard->number_pad_present);
  }
}

TEST_F(InputDataProviderTest, GetKeyboardMechanicalLayout_Unknown2) {
  statistics_provider_.SetMachineStatistic(
      chromeos::system::kKeyboardMechanicalLayoutKey, kInvalidMechnicalLayout);
  ui::DeviceEvent add_keyboard_event(ui::DeviceEvent::DeviceType::INPUT,
                                     ui::DeviceEvent::ActionType::ADD,
                                     base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(add_keyboard_event);
  base::RunLoop().RunUntilIdle();

  {
    base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                           std::vector<mojom::TouchDeviceInfoPtr>>
        future;
    provider_->GetConnectedDevices(future.GetCallback());

    const auto& keyboards = future.Get<0>();

    ASSERT_EQ(1ul, keyboards.size());

    const mojom::KeyboardInfoPtr& builtin_keyboard = keyboards[0];
    EXPECT_EQ(6u, builtin_keyboard->id);
    EXPECT_EQ(mojom::PhysicalLayout::kChromeOS,
              builtin_keyboard->physical_layout);
    EXPECT_EQ(mojom::MechanicalLayout::kUnknown,
              builtin_keyboard->mechanical_layout);
    EXPECT_EQ(mojom::NumberPadPresence::kNotPresent,
              builtin_keyboard->number_pad_present);
  }
}

TEST_F(InputDataProviderTest, ResetReceiverOnDisconnect) {
  ASSERT_FALSE(provider_->ReceiverIsBound());
  mojo::Remote<mojom::InputDataProvider> remote;
  provider_->BindInterface(remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(provider_->ReceiverIsBound());

  // Unbind remote to trigger disconnect and disconnect handler.
  remote.reset();
  base::RunLoop().RunUntilIdle();
  ASSERT_FALSE(provider_->ReceiverIsBound());

  // Test intent is to ensure interface can be rebound when application is
  // reloaded using |CTRL + R|.  A disconnect should be signaled in which we
  // will reset the receiver to its unbound state.
  provider_->BindInterface(remote.BindNewPipeAndPassReceiver());
  ASSERT_TRUE(provider_->ReceiverIsBound());
}

TEST_F(InputDataProviderTest, KeyObservationBasic) {
  std::unique_ptr<FakeKeyboardObserver> fake_observer =
      std::make_unique<FakeKeyboardObserver>();

  // Widget must be active and visible.
  provider_->attached_widget_->Show();
  provider_->attached_widget_->Activate();

  // Construct a keyboard.
  const ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                               ui::DeviceEvent::ActionType::ADD,
                               base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(event0);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, fake_observer->events_.size());
  EXPECT_EQ(0u, provider_->watchers_.size());

  // Attach a key observer.
  provider_->ObserveKeyEvents(
      6u, fake_observer->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Ensure an event watcher was constructed for the observer,
  // but has not posted any events.
  EXPECT_EQ(0u, fake_observer->events_.size());
  EXPECT_EQ(1u, provider_->watchers_.size());
  ASSERT_TRUE(provider_->watchers_[6]);

  // Post a key event through the watcher that
  // was created for the observer.
  provider_->watchers_[6]->PostKeyEvent(true, kKeyA.key_code,
                                        kKeyA.at_scan_code);
  base::RunLoop().RunUntilIdle();

  // Ensure the event came through.
  EXPECT_EQ(1u, fake_observer->events_.size());
  EXPECT_EQ(FakeKeyboardObserver::kEvent, fake_observer->events_[0].first);
  ASSERT_TRUE(fake_observer->events_[0].second);

  EXPECT_EQ(*fake_observer->events_[0].second,
            mojom::KeyEvent(/*id=*/6u, /*type=*/mojom::KeyEventType::kPress,
                            /*key_code=*/kKeyA.key_code,
                            /*scan_code=*/kKeyA.at_scan_code,
                            /*top_row_position=*/-1));
}

TEST_F(InputDataProviderTest, KeyObservationRemoval) {
  std::unique_ptr<FakeKeyboardObserver> fake_observer =
      std::make_unique<FakeKeyboardObserver>();

  // Widget must be active and visible.
  provider_->attached_widget_->Show();
  provider_->attached_widget_->Activate();

  // Construct a keyboard.
  const ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                               ui::DeviceEvent::ActionType::ADD,
                               base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(event0);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, fake_observer->events_.size());
  EXPECT_EQ(0u, provider_->watchers_.size());

  bool disconnected = false;

  // Attach a key observer.
  provider_->ObserveKeyEvents(
      6u, fake_observer->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  fake_observer->receiver.set_disconnect_handler(
      base::BindOnce([](bool* disconnected) { *disconnected = true; },
                     base::Unretained(&disconnected)));

  base::RunLoop().RunUntilIdle();

  // Ensure an event watcher was constructed for the observer,
  // but has not posted any events.
  EXPECT_EQ(0u, fake_observer->events_.size());
  EXPECT_EQ(1u, provider_->watchers_.size());
  EXPECT_FALSE(disconnected);
  ASSERT_TRUE(provider_->watchers_[6]);

  // Test a key event.
  EXPECT_KEY_EVENTS(fake_observer.get(), 6u, {{kKeyA, -1}});

  // Disconnect keyboard while it is being observed.
  ui::DeviceEvent remove_kbd_event(ui::DeviceEvent::DeviceType::INPUT,
                                   ui::DeviceEvent::ActionType::REMOVE,
                                   base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(remove_kbd_event);
  base::RunLoop().RunUntilIdle();

  // Watcher should have been shut down, and receiver disconnected.
  EXPECT_FALSE(provider_->watchers_[6]);
  EXPECT_TRUE(disconnected);
}

TEST_F(InputDataProviderTest, KeyObservationMultiple) {
  std::unique_ptr<FakeKeyboardObserver> fake_observer =
      std::make_unique<FakeKeyboardObserver>();

  // Widget must be active and visible.
  provider_->attached_widget_->Show();
  provider_->attached_widget_->Activate();

  // Construct a keyboard.
  const ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                               ui::DeviceEvent::ActionType::ADD,
                               base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(event0);
  base::RunLoop().RunUntilIdle();

  // Attach a key observer.
  provider_->ObserveKeyEvents(
      6u, fake_observer->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(provider_->watchers_[6]);

  EXPECT_KEY_EVENTS(fake_observer.get(), 6u,
                    {{kKeyA, -1, true},
                     {kKeyB, -1, true},
                     {kKeyA, -1, false},
                     {kKeyB, -1, false}});
}

TEST_F(InputDataProviderTest, KeyObservationObeysFocus) {
  std::unique_ptr<FakeKeyboardObserver> fake_observer =
      std::make_unique<FakeKeyboardObserver>();

  provider_->attached_widget_->Deactivate();
  provider_->attached_widget_->Hide();

  const ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                               ui::DeviceEvent::ActionType::ADD,
                               base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(event0);
  base::RunLoop().RunUntilIdle();

  provider_->ObserveKeyEvents(
      6u, fake_observer->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Verify we got the pause event from hiding the window.
  ASSERT_EQ(1u, fake_observer->events_.size());
  ASSERT_TRUE(provider_->watchers_[6]);

  EXPECT_EQ(FakeKeyboardObserver::kPause, fake_observer->events_[0].first);

  // Post a key event through the watcher that
  // was created for the observer.
  provider_->watchers_[6]->PostKeyEvent(true, kKeyA.key_code,
                                        kKeyA.at_scan_code);
  base::RunLoop().RunUntilIdle();

  // Ensure the event did not come through, as the widget was not visible and
  // focused.
  ASSERT_EQ(1u, fake_observer->events_.size());
  EXPECT_EQ(FakeKeyboardObserver::kPause, fake_observer->events_[0].first);
}

TEST_F(InputDataProviderTest, KeyObservationDisconnect) {
  std::unique_ptr<FakeKeyboardObserver> fake_observer =
      std::make_unique<FakeKeyboardObserver>();

  // Widget must be active and visible.
  provider_->attached_widget_->Show();
  provider_->attached_widget_->Activate();

  const ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                               ui::DeviceEvent::ActionType::ADD,
                               base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(event0);
  base::RunLoop().RunUntilIdle();

  provider_->ObserveKeyEvents(
      6u, fake_observer->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, fake_observer->events_.size());
  ASSERT_TRUE(provider_->watchers_[6]);

  fake_observer->receiver.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, fake_observer->events_.size());
  ASSERT_FALSE(provider_->watchers_[6]);
}

TEST_F(InputDataProviderTest, KeyObservationObeysFocusSwitching) {
  std::unique_ptr<FakeKeyboardObserver> fake_observer =
      std::make_unique<FakeKeyboardObserver>();
  std::unique_ptr<views::Widget> other_widget = CreateTestWidget();

  // Provider's widget must be active and visible.
  provider_->attached_widget_->Show();

  // Construct a keyboard.
  const ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                               ui::DeviceEvent::ActionType::ADD,
                               base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(event0);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, fake_observer->events_.size());
  EXPECT_EQ(0u, provider_->watchers_.size());

  // Attach a key observer.
  provider_->ObserveKeyEvents(
      6u, fake_observer->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  // Ensure an event watcher was constructed for the observer,
  // but has not posted any events.
  EXPECT_EQ(0u, fake_observer->events_.size());
  EXPECT_EQ(1u, provider_->watchers_.size());
  ASSERT_TRUE(provider_->watchers_[6]);

  // Focus on the other window.
  other_widget->Show();
  other_widget->Activate();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(provider_->attached_widget_->IsActive());
  EXPECT_TRUE(other_widget->IsVisible());
  EXPECT_TRUE(other_widget->IsActive());

  EXPECT_EQ(1u, fake_observer->events_.size());
  EXPECT_EQ(FakeKeyboardObserver::kPause, fake_observer->events_[0].first);
  ASSERT_FALSE(fake_observer->events_[0].second);

  // Post a key event through the watcher that
  // was created for the observer.
  provider_->watchers_[6]->PostKeyEvent(true, kKeyA.key_code,
                                        kKeyA.at_scan_code);
  base::RunLoop().RunUntilIdle();

  // Ensure the event did not come through.
  EXPECT_EQ(1u, fake_observer->events_.size());
  EXPECT_EQ(FakeKeyboardObserver::kPause, fake_observer->events_[0].first);

  // Clear events for next round.
  fake_observer->events_.clear();

  // Switch windows back.
  provider_->attached_widget_->Show();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(provider_->attached_widget_->IsActive());
  EXPECT_FALSE(other_widget->IsActive());

  // Post another key event.
  provider_->watchers_[6]->PostKeyEvent(true, kKeyB.key_code,
                                        kKeyB.at_scan_code);
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(2u, fake_observer->events_.size());
  EXPECT_EQ(FakeKeyboardObserver::kResume, fake_observer->events_[0].first);
  EXPECT_EQ(FakeKeyboardObserver::kEvent, fake_observer->events_[1].first);
  ASSERT_TRUE(fake_observer->events_[1].second);

  EXPECT_EQ(*fake_observer->events_[1].second,
            mojom::KeyEvent(/*id=*/6u, /*type=*/mojom::KeyEventType::kPress,
                            /*key_code=*/kKeyB.key_code,
                            /*scan_code=*/kKeyB.at_scan_code,
                            /*top_row_position=*/-1));
}

// Test overlapping lifetimes of separate observers of one device.
TEST_F(InputDataProviderTest, KeyObservationOverlappingeObserversOfDevice) {
  std::unique_ptr<FakeKeyboardObserver> fake_observer1 =
      std::make_unique<FakeKeyboardObserver>();

  provider_->attached_widget_->Show();

  const ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                               ui::DeviceEvent::ActionType::ADD,
                               base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(event0);
  base::RunLoop().RunUntilIdle();

  provider_->ObserveKeyEvents(
      6u, fake_observer1->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, fake_observer1->events_.size());
  EXPECT_EQ(1u, provider_->watchers_.size());
  EXPECT_TRUE(provider_->watchers_[6]);

  std::unique_ptr<FakeKeyboardObserver> fake_observer2 =
      std::make_unique<FakeKeyboardObserver>();

  provider_->ObserveKeyEvents(
      6u, fake_observer2->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, fake_observer1->events_.size());
  EXPECT_EQ(0u, fake_observer2->events_.size());
  EXPECT_TRUE(provider_->watchers_[6]);

  fake_observer1.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, fake_observer2->events_.size());
  ASSERT_TRUE(provider_->watchers_[6]);

  // And send an event through to check functionality.
  provider_->watchers_[6]->PostKeyEvent(true, kKeyA.key_code,
                                        kKeyA.at_scan_code);
  base::RunLoop().RunUntilIdle();

  // Ensure an event comes through properly after all that.
  EXPECT_EQ(1u, fake_observer2->events_.size());
  EXPECT_EQ(FakeKeyboardObserver::kEvent, fake_observer2->events_[0].first);
  ASSERT_TRUE(fake_observer2->events_[0].second);
  EXPECT_EQ(*fake_observer2->events_[0].second,
            mojom::KeyEvent(/*id=*/6u, /*type=*/mojom::KeyEventType::kPress,
                            /*key_code=*/kKeyA.key_code,
                            /*scan_code=*/kKeyA.at_scan_code,
                            /*top_row_position=*/-1));

  fake_observer2.reset();
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(0u, provider_->watchers_.count(6));
}

// Double-check security model and ensure that multiple instances
// do not interfere with each other, and that key observations obey
// individual window focus combined with multiple instances.
TEST_F(InputDataProviderTest, KeyObservationMultipleProviders) {
  // Create a second InputDataProvider, with a separate window/widget,
  // as would happen if multiple instances of the SWA were created.
  watchers_t provider2_watchers;
  auto provider2_widget = CreateTestWidget();

  std::unique_ptr<TestInputDataProvider> provider2_ =
      std::make_unique<TestInputDataProvider>(provider2_widget.get(),
                                              provider2_watchers);
  auto& provider1_ = provider_;

  std::unique_ptr<FakeKeyboardObserver> fake_observer1 =
      std::make_unique<FakeKeyboardObserver>();
  std::unique_ptr<FakeKeyboardObserver> fake_observer2 =
      std::make_unique<FakeKeyboardObserver>();

  // Show and activate first window.
  provider1_->attached_widget_->Show();
  // Show and activate second window; this will deactivate the first window.
  provider2_->attached_widget_->Show();

  EXPECT_FALSE(provider1_->attached_widget_->IsActive());
  EXPECT_TRUE(provider2_->attached_widget_->IsActive());

  // Construct a keyboard.
  const ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                               ui::DeviceEvent::ActionType::ADD,
                               base::FilePath("/dev/input/event6"));
  provider1_->OnDeviceEvent(event0);
  provider2_->OnDeviceEvent(event0);
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(provider1_->watchers_.empty());
  EXPECT_TRUE(provider2_->watchers_.empty());

  // Connected observer 1 to provider 1.
  provider1_->ObserveKeyEvents(
      6u, fake_observer1->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(provider1_->watchers_.empty());
  EXPECT_TRUE(provider2_->watchers_.empty());
  EXPECT_EQ(1u, fake_observer1->events_.size());
  EXPECT_EQ(FakeKeyboardObserver::kPause, fake_observer1->events_[0].first);
  EXPECT_EQ(1u, provider1_->watchers_.count(6));

  EXPECT_EQ(0u, fake_observer2->events_.size());
  EXPECT_EQ(0u, provider2_->watchers_.count(6));

  // Connected observer 2 to provider 2.

  provider2_->ObserveKeyEvents(
      6u, fake_observer2->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(provider1_->watchers_.empty());
  EXPECT_FALSE(provider2_->watchers_.empty());
  EXPECT_EQ(1u, fake_observer1->events_.size());
  EXPECT_EQ(FakeKeyboardObserver::kPause, fake_observer1->events_[0].first);
  EXPECT_EQ(1u, provider1_->watchers_.size());
  EXPECT_EQ(1u, provider1_->watchers_.size());
  ASSERT_TRUE(provider1_->watchers_[6]);
  ASSERT_TRUE(provider2_->watchers_[6]);

  EXPECT_EQ(0u, fake_observer2->events_.size());
  EXPECT_EQ(1u, provider2_->watchers_.size());
  EXPECT_TRUE(provider2_->watchers_[6]);
  // Providers should have distinct Watcher instances.
  EXPECT_NE(provider1_->watchers_[6], provider2_->watchers_[6]);

  // Reset event logs for next round.
  fake_observer1->events_.clear();
  fake_observer2->events_.clear();

  // Post two separate key events.
  provider1_->watchers_[6]->PostKeyEvent(true, kKeyA.key_code,
                                         kKeyA.at_scan_code);
  provider2_->watchers_[6]->PostKeyEvent(true, kKeyB.key_code,
                                         kKeyB.at_scan_code);
  base::RunLoop().RunUntilIdle();

  // Ensure the events came through to expected targets.
  EXPECT_EQ(0u, fake_observer1->events_.size());

  EXPECT_EQ(1u, fake_observer2->events_.size());
  EXPECT_EQ(FakeKeyboardObserver::kEvent, fake_observer2->events_[0].first);
  ASSERT_TRUE(fake_observer2->events_[0].second);
  EXPECT_EQ(*fake_observer2->events_[0].second,
            mojom::KeyEvent(/*id=*/6u, /*type=*/mojom::KeyEventType::kPress,
                            /*key_code=*/kKeyB.key_code,
                            /*scan_code=*/kKeyB.at_scan_code,
                            /*top_row_position=*/-1));

  // Reset event logs for next round.
  fake_observer1->events_.clear();
  fake_observer2->events_.clear();

  // Switch active window.
  provider1_->attached_widget_->Activate();
  base::RunLoop().RunUntilIdle();

  EXPECT_TRUE(provider1_->attached_widget_->IsActive());
  EXPECT_TRUE(provider1_->attached_widget_->IsVisible());
  EXPECT_FALSE(provider2_->attached_widget_->IsActive());

  provider1_->watchers_[6]->PostKeyEvent(true, kKeyA.key_code,
                                         kKeyA.at_scan_code);
  provider2_->watchers_[6]->PostKeyEvent(true, kKeyB.key_code,
                                         kKeyB.at_scan_code);
  base::RunLoop().RunUntilIdle();

  // Ensure the events came through to expected targets.

  EXPECT_EQ(2u, fake_observer1->events_.size());
  EXPECT_EQ(FakeKeyboardObserver::kResume, fake_observer1->events_[0].first);
  EXPECT_FALSE(fake_observer1->events_[0].second);
  EXPECT_EQ(FakeKeyboardObserver::kEvent, fake_observer1->events_[1].first);
  ASSERT_TRUE(fake_observer1->events_[1].second);
  EXPECT_EQ(*fake_observer1->events_[1].second,
            mojom::KeyEvent(/*id=*/6u, /*type=*/mojom::KeyEventType::kPress,
                            /*key_code=*/kKeyA.key_code,
                            /*scan_code=*/kKeyA.at_scan_code,
                            /*top_row_position=*/-1));

  EXPECT_EQ(1u, fake_observer2->events_.size());
  EXPECT_EQ(FakeKeyboardObserver::kPause, fake_observer2->events_[0].first);
  EXPECT_FALSE(fake_observer2->events_[0].second);

  // Reset event logs for next round.
  fake_observer1->events_.clear();
  fake_observer2->events_.clear();

  // Activate a new widget, ensuring neither previous window is active.
  auto widget3 = CreateTestWidget();
  widget3->Activate();
  base::RunLoop().RunUntilIdle();

  EXPECT_FALSE(provider1_->attached_widget_->IsActive());
  EXPECT_FALSE(provider2_->attached_widget_->IsActive());

  // Event should show key paused from previously active window.
  EXPECT_EQ(1u, fake_observer1->events_.size());
  EXPECT_EQ(FakeKeyboardObserver::kPause, fake_observer1->events_[0].first);
  EXPECT_FALSE(fake_observer1->events_[0].second);

  // Reset event logs for next round.
  fake_observer1->events_.clear();
  fake_observer2->events_.clear();

  // Deliver keys to both.
  provider1_->watchers_[6]->PostKeyEvent(true, kKeyA.key_code,
                                         kKeyA.at_scan_code);
  provider2_->watchers_[6]->PostKeyEvent(true, kKeyB.key_code,
                                         kKeyB.at_scan_code);
  base::RunLoop().RunUntilIdle();

  // Neither window is visible and active, no events should be received.
  EXPECT_FALSE(provider1_->attached_widget_->IsVisible() &&
               provider1_->attached_widget_->IsActive());
  EXPECT_FALSE(provider2_->attached_widget_->IsVisible() &&
               provider2_->attached_widget_->IsActive());
  EXPECT_EQ(0u, fake_observer1->events_.size());
  EXPECT_EQ(0u, fake_observer2->events_.size());
}

TEST_F(InputDataProviderTest, KeyObservationTopRowBasic) {
  // Test with Eve keyboard: [Escape, Back, ..., Louder, Menu]
  std::unique_ptr<FakeKeyboardObserver> fake_observer =
      std::make_unique<FakeKeyboardObserver>();

  // Widget must be active and visible.
  provider_->attached_widget_->Show();
  provider_->attached_widget_->Activate();

  // Construct a keyboard.
  const ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                               ui::DeviceEvent::ActionType::ADD,
                               base::FilePath("/dev/input/event6"));
  provider_->OnDeviceEvent(event0);
  base::RunLoop().RunUntilIdle();

  // Attach a key observer.
  provider_->ObserveKeyEvents(
      6u, fake_observer->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(provider_->watchers_[6]);

  EXPECT_KEY_EVENTS(fake_observer.get(), 6u,
                    {{kKeyEsc, -1},
                     {kKeyF1, 0},
                     {kKeyF10, 9},
                     {kKeyMenu, -1},
                     {kKeyDelete, -1}});
}

TEST_F(InputDataProviderTest, KeyObservationTopRowUnknownAction) {
  // Test for Vivaldi descriptor having an unrecognized scan-code;
  // most likely due to external keyboard being newer than OS image.

  std::unique_ptr<FakeKeyboardObserver> fake_observer =
      std::make_unique<FakeKeyboardObserver>();

  // Widget must be active and visible.
  provider_->attached_widget_->Show();
  provider_->attached_widget_->Activate();

  std::vector<mojom::TopRowKey> modified_top_row_keys =
      std::vector(std::begin(kInternalJinlonTopRowKeys),
                  std::end(kInternalJinlonTopRowKeys));
  modified_top_row_keys[kUnknownScancodeIndex] = mojom::TopRowKey::kUnknown;

  // Construct a keyboard.
  const ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                               ui::DeviceEvent::ActionType::ADD,
                               base::FilePath("/dev/input/event11"));
  provider_->OnDeviceEvent(event0);
  base::RunLoop().RunUntilIdle();

  base::test::TestFuture<std::vector<mojom::KeyboardInfoPtr>,
                         std::vector<mojom::TouchDeviceInfoPtr>>
      future;
  provider_->GetConnectedDevices(future.GetCallback());

  const auto& keyboards = future.Get<0>();

  ASSERT_EQ(1ul, keyboards.size());
  const mojom::KeyboardInfoPtr& keyboard = keyboards[0];
  EXPECT_EQ(11u, keyboard->id);
  EXPECT_EQ(modified_top_row_keys, keyboard->top_row_keys);

  // Attach a key observer.
  provider_->ObserveKeyEvents(
      11u, fake_observer->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(provider_->watchers_[11]);

  EXPECT_KEY_EVENTS(fake_observer.get(), 11u,
                    {{kKeyEsc, -1},
                     {kKeyActionBack, 0},
                     {kKeyF1, 0},
                     {kKeyActionRefresh, 1},
                     {kKeyActionFullscreen, 2},
                     {kKeyActionOverview, 3},
                     {kKeyActionScreenshot, 4},
                     {kKeyActionScreenBrightnessDown, 5},
                     {kKeyActionScreenBrightnessUp, 6},
                     {{0, kUnknownScancode, 0}, kUnknownScancodeIndex},
                     {kKeyF8, 7},
                     {kKeyActionKeyboardBrightnessDown, 8},
                     {kKeyActionKeyboardBrightnessUp, 9},
                     {kKeyActionKeyboardVolumeMute, 10},
                     {kKeyF10, 9},
                     {kKeyActionKeyboardVolumeDown, 11},
                     {kKeyActionKeyboardVolumeUp, 12},
                     {kKeySleep, -1}});
}

// TODO(b/208729519): Not available until we can test Drallion keyboards.
#if 0
TEST_F(InputDataProviderTest, KeyObservationTopRowDrallion) {
  // Test with Drallion keyboard:
  //  [Escape, Back, ..., Louder, F10, F11, F12, Mirror, Delete]
  // ...

  // Construct a keyboard
  const ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                               ui::DeviceEvent::ActionType::ADD,
                               base::FilePath("/dev/input/event10"));
  // ...
  struct {
    KeyDefinition key;
    int position;
  } keys[] = {
      {kKeyA, -1},
      {kKeyB, -1},
      {kKeyEsc, -1},
      {kKeyF1, 0},
      {kKeyF10, 9},
      {kKeyF11, 10},
      {kKeyF12, 11},
      {kKeySwitchVideoMode, 12},
      {kKeyDelete, 13},
  };

  for (size_t i = 0; i < std::size(keys); i++) {
    auto item = keys[i];
    provider_->watchers_[10]->PostKeyEvent(true, item.key.key_code,
         item.key.at_scan_code);
  }
  base::RunLoop().RunUntilIdle();

  // ...
}
#endif  // 0

TEST_F(InputDataProviderTest, KeyObservationTopRowExternalUSB) {
  std::unique_ptr<FakeKeyboardObserver> fake_observer =
      std::make_unique<FakeKeyboardObserver>();

  // Widget must be active and visible.
  provider_->attached_widget_->Show();
  provider_->attached_widget_->Activate();

  // Construct a keyboard.
  const ui::DeviceEvent event0(ui::DeviceEvent::DeviceType::INPUT,
                               ui::DeviceEvent::ActionType::ADD,
                               base::FilePath("/dev/input/event9"));
  provider_->OnDeviceEvent(event0);
  base::RunLoop().RunUntilIdle();

  // Attach a key observer.
  provider_->ObserveKeyEvents(
      9u, fake_observer->receiver.BindNewPipeAndPassRemote());
  base::RunLoop().RunUntilIdle();

  ASSERT_TRUE(provider_->watchers_[9]);

  // Test with generic external keyboard.
  EXPECT_KEY_EVENTS(fake_observer.get(), 9u,
                    {{kKeyA, -1},
                     {kKeyB, -1},
                     {kKeyMenu, -1},
                     {kKeyDelete, -1},
                     {kKeyEsc, -1},
                     {kKeyF1, 0},
                     {kKeyF10, 9},
                     {kKeyF11, 10},
                     {kKeyF12, 11}});
}

// TODO(b/211780758): Test all Fx scancodes using
// ui/events/keycodes/dom/dom_code_data.inc as source of truth.

}  // namespace diagnostics
}  // namespace ash
