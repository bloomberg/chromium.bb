/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "tests/Test.h"

#include "include/gpu/graphite/Context.h"
#include "include/gpu/graphite/Recorder.h"
#include "src/core/SkPipelineData.h"
#include "src/core/SkUniform.h"
#include "src/gpu/graphite/PipelineDataCache.h"
#include "src/gpu/graphite/RecorderPriv.h"

using namespace skgpu::graphite;

DEF_GRAPHITE_TEST_FOR_CONTEXTS(PipelineDataCacheTest, reporter, context) {
    std::unique_ptr<Recorder> recorder = context->makeRecorder();

    auto cache = recorder->priv().uniformDataCache();

    REPORTER_ASSERT(reporter, cache->count() == 0);

    // Nullptr should already be in the cache
    {
        UniformDataCache::Index invalid;
        REPORTER_ASSERT(reporter, !invalid.isValid());

        const SkUniformDataBlock* lookup = cache->lookup(invalid);
        REPORTER_ASSERT(reporter, !lookup);
    }

    static const int kSize = 16;

    // Add a new unique UDB
    static const char kMemory1[kSize] = {
            7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22
    };
    SkUniformDataBlock udb1(SkMakeSpan(kMemory1, kSize));
    UniformDataCache::Index id1;
    {
        id1 = cache->insert(udb1);
        REPORTER_ASSERT(reporter, id1.isValid());
        const SkUniformDataBlock* lookup = cache->lookup(id1);
        REPORTER_ASSERT(reporter, *lookup == udb1);

        REPORTER_ASSERT(reporter, cache->count() == 1);
    }

    // Try to add a duplicate UDB
    {
        static const char kMemory2[kSize] = {
                7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22
        };
        SkUniformDataBlock udb2(SkMakeSpan(kMemory2, kSize));
        UniformDataCache::Index id2 = cache->insert(udb2);
        REPORTER_ASSERT(reporter, id2.isValid());
        REPORTER_ASSERT(reporter, id2 == id1);
        const SkUniformDataBlock* lookup = cache->lookup(id2);
        REPORTER_ASSERT(reporter, *lookup == udb1);
        REPORTER_ASSERT(reporter, *lookup == udb2);

        REPORTER_ASSERT(reporter, cache->count() == 1);
    }

    // Add a second new unique UDB
    {
        static const char kMemory3[kSize] = {
                6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21
        };
        SkUniformDataBlock udb3(SkMakeSpan(kMemory3, kSize));
        UniformDataCache::Index id3 = cache->insert(udb3);
        REPORTER_ASSERT(reporter, id3.isValid());
        REPORTER_ASSERT(reporter, id3 != id1);
        const SkUniformDataBlock* lookup = cache->lookup(id3);
        REPORTER_ASSERT(reporter, *lookup == udb3);
        REPORTER_ASSERT(reporter, *lookup != udb1);

        REPORTER_ASSERT(reporter, cache->count() == 2);
    }

    // TODO(robertphillips): expand this test to exercise all the UDB comparison failure modes
}
