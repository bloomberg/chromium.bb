// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/format_macros.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/events/ozone/evdev/event_device_info.h"
#include "ui/events/ozone/evdev/event_device_util.h"

namespace ui {

namespace {

struct DeviceCapabilities {
  // Full sysfs path (readlink -f /sys/class/input/event*)
  const char* path;

  // EVIOCGNAME (/sys/class/input/*/device/name)
  const char* name;

  // EVIOCGPHYS (/sys/class/input/*/device/phys)
  const char* phys;

  // EVIOCGUNIQ (/sys/class/input/*/device/uniq)
  const char* uniq;

  // EVIOCGID (/sys/class/input/*/device/id)
  const char* bustype;
  const char* vendor;
  const char* product;
  const char* version;

  // EVIOCGPROP (/sys/class/input/*/device/properties)
  // 64-bit groups.
  const char* prop;

  // EVIOCGBIT (/sys/class/input/*/device/capabilities)
  // 64-bit groups.
  const char* ev;
  const char* key;
  const char* rel;
  const char* abs;
  const char* msc;
  const char* sw;
  const char* led;
  const char* ff;
};

// # Script to generate DeviceCapabilities literal.
// cd /sys/class/input/input?
//
// test "$(uname -m)" = x86_64 && cat <<EOF
// static const DeviceCapabilities device = {
//     /* path */ "$(readlink -f .)",
//     /* name */ "$(cat device/name)",
//     /* phys */ "$(cat device/phys)",
//     /* uniq */ "$(cat device/uniq)",
//     /* bustype */ "$(cat device/id/bustype)",
//     /* vendor */ "$(cat device/id/vendor)",
//     /* product */ "$(cat device/id/product)",
//     /* version */ "$(cat device/id/version)",
//     /* prop */ "$(cat device/properties)",
//     /* ev */ "$(cat device/capabilities/ev)",
//     /* key */ "$(cat device/capabilities/key)",
//     /* rel */ "$(cat device/capabilities/rel)",
//     /* abs */ "$(cat device/capabilities/abs)",
//     /* msc */ "$(cat device/capabilities/msc)",
//     /* led */ "$(cat device/capabilities/led)",
//     /* ff */ "$(cat device/capabilities/ff)",
// };
// EOF

// This test requres 64 bit groups in bitmask inputs (merge them if 32-bit).
const int kTestDataWordSize = 64;

// Captured from Chromebook Pixel.
const DeviceCapabilities kLinkKeyboard = {
    /* path */ "/sys/devices/platform/i8042/serio0/input/input6/event6",
    /* name */ "AT Translated Set 2 keyboard",
    /* phys */ "isa0060/serio0/input0",
    /* uniq */ "",
    /* bustype */ "0011",
    /* vendor */ "0001",
    /* product */ "0001",
    /* version */ "ab83",
    /* prop */ "0",
    /* ev */ "120013",
    /* key */ "400402000000 3803078f800d001 feffffdfffefffff fffffffffffffffe",
    /* rel */ "0",
    /* abs */ "0",
    /* msc */ "10",
    /* led */ "7",
    /* ff */ "0",
};

// Captured from Chromebook Pixel.
const DeviceCapabilities kLinkTouchscreen = {
    /* path */ "/sys/devices/pci0000:00/0000:00:02.0/i2c-2/2-004a/"
               "input/input7/event7",
    /* name */ "Atmel maXTouch Touchscreen",
    /* phys */ "i2c-2-004a/input0",
    /* uniq */ "",
    /* bustype */ "0018",
    /* vendor */ "0000",
    /* product */ "0000",
    /* version */ "0000",
    /* prop */ "0",
    /* ev */ "b",
    /* key */ "400 0 0 0 0 0",
    /* rel */ "0",
    /* abs */ "671800001000003",
    /* msc */ "0",
    /* led */ "0",
    /* ff */ "0",
};

// Captured from Chromebook Pixel.
const DeviceCapabilities kLinkTouchpad = {
    /* path */ "/sys/devices/pci0000:00/0000:00:02.0/i2c-1/1-004b/"
               "input/input8/event8",
    /* name */ "Atmel maXTouch Touchpad",
    /* phys */ "i2c-1-004b/input0",
    /* uniq */ "",
    /* bustype */ "0018",
    /* vendor */ "0000",
    /* product */ "0000",
    /* version */ "0000",
    /* prop */ "5",
    /* ev */ "b",
    /* key */ "e520 10000 0 0 0 0",
    /* rel */ "0",
    /* abs */ "671800001000003",
    /* msc */ "0",
    /* led */ "0",
    /* ff */ "0",
};

// Captured from generic HP KU-1156 USB keyboard.
static const DeviceCapabilities kHpUsbKeyboard = {
    /* path */ "/sys/devices/pci0000:00/0000:00:1d.0/usb2/2-1/2-1.3/2-1.3:1.0/"
               "input/input17/event10",
    /* name */ "Chicony HP Elite USB Keyboard",
    /* phys */ "usb-0000:00:1d.0-1.3/input0",
    /* uniq */ "",
    /* bustype */ "0003",
    /* vendor */ "03f0",
    /* product */ "034a",
    /* version */ "0110",
    /* prop */ "0",
    /* ev */ "120013",
    /* key */ "1000000000007 ff9f207ac14057ff febeffdfffefffff "
              "fffffffffffffffe",
    /* rel */ "0",
    /* abs */ "0",
    /* msc */ "10",
    /* led */ "7",
    /* ff */ "0",
};

// Captured from generic HP KU-1156 USB keyboard (2nd device with media keys).
static const DeviceCapabilities kHpUsbKeyboard_Extra = {
    /* path */ "/sys/devices/pci0000:00/0000:00:1d.0/usb2/2-1/2-1.3/2-1.3:1.1/"
               "input/input18/event16",
    /* name */ "Chicony HP Elite USB Keyboard",
    /* phys */ "usb-0000:00:1d.0-1.3/input1",
    /* uniq */ "",
    /* bustype */ "0003",
    /* vendor */ "03f0",
    /* product */ "034a",
    /* version */ "0110",
    /* prop */ "0",
    /* ev */ "1f",
    /* key */ "3007f 0 0 483ffff17aff32d bf54444600000000 1 120f938b17c000 "
              "677bfad941dfed 9ed68000004400 10000002",
    /* rel */ "40",
    /* abs */ "100000000",
    /* msc */ "10",
    /* led */ "0",
    /* ff */ "0",
};

// Captured from Dell MS111-L 3-Button Optical USB Mouse.
static const DeviceCapabilities kLogitechUsbMouse = {
    /* path */ "/sys/devices/pci0000:00/0000:00:1d.0/usb2/2-1/2-1.2/2-1.2.4/"
               "2-1.2.4:1.0/input/input16/event9",
    /* name */ "Logitech USB Optical Mouse",
    /* phys */ "usb-0000:00:1d.0-1.2.4/input0",
    /* uniq */ "",
    /* bustype */ "0003",
    /* vendor */ "046d",
    /* product */ "c05a",
    /* version */ "0111",
    /* prop */ "0",
    /* ev */ "17",
    /* key */ "ff0000 0 0 0 0",
    /* rel */ "143",
    /* abs */ "0",
    /* msc */ "10",
    /* led */ "0",
    /* ff */ "0",
};

// Captured from "Mimo Touch 2" Universal DisplayLink monitor.
static const DeviceCapabilities kMimoTouch2Touchscreen = {
    /* path */ "/sys/devices/pci0000:00/0000:00:1d.0/usb2/2-1/2-1.3/2-1.3.2/"
               "2-1.3.2:1.0/input/input15/event14",
    /* name */ "eGalax Inc. USB TouchController",
    /* phys */ "usb-0000:00:1d.0-1.3.2/input0",
    /* uniq */ "",
    /* bustype */ "0003",
    /* vendor */ "0eef",
    /* product */ "0001",
    /* version */ "0100",
    /* prop */ "0",
    /* ev */ "b",
    /* key */ "400 0 0 0 0 0",
    /* rel */ "0",
    /* abs */ "3",
    /* msc */ "0",
    /* led */ "0",
    /* ff */ "0",
};

// Captured from Wacom Intuos Pen and Touch Small Tablet.
static const DeviceCapabilities kWacomIntuosPtS_Pen = {
    /* path */ "/sys/devices/pci0000:00/0000:00:1d.0/usb2/2-1/2-1.2/2-1.2.3/"
               "2-1.2.3:1.0/input/input9/event9",
    /* name */ "Wacom Intuos PT S Pen",
    /* phys */ "",
    /* uniq */ "",
    /* bustype */ "0003",
    /* vendor */ "056a",
    /* product */ "0302",
    /* version */ "0100",
    /* prop */ "1",
    /* ev */ "b",
    /* key */ "1c03 0 0 0 0 0",
    /* rel */ "0",
    /* abs */ "3000003",
    /* msc */ "0",
    /* led */ "0",
    /* ff */ "0",
};

// Captured from Wacom Intuos Pen and Touch Small Tablet.
static const DeviceCapabilities kWacomIntuosPtS_Finger = {
    /* path */ "/sys/devices/pci0000:00/0000:00:1d.0/usb2/2-1/2-1.2/2-1.2.3/"
               "2-1.2.3:1.1/input/input10/event10",
    /* name */ "Wacom Intuos PT S Finger",
    /* phys */ "",
    /* uniq */ "",
    /* bustype */ "0003",
    /* vendor */ "056a",
    /* product */ "0302",
    /* version */ "0100",
    /* prop */ "1",
    /* ev */ "2b",
    /* key */ "e520 630000 0 0 0 0",
    /* rel */ "0",
    /* abs */ "263800000000003",
    /* msc */ "0",
    /* led */ "0",
    /* ff */ "0",
};

// Captured from Logitech Wireless Touch Keyboard K400.
static const DeviceCapabilities kLogitechTouchKeyboardK400 = {
    /* path */ "/sys/devices/pci0000:00/0000:00:1d.0/usb2/2-1/2-1.2/2-1.2.3/"
               "2-1.2.3:1.2/0003:046D:C52B.0006/input/input19/event17",
    /* name */ "Logitech Unifying Device. Wireless PID:4024",
    /* phys */ "usb-0000:00:1d.0-1.2.3:1",
    /* uniq */ "",
    /* bustype */ "001d",
    /* vendor */ "046d",
    /* product */ "4024",
    /* version */ "0111",
    /* prop */ "0",
    /* ev */ "12001f",
    /* key */ "3007f 0 0 483ffff17aff32d bf54444600000000 ffff0001 "
              "130f938b17c007 ffff7bfad9415fff febeffdfffefffff "
              "fffffffffffffffe",
    /* rel */ "1c3",
    /* abs */ "100000000",
    /* msc */ "10",
    /* led */ "1f",
    /* ff */ "0",
};

#define EVDEV_BITS_TO_GROUPS(x) \
  (((x) + kTestDataWordSize - 1) / kTestDataWordSize)

std::string SerializeBitfield(unsigned long* bitmap, int max) {
  std::string ret;

  for (int i = EVDEV_BITS_TO_GROUPS(max) - 1; i >= 0; i--) {
    if (bitmap[i] || ret.size()) {
      base::StringAppendF(&ret, "%" PRIx64, bitmap[i]);

      if (i > 0)
        ret += " ";
    }
  }

  if (ret.length() == 0)
    ret = "0";

  return ret;
}

bool ParseBitfield(const std::string& bitfield,
                   size_t max_bits,
                   std::vector<unsigned long>* out) {
  std::vector<std::string> groups;
  base::SplitString(bitfield, ' ', &groups);

  out->resize(EVDEV_BITS_TO_LONGS(max_bits));

  // Convert big endian 64-bit groups to little endian EVDEV_LONG_BIT groups.
  for (unsigned int i = 0; i < groups.size(); ++i) {
    int off = groups.size() - 1 - i;

    uint64_t val;
    if (!base::HexStringToUInt64(groups[off], &val))
      return false;

    for (int j = 0; j < kTestDataWordSize; ++j) {
      unsigned int code = i * kTestDataWordSize + j;

      if (code >= max_bits)
        break;

      if (val & (1UL << j))
        EvdevSetBit(&(*out)[0], code);
    }
  }

  // Require canonically formatted input.
  EXPECT_EQ(bitfield, SerializeBitfield(out->data(), max_bits));

  return true;
}

bool CapabilitiesToDeviceInfo(const DeviceCapabilities& capabilities,
                              EventDeviceInfo* devinfo) {
  std::vector<unsigned long> ev_bits;
  if (!ParseBitfield(capabilities.ev, EV_CNT, &ev_bits))
    return false;
  devinfo->SetEventTypes(&ev_bits[0], ev_bits.size());

  std::vector<unsigned long> key_bits;
  if (!ParseBitfield(capabilities.key, KEY_CNT, &key_bits))
    return false;
  devinfo->SetKeyEvents(&key_bits[0], key_bits.size());

  std::vector<unsigned long> rel_bits;
  if (!ParseBitfield(capabilities.rel, REL_CNT, &rel_bits))
    return false;
  devinfo->SetRelEvents(&rel_bits[0], rel_bits.size());

  std::vector<unsigned long> abs_bits;
  if (!ParseBitfield(capabilities.abs, ABS_CNT, &abs_bits))
    return false;
  devinfo->SetAbsEvents(&abs_bits[0], abs_bits.size());

  std::vector<unsigned long> msc_bits;
  if (!ParseBitfield(capabilities.msc, MSC_CNT, &msc_bits))
    return false;
  devinfo->SetMscEvents(&msc_bits[0], msc_bits.size());

  std::vector<unsigned long> led_bits;
  if (!ParseBitfield(capabilities.led, LED_CNT, &led_bits))
    return false;
  devinfo->SetLedEvents(&led_bits[0], led_bits.size());

  std::vector<unsigned long> prop_bits;
  if (!ParseBitfield(capabilities.prop, INPUT_PROP_CNT, &prop_bits))
    return false;
  devinfo->SetProps(&prop_bits[0], prop_bits.size());

  return true;
}

}  // namespace

class EventDeviceInfoTest : public testing::Test {
 public:
  EventDeviceInfoTest();

 private:
  DISALLOW_COPY_AND_ASSIGN(EventDeviceInfoTest);
};

EventDeviceInfoTest::EventDeviceInfoTest() {
}

TEST_F(EventDeviceInfoTest, BasicCrosKeyboard) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kLinkKeyboard, &devinfo));

  EXPECT_TRUE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
}

TEST_F(EventDeviceInfoTest, BasicCrosTouchscreen) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kLinkTouchscreen, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_TRUE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
}

TEST_F(EventDeviceInfoTest, BasicCrosTouchpad) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kLinkTouchpad, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_TRUE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
}

TEST_F(EventDeviceInfoTest, BasicUsbKeyboard) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kHpUsbKeyboard, &devinfo));

  EXPECT_TRUE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
}

TEST_F(EventDeviceInfoTest, BasicUsbKeyboard_Extra) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kHpUsbKeyboard_Extra, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());  // Has keys, but not a full keyboard.
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
}

TEST_F(EventDeviceInfoTest, BasicUsbMouse) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kLogitechUsbMouse, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_TRUE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
}

TEST_F(EventDeviceInfoTest, BasicUsbTouchscreen) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kMimoTouch2Touchscreen, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_TRUE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
}

TEST_F(EventDeviceInfoTest, BasicUsbTablet) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kWacomIntuosPtS_Pen, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_TRUE(devinfo.HasTablet());
}

TEST_F(EventDeviceInfoTest, BasicUsbTouchpad) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kWacomIntuosPtS_Finger, &devinfo));

  EXPECT_FALSE(devinfo.HasKeyboard());
  EXPECT_FALSE(devinfo.HasMouse());
  EXPECT_TRUE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
}

TEST_F(EventDeviceInfoTest, HybridKeyboardWithMouse) {
  EventDeviceInfo devinfo;
  EXPECT_TRUE(CapabilitiesToDeviceInfo(kLogitechTouchKeyboardK400, &devinfo));

  // The touchpad actually exposes mouse (relative) Events.
  EXPECT_TRUE(devinfo.HasKeyboard());
  EXPECT_TRUE(devinfo.HasMouse());
  EXPECT_FALSE(devinfo.HasTouchpad());
  EXPECT_FALSE(devinfo.HasTouchscreen());
  EXPECT_FALSE(devinfo.HasTablet());
}

}  // namespace ui
