// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/memory/ref_counted.h"
#include "base/strings/utf_string_conversions.h"
#include "device/usb/mock_usb_device.h"
#include "device/usb/mojo/type_converters.h"
#include "device/usb/public/cpp/usb_utils.h"
#include "device/usb/public/mojom/device_enumeration_options.mojom.h"
#include "device/usb/usb_descriptors.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace device {

namespace {

using testing::Return;

const uint8_t kEndpointAddress_1 = 0x81;
const uint8_t kEndpointAddress_2 = 0x02;

class UsbUtilsTest : public testing::Test {
 public:
  void SetUp() override {
    UsbConfigDescriptor config(1, false, false, 0);
    UsbInterfaceDescriptor interface(1, 0, 0xff, 0x42, 0x01);
    // Endpoint 1 IN
    interface.endpoints.emplace_back(kEndpointAddress_1, 0x01, 0x0400, 0x08);
    // Endpoint 2 OUT
    interface.endpoints.emplace_back(kEndpointAddress_2, 0x11, 0x0400, 0x08);

    config.interfaces.push_back(interface);

    android_phone_ = new MockUsbDevice(0x18d1, 0x4ee2, "Google Inc.", "Nexus 5",
                                       "ABC123", {config});
  }

 protected:
  scoped_refptr<MockUsbDevice> android_phone_;
};

TEST_F(UsbUtilsTest, MatchAny) {
  auto filter = mojom::UsbDeviceFilter::New();
  EXPECT_TRUE(UsbDeviceFilterMatches(*filter, *android_phone_));
}

TEST_F(UsbUtilsTest, MatchVendorId) {
  auto filter = mojom::UsbDeviceFilter::New();
  filter->has_vendor_id = true;
  filter->vendor_id = 0x18d1;
  EXPECT_TRUE(UsbDeviceFilterMatches(*filter, *android_phone_));
}

TEST_F(UsbUtilsTest, MatchVendorIdNegative) {
  auto filter = mojom::UsbDeviceFilter::New();
  filter->has_vendor_id = true;
  filter->vendor_id = 0x1d6b;
  EXPECT_FALSE(UsbDeviceFilterMatches(*filter, *android_phone_));
}

TEST_F(UsbUtilsTest, MatchProductId) {
  auto filter = mojom::UsbDeviceFilter::New();
  filter->has_vendor_id = true;
  filter->vendor_id = 0x18d1;
  filter->has_product_id = true;
  filter->product_id = 0x4ee2;
  EXPECT_TRUE(UsbDeviceFilterMatches(*filter, *android_phone_));
}

TEST_F(UsbUtilsTest, MatchProductIdNegative) {
  auto filter = mojom::UsbDeviceFilter::New();
  filter->has_vendor_id = true;
  filter->vendor_id = 0x18d1;
  filter->has_product_id = true;
  filter->product_id = 0x4ee1;
  EXPECT_FALSE(UsbDeviceFilterMatches(*filter, *android_phone_));
}

TEST_F(UsbUtilsTest, MatchInterfaceClass) {
  auto filter = mojom::UsbDeviceFilter::New();
  filter->has_class_code = true;
  filter->class_code = 0xff;
  EXPECT_TRUE(UsbDeviceFilterMatches(*filter, *android_phone_));
}

TEST_F(UsbUtilsTest, MatchInterfaceClassNegative) {
  auto filter = mojom::UsbDeviceFilter::New();
  filter->has_class_code = true;
  filter->class_code = 0xe0;
  EXPECT_FALSE(UsbDeviceFilterMatches(*filter, *android_phone_));
}

TEST_F(UsbUtilsTest, MatchInterfaceSubclass) {
  auto filter = mojom::UsbDeviceFilter::New();
  filter->has_class_code = true;
  filter->class_code = 0xff;
  filter->has_subclass_code = true;
  filter->subclass_code = 0x42;
  EXPECT_TRUE(UsbDeviceFilterMatches(*filter, *android_phone_));
}

TEST_F(UsbUtilsTest, MatchInterfaceSubclassNegative) {
  auto filter = mojom::UsbDeviceFilter::New();
  filter->has_class_code = true;
  filter->class_code = 0xff;
  filter->has_subclass_code = true;
  filter->subclass_code = 0x01;
  EXPECT_FALSE(UsbDeviceFilterMatches(*filter, *android_phone_));
}

TEST_F(UsbUtilsTest, MatchInterfaceProtocol) {
  auto filter = mojom::UsbDeviceFilter::New();
  filter->has_class_code = true;
  filter->class_code = 0xff;
  filter->has_subclass_code = true;
  filter->subclass_code = 0x42;
  filter->has_protocol_code = true;
  filter->protocol_code = 0x01;
  EXPECT_TRUE(UsbDeviceFilterMatches(*filter, *android_phone_));
}

TEST_F(UsbUtilsTest, MatchInterfaceProtocolNegative) {
  auto filter = mojom::UsbDeviceFilter::New();
  filter->has_class_code = true;
  filter->class_code = 0xff;
  filter->has_subclass_code = true;
  filter->subclass_code = 0x42;
  filter->has_protocol_code = true;
  filter->protocol_code = 0x02;
  EXPECT_FALSE(UsbDeviceFilterMatches(*filter, *android_phone_));
}

TEST_F(UsbUtilsTest, MatchSerialNumber) {
  auto filter = mojom::UsbDeviceFilter::New();
  filter->serial_number = base::ASCIIToUTF16("ABC123");
  EXPECT_TRUE(UsbDeviceFilterMatches(*filter, *android_phone_));
  filter->has_vendor_id = true;
  filter->vendor_id = 0x18d1;
  EXPECT_TRUE(UsbDeviceFilterMatches(*filter, *android_phone_));
  filter->vendor_id = 0x18d2;
  EXPECT_FALSE(UsbDeviceFilterMatches(*filter, *android_phone_));
  filter->vendor_id = 0x18d1;
  filter->serial_number = base::ASCIIToUTF16("DIFFERENT");
  EXPECT_FALSE(UsbDeviceFilterMatches(*filter, *android_phone_));
}

TEST_F(UsbUtilsTest, MatchAnyEmptyList) {
  std::vector<mojom::UsbDeviceFilterPtr> filters;
  ASSERT_TRUE(UsbDeviceFilterMatchesAny(filters, *android_phone_));
}

TEST_F(UsbUtilsTest, MatchesAnyVendorId) {
  std::vector<mojom::UsbDeviceFilterPtr> filters;
  filters.push_back(mojom::UsbDeviceFilter::New());
  filters.back()->has_vendor_id = true;
  filters.back()->vendor_id = 0x18d1;
  ASSERT_TRUE(UsbDeviceFilterMatchesAny(filters, *android_phone_));
}

TEST_F(UsbUtilsTest, MatchesAnyVendorIdNegative) {
  std::vector<mojom::UsbDeviceFilterPtr> filters;
  filters.push_back(mojom::UsbDeviceFilter::New());
  filters.back()->has_vendor_id = true;
  filters.back()->vendor_id = 0x1d6b;
  ASSERT_FALSE(UsbDeviceFilterMatchesAny(filters, *android_phone_));
}

TEST_F(UsbUtilsTest, EndpointDirectionNumberConversion) {
  UsbInterfaceDescriptor native_interface =
      android_phone_->configurations()[0].interfaces[0];
  EXPECT_EQ(2u, native_interface.endpoints.size());

  // Check Endpoint 1 IN
  auto& native_endpoint_1 = native_interface.endpoints[0];
  auto mojo_endpoint_1 =
      device::mojom::UsbEndpointInfo::From(native_endpoint_1);

  ASSERT_EQ(ConvertEndpointAddressToNumber(native_endpoint_1), 0x01);
  ASSERT_EQ(ConvertEndpointNumberToAddress(*mojo_endpoint_1),
            kEndpointAddress_1);

  // Check Endpoint 2 OUT
  auto& native_endpoint_2 = native_interface.endpoints[1];
  auto mojo_endpoint_2 =
      device::mojom::UsbEndpointInfo::From(native_endpoint_2);
  ASSERT_EQ(ConvertEndpointAddressToNumber(native_endpoint_2), 0x02);
  ASSERT_EQ(ConvertEndpointNumberToAddress(*mojo_endpoint_2),
            kEndpointAddress_2);
}

}  // namespace

}  // namespace device
