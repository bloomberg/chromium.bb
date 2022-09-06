/*
 * Copyright 2021 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef skgpu_UniformManager_DEFINED
#define skgpu_UniformManager_DEFINED

#include "include/core/SkRefCnt.h"
#include "include/core/SkSpan.h"
#include "include/private/SkColorData.h"
#include "include/private/SkTDArray.h"
#include "include/private/SkVx.h"
#include "src/core/SkSLTypeShared.h"

class SkM44;
struct SkPoint;
struct SkRect;
class SkUniform;
class SkUniformDataBlock;

namespace skgpu::graphite {

enum class CType : unsigned;

enum class Layout {
    kStd140,
    kStd430,
    kMetal, /** This is our own self-imposed layout we use for Metal. */
};

// TODO: This is only used in the SkPipelineDataGatherer - maybe hide it better.
class UniformManager {
public:
    UniformManager(Layout layout);

    SkUniformDataBlock peekData() const;
    int size() const { return fStorage.count(); }

    void reset();
#ifdef SK_DEBUG
    void checkReset() const;
    void setExpectedUniforms(SkSpan<const SkUniform>);
    void checkExpected(SkSLType, unsigned int count);
    void doneWithExpectedUniforms();
#endif

    // TODO: do we need to add a 'makeArray' parameter to these?
    void write(const SkM44&);
    void write(const SkColor4f*, int count);
    void write(const SkPMColor4f*, int count);
    void write(const SkPMColor4f& color) { this->write(&color, 1); }
    void write(const SkRect&);
    void write(SkPoint);
    void write(const float*, int count);
    void write(float f) { this->write(&f, 1); }
    void write(int);
    void write(skvx::float2);
    void write(skvx::float4);

private:
    SkSLType getUniformTypeForLayout(SkSLType type);
    void write(SkSLType type, unsigned int count, const void* src);

    using WriteUniformFn = uint32_t(*)(SkSLType type,
                                       CType ctype,
                                       void *dest,
                                       int n,
                                       const void *src);

    WriteUniformFn fWriteUniform;
    Layout fLayout;  // TODO: eventually 'fLayout' will not need to be stored
#ifdef SK_DEBUG
    uint32_t fCurUBOOffset;
    uint32_t fCurUBOMaxAlignment;

    SkSpan<const SkUniform> fExpectedUniforms;
    int fExpectedUniformIndex = 0;
#endif // SK_DEBUG
    uint32_t fOffset;

    SkTDArray<char> fStorage;
};

} // namespace skgpu

#endif // skgpu_UniformManager_DEFINED
