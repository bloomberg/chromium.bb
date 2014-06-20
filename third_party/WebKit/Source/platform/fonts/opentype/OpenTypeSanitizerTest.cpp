// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"

#include "platform/fonts/opentype/OpenTypeSanitizer.h"

#include "platform/RuntimeEnabledFeatures.h"
#include "platform/SharedBuffer.h"
#include "public/platform/Platform.h"
#include "public/platform/WebUnitTestSupport.h"
#include <gtest/gtest.h>

using namespace WebCore;
using namespace blink;

namespace {

static PassRefPtr<SharedBuffer> readFont(const char* fileName)
{
    String filePath = Platform::current()->unitTestSupport()->webKitRootDir();
    filePath.append("/LayoutTests/resources/");
    filePath.append(fileName);

    return Platform::current()->unitTestSupport()->readFromFile(filePath);
}

TEST(OpenTypeSanitizerTest, TestOTF)
{
    RefPtr<SharedBuffer> buffer = readFont("Ahem.otf");
    OpenTypeSanitizer sanitizer(buffer.get());
    RefPtr<SharedBuffer> sanitized = sanitizer.sanitize();
    EXPECT_GE(sanitized->size(), 4u);
    const char* data = sanitized->data();
    // Sfnt version: 'OTTO'
    EXPECT_TRUE(data[0] == 'O' && data[1] == 'T' && data[2] == 'T' && data[3] == 'O');
}

TEST(OpenTypeSanitizerTest, TestTTF)
{
    RefPtr<SharedBuffer> buffer = readFont("Ahem.ttf");
    OpenTypeSanitizer sanitizer(buffer.get());
    RefPtr<SharedBuffer> sanitized = sanitizer.sanitize();
    EXPECT_GE(sanitized->size(), 4u);
    const char* data = sanitized->data();
    // Sfnt version: 0x00010000
    EXPECT_TRUE(data[0] == 0 && data[1] == 1 && data[2] == 0 && data[3] == 0);
}

TEST(OpenTypeSanitizerTest, TestWOFF)
{
    RefPtr<SharedBuffer> buffer = readFont("Ahem.woff");
    EXPECT_GE(buffer->size(), 4u);
    const char* originalData = buffer->data();
    // Sfnt version: 'wOFF'
    EXPECT_TRUE(originalData[0] == 'w' && originalData[1] == 'O' && originalData[2] == 'F' && originalData[3] == 'F');

    OpenTypeSanitizer sanitizer(buffer.get());
    RefPtr<SharedBuffer> sanitized = sanitizer.sanitize();
    EXPECT_GE(sanitized->size(), 4u);
    const char* data = sanitized->data();
    // Decompressed sfnt version: 'OTTO'
    EXPECT_TRUE(data[0] == 'O' && data[1] == 'T' && data[2] == 'T' && data[3] == 'O');
}

TEST(OpenTypeSanitizerTest, TestWOFF2)
{
    RuntimeEnabledFeatures::setWOFF2Enabled(true);

    RefPtr<SharedBuffer> buffer = readFont("Ahem.woff2");
    EXPECT_GE(buffer->size(), 4u);
    const char* originalData = buffer->data();
    // Sfnt version: 'wOF2'
    EXPECT_TRUE(originalData[0] == 'w' && originalData[1] == 'O' && originalData[2] == 'F' && originalData[3] == '2');

    OpenTypeSanitizer sanitizer(buffer.get());
    RefPtr<SharedBuffer> sanitized = sanitizer.sanitize();
    EXPECT_GE(sanitized->size(), 4u);
    const char* data = sanitized->data();
    // Decompressed sfnt version: 0x00010000
    EXPECT_TRUE(data[0] == 0 && data[1] == 1 && data[2] == 0 && data[3] == 0);
}

} // namespace
