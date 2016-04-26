// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/payments/ShippingAddress.h"

#include "testing/gtest/include/gtest/gtest.h"
#include <utility>

namespace blink {
namespace {

TEST(ShippingAddressTest, ValuesAreCopiedOver)
{
    mojom::blink::ShippingAddressPtr input = mojom::blink::ShippingAddress::New();
    input->region_code = "US";
    input->address_line = mojo::WTFArray<WTF::String>::New(3);
    input->address_line[0] = "340 Main St";
    input->address_line[1] = "BIN1";
    input->address_line[2] = "First floor";
    input->administrative_area = "CA";
    input->locality = "Los Angeles";
    input->dependent_locality = "Venice";
    input->postal_code = "90291";
    input->sorting_code = "CEDEX";
    input->language_code = "en";
    input->script_code = "Latn";
    input->organization = "Google";
    input->recipient = "Jon Doe";

    ShippingAddress output(std::move(input));

    EXPECT_EQ("US", output.regionCode());
    EXPECT_EQ(3U, output.addressLine().size());
    EXPECT_EQ("340 Main St", output.addressLine()[0]);
    EXPECT_EQ("BIN1", output.addressLine()[1]);
    EXPECT_EQ("First floor", output.addressLine()[2]);
    EXPECT_EQ("CA", output.administrativeArea());
    EXPECT_EQ("Los Angeles", output.locality());
    EXPECT_EQ("Venice", output.dependentLocality());
    EXPECT_EQ("90291", output.postalCode());
    EXPECT_EQ("CEDEX", output.sortingCode());
    EXPECT_EQ("en-Latn", output.languageCode());
    EXPECT_EQ("Google", output.organization());
    EXPECT_EQ("Jon Doe", output.recipient());
}

TEST(ShippingAddressTest, IgnoreScriptCodeWithEmptyLanguageCode)
{
    mojom::blink::ShippingAddressPtr input = mojom::blink::ShippingAddress::New();
    input->script_code = "Latn";

    ShippingAddress output(std::move(input));

    EXPECT_TRUE(output.languageCode().isEmpty());
}

TEST(ShippingAddressTest, NoHyphenWithEmptyScriptCode)
{
    mojom::blink::ShippingAddressPtr input = mojom::blink::ShippingAddress::New();
    input->language_code = "en";

    ShippingAddress output(std::move(input));

    EXPECT_EQ("en", output.languageCode());
}

} // namespace
} // namespace blink
