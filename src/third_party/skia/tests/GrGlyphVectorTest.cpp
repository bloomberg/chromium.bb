/*
* Copyright 2022 Google LLC
*
* Use of this source code is governed by a BSD-style license that can be
* found in the LICENSE file.
 */

#include "src/core/SkGlyph.h"
#include "src/gpu/ganesh/GrResourceProvider.h"

#include "src/core/SkGlyphBuffer.h"
#include "src/core/SkReadBuffer.h"
#include "src/core/SkStrikeCache.h"
#include "src/core/SkStrikeSpec.h"
#include "src/core/SkWriteBuffer.h"
#include "src/text/gpu/GlyphVector.h"
#include "src/text/gpu/SubRunAllocator.h"
#include "tests/Test.h"

using GlyphVector = sktext::gpu::GlyphVector;
using SubRunAllocator = sktext::gpu::SubRunAllocator;

namespace sktext::gpu {

class TestingPeer {
public:
    static const SkDescriptor& GetDescriptor(const GlyphVector& v) {
        return v.fSkStrike->getDescriptor();
    }
    static SkSpan<GlyphVector::Variant> GetGlyphs(const GlyphVector& v) {
        return v.fGlyphs;
    }
};

DEF_TEST(GlyphVector_Serialization, r) {
    SkFont font;
    auto [strikeSpec, _] = SkStrikeSpec::MakeCanonicalized(font);

    SubRunAllocator alloc;

    SkBulkGlyphMetricsAndImages glyphFinder{strikeSpec};
    const int N = 10;
    SkGlyphVariant* glyphs = alloc.makePODArray<SkGlyphVariant>(N);
    for (int i = 0; i < N; i++) {
        glyphs[i] = glyphFinder.glyph(SkPackedGlyphID(SkTo<SkGlyphID>(i + 1)));
    }

    GlyphVector src = GlyphVector::Make(
            strikeSpec.findOrCreateStrike(), SkMakeSpan(glyphs, N), &alloc);

    SkBinaryWriteBuffer wBuffer;
    src.flatten(wBuffer);

    auto data = wBuffer.snapshotAsData();
    SkReadBuffer rBuffer{data->data(), data->size()};
    auto dst = GlyphVector::MakeFromBuffer(rBuffer, nullptr, &alloc);
    REPORTER_ASSERT(r, dst.has_value());
    REPORTER_ASSERT(r, TestingPeer::GetDescriptor(src) == TestingPeer::GetDescriptor(*dst));

    auto srcGlyphs = TestingPeer::GetGlyphs(src);
    auto dstGlyphs = TestingPeer::GetGlyphs(*dst);
    for (auto [srcGlyphID, dstGlyphID] : SkMakeZip(srcGlyphs, dstGlyphs)) {
        REPORTER_ASSERT(r, srcGlyphID.packedGlyphID == dstGlyphID.packedGlyphID);
    }
}

DEF_TEST(GlyphVector_BadLengths, r) {
    {
        SkFont font;
        auto [strikeSpec, _] = SkStrikeSpec::MakeCanonicalized(font);

        // Make broken stream by hand - zero length
        SkBinaryWriteBuffer wBuffer;
        strikeSpec.descriptor().flatten(wBuffer);
        wBuffer.write32(0);  // length
        auto data = wBuffer.snapshotAsData();
        SkReadBuffer rBuffer{data->data(), data->size()};
        SubRunAllocator alloc;
        auto dst = GlyphVector::MakeFromBuffer(rBuffer, nullptr, &alloc);
        REPORTER_ASSERT(r, !dst.has_value());
    }

    {
        SkFont font;
        auto [strikeSpec, _] = SkStrikeSpec::MakeCanonicalized(font);

        // Make broken stream by hand - stream is too short
        SkBinaryWriteBuffer wBuffer;
        strikeSpec.descriptor().flatten(wBuffer);
        wBuffer.write32(5);  // length
        wBuffer.writeUInt(12);  // random data
        wBuffer.writeUInt(12);  // random data
        wBuffer.writeUInt(12);  // random data
        auto data = wBuffer.snapshotAsData();
        SkReadBuffer rBuffer{data->data(), data->size()};
        SubRunAllocator alloc;
        auto dst = GlyphVector::MakeFromBuffer(rBuffer, nullptr, &alloc);
        REPORTER_ASSERT(r, !dst.has_value());
    }

    {
        SkFont font;
        auto [strikeSpec, _] = SkStrikeSpec::MakeCanonicalized(font);

        // Make broken stream by hand - length out of range of safe calculations
        SkBinaryWriteBuffer wBuffer;
        strikeSpec.descriptor().flatten(wBuffer);
        wBuffer.write32(INT_MAX - 10);  // length
        wBuffer.writeUInt(12);  // random data
        wBuffer.writeUInt(12);  // random data
        wBuffer.writeUInt(12);  // random data
        auto data = wBuffer.snapshotAsData();
        SkReadBuffer rBuffer{data->data(), data->size()};
        SubRunAllocator alloc;
        auto dst = GlyphVector::MakeFromBuffer(rBuffer, nullptr, &alloc);
        REPORTER_ASSERT(r, !dst.has_value());
    }
}

}  // namespace sktext::gpu
