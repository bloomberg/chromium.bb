// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef Experiments_h
#define Experiments_h

#include "core/CoreExport.h"
#include "core/dom/DOMException.h"
#include "core/dom/ExecutionContext.h"
#include "wtf/text/WTFString.h"

namespace blink {

// The Experimental Framework (EF) provides limited access to experimental APIs,
// on a per-origin basis.  This class provides the implementation to check if
// the experimental API should be enabled for the current context.  This class
// is not for direct use by API implementers.  Instead, the ExperimentalFeatures
// generated class provides a static method for each API to check if it is
// enabled. Experimental APIs must be defined in RuntimeEnabledFeatures.in,
// which is used to generate ExperimentalFeatures.h/cpp.
//
// Experimental APIs are defined by string names, provided by the implementers.
// The EF code does not maintain an enum or constant list for experiment names.
// Instead, the EF validates the name provided by the API implementation against
// any provided API keys.
// TODO(chasej): Link to documentation, or provide more detail on keys, .etc
class CORE_EXPORT Experiments {
public:
    // Creates a NotSupportedError exception with a message explaining to
    // external developers why the API is disabled and how to join API
    // experiments.
    static DOMException* createApiDisabledException(const String& apiName);

private:
    friend class ExperimentalFeatures;
    friend class ExperimentsTest;

    explicit Experiments();

    static bool isApiEnabled(ExecutionContext*, const String& apiName, String* errorMessage);
};

} // namespace blink

#endif // Experiments_h
