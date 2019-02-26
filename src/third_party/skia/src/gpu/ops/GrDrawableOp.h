/*
 * Copyright 2018 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef GrDrawableOp_DEFINED
#define GrDrawableOp_DEFINED

#include "GrOp.h"

#include "GrSemaphore.h"
#include "SkDrawable.h"
#include "SkMatrix.h"

class GrDrawableOp final : public GrOp {
public:
    DEFINE_OP_CLASS_ID

    static std::unique_ptr<GrDrawableOp> Make(GrContext*,
                                              std::unique_ptr<SkDrawable::GpuDrawHandler> drawable,
                                              const SkRect& bounds);

    const char* name() const override { return "Drawable"; }

#ifdef SK_DEBUG
    SkString dumpInfo() const override {
        return INHERITED::dumpInfo();
    }
#endif

private:
    friend class GrOpMemoryPool; // for ctor

    GrDrawableOp(std::unique_ptr<SkDrawable::GpuDrawHandler>, const SkRect& bounds);

    CombineResult onCombineIfPossible(GrOp* that, const GrCaps& caps) override {
        return CombineResult::kCannotCombine;
    }
    void onPrepare(GrOpFlushState*) override {}

    void onExecute(GrOpFlushState*, const SkRect& chainBounds) override;

    std::unique_ptr<SkDrawable::GpuDrawHandler> fDrawable;

    typedef GrOp INHERITED;
};

#endif

