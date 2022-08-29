/*
 * Copyright 2020 Google LLC
 *
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef SKSL_BUILTINMAP
#define SKSL_BUILTINMAP

#include "include/private/SkSLProgramElement.h"
#include "include/private/SkTHash.h"

#include <functional>
#include <memory>
#include <string>

namespace SkSL {

/**
 * Represents the builtin elements in the Context.
 */
class BuiltinMap {
public:
    BuiltinMap(BuiltinMap* parent) : fParent(parent) {}

    void insertOrDie(std::string key, std::unique_ptr<ProgramElement> element);

    const ProgramElement* find(const std::string& key);

    const ProgramElement* findAndInclude(const std::string& key);

    void resetAlreadyIncluded();

    void foreach(const std::function<void(const std::string&, const ProgramElement&)>& fn) const;

private:
    struct BuiltinElement {
        std::unique_ptr<ProgramElement> fElement;
        bool fAlreadyIncluded = false;
    };

    SkTHashMap<std::string, BuiltinElement> fElements;
    BuiltinMap* fParent = nullptr;
};

} // namespace SkSL

#endif
