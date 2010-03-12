/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/include/nacl_string.h"
#include "native_client/src/trusted/validator_arm/string_split.h"

// ----------------------------------------------------------------------
// SplitStringUsing()
//    Split a string using a character delimiter. Append the components
//    to 'result'.
//
// Note: For multi-character delimiters, this routine will split on *ANY* of
// the characters in the string, not the entire string as a single delimiter.
// ----------------------------------------------------------------------
template <typename StringType, typename ITR>
static inline
void SplitStringToIteratorUsing(const StringType& full,
                                const char* delim,
                                ITR& result) {
  // Optimize the common case where delim is a single character.
  if (delim[0] != '\0' && delim[1] == '\0') {
    char c = delim[0];
    const char* p = full.data();
    const char* end = p + full.size();
    while (p != end) {
      if (*p == c) {
        ++p;
      } else {
        const char* start = p;
        while (++p != end && *p != c);
        *result++ = StringType(start, p - start);
      }
    }
    return;
  }

  nacl::string::size_type begin_index, end_index;
  begin_index = full.find_first_not_of(delim);
  while (begin_index != nacl::string::npos) {
    end_index = full.find_first_of(delim, begin_index);
    if (end_index == nacl::string::npos) {
      *result++ = full.substr(begin_index);
      return;
    }
    *result++ = full.substr(begin_index, (end_index - begin_index));
    begin_index = full.find_first_not_of(delim, end_index);
  }
}

void SplitStringUsing(const nacl::string& full,
                      const char* delim,
                      std::vector<nacl::string>* result) {
  std::back_insert_iterator< std::vector<nacl::string> > it(*result);
  SplitStringToIteratorUsing(full, delim, it);
}
