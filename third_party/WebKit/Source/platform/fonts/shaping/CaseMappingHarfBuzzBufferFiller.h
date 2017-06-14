// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CaseMappingHarfBuzzBufferFiller_h
#define CaseMappingHarfBuzzBufferFiller_h

#include "platform/wtf/Allocator.h"
#include "platform/wtf/text/AtomicString.h"
#include "platform/wtf/text/Unicode.h"

#include <hb.h>

namespace blink {

enum class CaseMapIntend { kKeepSameCase, kUpperCase, kLowerCase };

class CaseMappingHarfBuzzBufferFiller {
  STACK_ALLOCATED();

 public:
  CaseMappingHarfBuzzBufferFiller(CaseMapIntend,
                                  AtomicString locale,
                                  hb_buffer_t* harf_buzz_buffer,
                                  const UChar* buffer,
                                  unsigned buffer_length,
                                  unsigned start_index,
                                  unsigned num_characters);

 private:
  void FillSlowCase(CaseMapIntend,
                    AtomicString locale,
                    const UChar* buffer,
                    unsigned buffer_length,
                    unsigned start_index,
                    unsigned num_characters);
  hb_buffer_t* harf_buzz_buffer_;
};

}  // namespace blink

#endif
