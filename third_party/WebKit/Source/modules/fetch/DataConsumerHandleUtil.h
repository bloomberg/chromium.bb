// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DataConsumerHandleUtil_h
#define DataConsumerHandleUtil_h

#include "modules/ModulesExport.h"
#include "modules/fetch/FetchDataConsumerHandle.h"
#include "public/platform/WebDataConsumerHandle.h"
#include <memory>

namespace blink {

// Returns a handle that returns ShouldWait for read / beginRead and
// UnexpectedError for endRead.
MODULES_EXPORT std::unique_ptr<WebDataConsumerHandle>
createWaitingDataConsumerHandle();

// Returns a FetchDataConsumerHandle that wraps WebDataConsumerHandle.
MODULES_EXPORT std::unique_ptr<FetchDataConsumerHandle>
    createFetchDataConsumerHandleFromWebHandle(
        std::unique_ptr<WebDataConsumerHandle>);

}  // namespace blink

#endif  // DataConsumerHandleUtil_h
