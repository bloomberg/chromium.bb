// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "config.h"
#include "core/experiments/Experiments.h"

#include "core/dom/ExceptionCode.h"

namespace blink {

bool Experiments::isApiEnabled(ExecutionContext* executionContext, const String& apiName)
{
    return false;
}

DOMException* Experiments::createApiDisabledException(const String& apiName)
{
    // TODO(chasej): Update message with URL to experiments site, when live
    return DOMException::create(NotSupportedError, "The '" + apiName + "' API is currently enabled in limited experiments. Please see [Chrome experiments website URL] for information on enabling this experiment on your site.");
}

} // namespace blink
