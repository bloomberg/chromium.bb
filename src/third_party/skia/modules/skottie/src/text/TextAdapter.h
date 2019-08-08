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
#include "modules/skottie/src/text/TextValue.h"

#include <vector>

namespace sksg {
class Group;
} // namespace sksg

namespace skottie {

class TextAdapter final : public SkNVRefCnt<TextAdapter> {
public:
    explicit TextAdapter(sk_sp<sksg::Group> root);
    ~TextAdapter();

    ADAPTER_PROPERTY(Text, TextValue, TextValue())

    const sk_sp<sksg::Group>& root() const { return fRoot; }

private:
    struct FragmentRec;

    FragmentRec buildFragment(const skottie::Shaper::Fragment&) const;

    void apply();

    sk_sp<sksg::Group>       fRoot;
    std::vector<FragmentRec> fFragments;
};

} // namespace skottie

#endif // SkottieTextAdapter_DEFINED
