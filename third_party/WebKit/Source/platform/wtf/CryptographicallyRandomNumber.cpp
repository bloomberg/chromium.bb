/*
 * Copyright (c) 1996, David Mazieres <dm@uun.org>
 * Copyright (c) 2008, Damien Miller <djm@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include "platform/wtf/CryptographicallyRandomNumber.h"

#include "base/rand_util.h"
#include <string.h>

namespace WTF {

static bool g_should_use_always_zero_random_source_for_testing = false;

void SetAlwaysZeroRandomSourceForTesting() {
  g_should_use_always_zero_random_source_for_testing = true;
}

uint32_t CryptographicallyRandomNumber() {
  uint32_t result;
  CryptographicallyRandomValues(&result, sizeof(result));
  return result;
}

void CryptographicallyRandomValues(void* buffer, size_t length) {
  if (g_should_use_always_zero_random_source_for_testing) {
    memset(buffer, '\0', length);
    return;
  }

  // This should really be crypto::RandBytes(), but WTF can't depend on crypto.
  // The implementation of crypto::RandBytes() is just calling
  // base::RandBytes(), so both are actually same.
  base::RandBytes(buffer, length);
}

}  // namespace WTF
