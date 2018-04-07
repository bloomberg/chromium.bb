/*
* Copyright (C) 2012 Google Inc. All rights reserved.
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

#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_BYTE_ORDER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_BYTE_ORDER_H_

#include "build/build_config.h"

#if defined(OS_POSIX)
#include <arpa/inet.h>
#endif

#if defined(OS_WIN)

#include "third_party/blink/renderer/platform/wtf/byte_swap.h"

#if defined(ARCH_CPU_BIG_ENDIAN)
inline uint16_t ntohs(uint16_t x) {
  return x;
}
inline uint16_t htons(uint16_t x) {
  return x;
}
inline uint32_t ntohl(uint32_t x) {
  return x;
}
inline uint32_t htonl(uint32_t x) {
  return x;
}
#else
inline uint16_t ntohs(uint16_t x) {
  return WTF::Bswap16(x);
}
inline uint16_t htons(uint16_t x) {
  return WTF::Bswap16(x);
}
inline uint32_t ntohl(uint32_t x) {
  return WTF::Bswap32(x);
}
inline uint32_t htonl(uint32_t x) {
  return WTF::Bswap32(x);
}
#endif

#endif  // defined(OS_WIN)

#endif  // THIRD_PARTY_BLINK_RENDERER_PLATFORM_WTF_BYTE_ORDER_H_
