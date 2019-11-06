/*
 * Copyright 2019 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SkottieTextAdapter_DEFINED
#define SkottieTextAdapter_DEFINED

#include "modules/skottie/src/SkottieAdapter.h"
#include "modules/skottie/src/text/SkottieShaper.h"
#include "modules/skottie/src/text/TextAnimator.h"
#include "modules/skottie/src/text/TextValue.h"

#include <vector>

namespace skottie {
namespace internal {

class TextAdapter final : public SkNVRefCnt<TextAdapter> {
public:
    TextAdapter(sk_sp<sksg::Group> root, bool hasAnimators);
    ~TextAdapter();

    ADAPTER_PROPERTY(Text, TextValue, TextValue())

    const sk_sp<sksg::Group>& root() const { return fRoot; }

    void applyAnimators(const std::vector<sk_sp<TextAnimator>>&);

private:
    struct FragmentRec {
        SkPoint                       fOrigin; // fragment position

        sk_sp<sksg::Matrix<SkMatrix>> fMatrixNode;
        sk_sp<sksg::Color>            fFillColorNode,
                                      fStrokeColorNode;
    };

    void addFragment(const Shaper::Fragment&);
    void buildDomainMaps(const Shaper::Result&);

    void apply();

    void pushPropsToFragment(const TextAnimator::AnimatedProps&, const FragmentRec&) const;

    void adjustLineTracking(const TextAnimator::ModulatorBuffer&,
                            const TextAnimator::DomainSpan&,
                            float line_tracking) const;

    sk_sp<sksg::Group>       fRoot;
    std::vector<FragmentRec> fFragments;
    TextAnimator::DomainMaps fMaps;

    const bool               fHasAnimators;
};

} // namespace internal
} // namespace skottie

#endif // SkottieTextAdapter_DEFINED
