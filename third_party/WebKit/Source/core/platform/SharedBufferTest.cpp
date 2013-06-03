/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "core/platform/SharedBuffer.h"

#include "wtf/ArrayBuffer.h"
#include "wtf/RefPtr.h"
#include "wtf/Vector.h"
#include <gtest/gtest.h>

using namespace WebCore;

namespace {

TEST(SharedBufferTest, getAsArrayBuffer)
{
    char testData0[] = "Hello";
    char testData1[] = "World";
    char testData2[] = "Goodbye";

    RefPtr<SharedBuffer> sharedBuffer = SharedBuffer::create(testData0, sizeof(testData0) - 1);
    sharedBuffer->append(testData1, sizeof(testData1) - 1);
    sharedBuffer->append(testData2, sizeof(testData2) - 1);

    RefPtr<ArrayBuffer> arrayBuffer = sharedBuffer->getAsArrayBuffer();

    char expectedConcatenation[] = "HelloWorldGoodbye";
    ASSERT_EQ(sizeof(expectedConcatenation) - 1, arrayBuffer->byteLength());
    EXPECT_EQ(0, memcmp(expectedConcatenation, arrayBuffer->data(), sizeof(expectedConcatenation) - 1));
}

TEST(SharedBufferTest, getAsArrayBufferLargeSegments)
{
    Vector<char> vector0(0x4000);
    for (size_t i = 0; i < vector0.size(); ++i)
        vector0[i] = 'a';
    Vector<char> vector1(0x4000);
    for (size_t i = 0; i < vector1.size(); ++i)
        vector1[i] = 'b';
    Vector<char> vector2(0x4000);
    for (size_t i = 0; i < vector2.size(); ++i)
        vector2[i] = 'c';

    RefPtr<SharedBuffer> sharedBuffer = SharedBuffer::adoptVector(vector0);
    sharedBuffer->append(vector1);
    sharedBuffer->append(vector2);

    RefPtr<ArrayBuffer> arrayBuffer = sharedBuffer->getAsArrayBuffer();

    ASSERT_EQ(0x4000U + 0x4000U + 0x4000U, arrayBuffer->byteLength());
    int position = 0;
    for (int i = 0; i < 0x4000; ++i) {
        EXPECT_EQ('a', static_cast<char*>(arrayBuffer->data())[position]);
        ++position;
    }
    for (int i = 0; i < 0x4000; ++i) {
        EXPECT_EQ('b', static_cast<char*>(arrayBuffer->data())[position]);
        ++position;
    }
    for (int i = 0; i < 0x4000; ++i) {
        EXPECT_EQ('c', static_cast<char*>(arrayBuffer->data())[position]);
        ++position;
    }
}

} // namespace
