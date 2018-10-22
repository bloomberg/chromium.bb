/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "SkColorSpace_XYZ.h"
#include "SkColorSpacePriv.h"
#include "SkColorSpaceXformPriv.h"
#include "SkOpts.h"

SkColorSpace_XYZ::SkColorSpace_XYZ(SkGammaNamed gammaNamed,
                                   const SkColorSpaceTransferFn* transferFn,
                                   const SkMatrix44& toXYZD50)
        : fGammaNamed(gammaNamed)
        , fToXYZD50(toXYZD50)
        , fToXYZD50Hash(SkOpts::hash_fn(toXYZD50.values(), 16 * sizeof(SkMScalar), 0))
        , fFromXYZD50(SkMatrix44::kUninitialized_Constructor) {
    SkASSERT(fGammaNamed != kNonStandard_SkGammaNamed || transferFn);
    if (transferFn) {
        fTransferFn = *transferFn;
    }
}

const SkMatrix44* SkColorSpace_XYZ::onFromXYZD50() const {
    fFromXYZOnce([this] {
        if (!fToXYZD50.invert(&fFromXYZD50)) {
            // If a client gives us a dst gamut with a transform that we can't invert, we will
            // simply give them back a transform to sRGB gamut.
            SkMatrix44 srgbToxyzD50(SkMatrix44::kUninitialized_Constructor);
            srgbToxyzD50.set3x3RowMajorf(gSRGB_toXYZD50);
            srgbToxyzD50.invert(&fFromXYZD50);
        }
    });
    return &fFromXYZD50;
}

bool SkColorSpace_XYZ::onGammaCloseToSRGB() const {
    return kSRGB_SkGammaNamed == fGammaNamed;
}

bool SkColorSpace_XYZ::onGammaIsLinear() const {
    return kLinear_SkGammaNamed == fGammaNamed;
}

bool SkColorSpace_XYZ::onIsNumericalTransferFn(SkColorSpaceTransferFn* coeffs) const {
    switch (fGammaNamed) {
        case kSRGB_SkGammaNamed:
            *coeffs = gSRGB_TransferFn;
            break;
        case k2Dot2Curve_SkGammaNamed:
            *coeffs = g2Dot2_TransferFn;
            break;
        case kLinear_SkGammaNamed:
            *coeffs = gLinear_TransferFn;
            break;
        case kNonStandard_SkGammaNamed:
            *coeffs = fTransferFn;
            break;
        default:
            SkDEBUGFAIL("Unknown named gamma");
            return false;
    }

    return true;
}

sk_sp<SkColorSpace> SkColorSpace_XYZ::makeLinearGamma() const {
    if (this->gammaIsLinear()) {
        return sk_ref_sp(const_cast<SkColorSpace_XYZ*>(this));
    }
    return SkColorSpace::MakeRGB(kLinear_SkGammaNamed, fToXYZD50);
}

sk_sp<SkColorSpace> SkColorSpace_XYZ::makeSRGBGamma() const {
    if (this->gammaCloseToSRGB()) {
        return sk_ref_sp(const_cast<SkColorSpace_XYZ*>(this));
    }
    return SkColorSpace::MakeRGB(kSRGB_SkGammaNamed, fToXYZD50);
}

sk_sp<SkColorSpace> SkColorSpace_XYZ::makeColorSpin() const {
    SkMatrix44 spin(SkMatrix44::kUninitialized_Constructor);
    spin.set3x3(0, 1, 0, 0, 0, 1, 1, 0, 0);
    spin.postConcat(fToXYZD50);
    (void)spin.getType();  // Pre-cache spin matrix type to avoid races in future getType() calls.
    return sk_sp<SkColorSpace>(new SkColorSpace_XYZ(fGammaNamed, &fTransferFn, spin));
}
