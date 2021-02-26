/*
 * Copyright 2016 Google Inc.
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SKSL_EXTENSION
#define SKSL_EXTENSION

#include "src/sksl/ir/SkSLProgramElement.h"

namespace SkSL {

/**
 * An extension declaration.
 */
class Extension final : public ProgramElement {
public:
    static constexpr Kind kProgramElementKind = Kind::kExtension;

    Extension(int offset, String name)
        : INHERITED(offset, kProgramElementKind)
        , fName(std::move(name)) {}

    const String& name() const {
        return fName;
    }

    std::unique_ptr<ProgramElement> clone() const override {
        return std::unique_ptr<ProgramElement>(new Extension(fOffset, this->name()));
    }

    String description() const override {
        return "#extension " + this->name() + " : enable";
    }

private:
    String fName;

    using INHERITED = ProgramElement;
};

}  // namespace SkSL

#endif
