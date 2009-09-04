/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// Use this file to assert that various WebKit API enum values continue
// matching WebCore defined enum values.

// FIXME: Move all of the COMPILE_ASSERTs for enums into this file.

#include "EditorInsertAction.h"
#include "TextAffinity.h"
#include "WebEditingAction.h"
#include "WebTextAffinity.h"
#include <wtf/Assertions.h>

using namespace WebCore;

namespace WebKit {

#define COMPILE_ASSERT_MATCHING_ENUM(webkit_name, webcore_name) \
    COMPILE_ASSERT(int(webkit_name) == int(webcore_name), webkit_name)

COMPILE_ASSERT_MATCHING_ENUM(WebEditingActionTyped, EditorInsertActionTyped);
COMPILE_ASSERT_MATCHING_ENUM(WebEditingActionPasted, EditorInsertActionPasted);
COMPILE_ASSERT_MATCHING_ENUM(WebEditingActionDropped, EditorInsertActionDropped);

COMPILE_ASSERT_MATCHING_ENUM(WebTextAffinityUpstream, UPSTREAM);
COMPILE_ASSERT_MATCHING_ENUM(WebTextAffinityDownstream, DOWNSTREAM);

} // namespace WebKit
