/*
 * Copyright 2008, Google Inc.
 * All rights reserved.
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
/*
 * cdomshm.h - some helper stuff for sharing C-generated HTML with JavaScript
 *
 * These structs are used to manage a shared memory buffer as a big text
 * buffer that grows through append operations.
 */
#ifndef NATIVE_CLIENT_TESTS_CDOM_PERF_CDOMSHM_H_
#define NATIVE_CLIENT_TESTS_CDOM_PERF_CDOMSHM_H_

typedef struct big_buffer {
  char *buf;        /* pointer to the shared memory region */
  size_t index;     /* next byte to write */
  size_t max;       /* end of buffer/shared memory region */
  int failed;       /* failure (e.g. attempt to write past end) */
} BigBuffer;

/* Helper variable for NaCl shared memory link */
extern BigBuffer g_BigBuffer;

/* BBWrite() appends text to end of the BigBuffer. bb will commonly be
 * a pointer to g_BigBuffer.
 */
void BBWrite(const char *s, BigBuffer *bb);

#endif
