// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/experiments/Experiments.h"

#include "core/dom/DOMException.h"
#include "core/dom/ExceptionCode.h"
#include <gtest/gtest.h>

namespace blink {
namespace {

const char* kNonExistingAPIName = "This API does not exist";

TEST(ExperimentsTest, EnabledNonExistingAPI)
{
    bool isNonExistingApiEnabled = Experiments::isApiEnabled(nullptr, kNonExistingAPIName);
    EXPECT_FALSE(isNonExistingApiEnabled);
}

TEST(ExperimentsTest, DisabledException)
{
    DOMException* disabledException = Experiments::createApiDisabledException(kNonExistingAPIName);
    ASSERT_TRUE(disabledException) << "An exception should have been created";
    EXPECT_EQ(DOMException::getErrorName(NotSupportedError), disabledException->name());
    EXPECT_TRUE(disabledException->message().contains(kNonExistingAPIName)) << "Message should contain the API name, was: " << disabledException->message();
}

} // namespace
} // namespace blink
