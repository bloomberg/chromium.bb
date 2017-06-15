/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef ResourceLoadPriority_h
#define ResourceLoadPriority_h

#include "platform/wtf/Allocator.h"

namespace blink {

enum ResourceLoadPriority : int8_t {
  // The unresolved priority is here for the convenience of the clients. It
  // should not be passed to the ResourceLoadScheduler.
  kResourceLoadPriorityUnresolved = -1,
  kResourceLoadPriorityVeryLow = 0,
  kResourceLoadPriorityLow,
  kResourceLoadPriorityMedium,
  kResourceLoadPriorityHigh,
  kResourceLoadPriorityVeryHigh,
  kResourceLoadPriorityLowest = kResourceLoadPriorityVeryLow,
  kResourceLoadPriorityHighest = kResourceLoadPriorityVeryHigh,
};

struct ResourcePriority final {
  STACK_ALLOCATED();

 public:
  enum VisibilityStatus {
    kNotVisible,
    kVisible,
  };

  ResourcePriority() : ResourcePriority(kNotVisible, 0) {}
  ResourcePriority(VisibilityStatus status, int intra_value)
      : visibility(status), intra_priority_value(intra_value) {}

  VisibilityStatus visibility;
  int intra_priority_value;
};

}  // namespace blink

#endif
