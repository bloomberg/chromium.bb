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

#include "platform/SharedBuffer.h"

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

    RefPtr<SharedBuffer> sharedBuffer = SharedBuffer::create(testData0, strlen(testData0));
    sharedBuffer->append(testData1, strlen(testData1));
    sharedBuffer->append(testData2, strlen(testData2));

    RefPtr<ArrayBuffer> arrayBuffer = sharedBuffer->getAsArrayBuffer();

    char expectedConcatenation[] = "HelloWorldGoodbye";
    ASSERT_EQ(strlen(expectedConcatenation), arrayBuffer->byteLength());
    EXPECT_EQ(0, memcmp(expectedConcatenation, arrayBuffer->data(), strlen(expectedConcatenation)));
}

TEST(SharedBufferTest, moveToAvoidsMemcpy)
{
    char testData[] = "Hello";

    RefPtr<SharedBuffer> sharedBuffer = SharedBuffer::create(testData, strlen(testData));

    const char* originalData = sharedBuffer->data();
    const size_t originalSize = sharedBuffer->size();

    Vector<char> result;
    sharedBuffer->moveTo(result);

    EXPECT_TRUE(sharedBuffer->isEmpty());
    EXPECT_EQ(originalData, result.data());
    EXPECT_EQ(originalSize, result.size());
}

TEST(SharedBufferTest, moveToHandlesSegments)
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

    const size_t originalSize = sharedBuffer->size();

    Vector<char> result;
    sharedBuffer->moveTo(result);

    EXPECT_TRUE(sharedBuffer->isEmpty());
    EXPECT_EQ(originalSize, result.size());
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

TEST(SharedBufferTest, copy)
{
    char testData[] = "Habitasse integer eros tincidunt a scelerisque! Enim elit? Scelerisque magnis,"
        "et montes ultrices tristique a! Pid. Velit turpis, dapibus integer rhoncus sociis amet facilisis,"
        "adipiscing pulvinar nascetur magnis tempor sit pulvinar, massa urna enim porttitor sociis sociis proin enim?"
        "Lectus, platea dolor, integer a. A habitasse hac nunc, nunc, nec placerat vut in sit nunc nec, sed. Sociis,"
        "vut! Hac, velit rhoncus facilisis. Rhoncus et, enim, sed et in tristique nunc montes,"
        "natoque nunc sagittis elementum parturient placerat dolor integer? Pulvinar,"
        "magnis dignissim porttitor ac pulvinar mid tempor. A risus sed mid! Magnis elit duis urna,"
        "cras massa, magna duis. Vut magnis pid a! Penatibus aliquet porttitor nunc, adipiscing massa odio lundium,"
        "risus elementum ac turpis massa pellentesque parturient augue. Purus amet turpis pid aliquam?"
        "Dolor est tincidunt? Dolor? Dignissim porttitor sit in aliquam! Tincidunt, non nunc, rhoncus dictumst!"
        "Porta augue etiam. Cursus augue nunc lacus scelerisque. Rhoncus lectus, integer hac, nec pulvinar augue massa,"
        "integer amet nisi facilisis? A! A, enim velit pulvinar elit in non scelerisque in et ultricies amet est!"
        "in porttitor montes lorem et, hac aliquet pellentesque a sed? Augue mid purus ridiculus vel dapibus,"
        "sagittis sed, tortor auctor nascetur rhoncus nec, rhoncus, magna integer. Sit eu massa vut?"
        "Porta augue porttitor elementum, enim, rhoncus pulvinar duis integer scelerisque rhoncus natoque,"
        "mattis dignissim massa ac pulvinar urna, nunc ut. Sagittis, aliquet penatibus proin lorem, pulvinar lectus,"
        "augue proin! Ac, arcu quis. Placerat habitasse, ridiculus ridiculus.";

    unsigned length = strlen(testData);
    RefPtr<SharedBuffer> sharedBuffer = SharedBuffer::create(testData, length);
    sharedBuffer->append(testData, length);
    sharedBuffer->append(testData, length);
    sharedBuffer->append(testData, length);
    // sharedBuffer must contain data more than segmentSize (= 0x1000) to check copy().
    ASSERT_EQ(length * 4, sharedBuffer->size());

    RefPtr<SharedBuffer> clone = sharedBuffer->copy();
    ASSERT_EQ(length * 4, clone->size());
    ASSERT_EQ(0, memcmp(clone->data(), sharedBuffer->data(), clone->size()));

    clone->append(testData, length);
    ASSERT_EQ(length * 5, clone->size());
}

} // namespace
