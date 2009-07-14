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

// XRay -- a simple profiler for Native Client


#ifndef NATIVE_CLIENT_TOOLS_XRAY_XRAY_H_
#define NATIVE_CLIENT_TOOLS_XRAY_XRAY_H_

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif

#define XRAY_NO_INSTRUMENT  __attribute__((no_instrument_function))
#define XRAY_INLINE __attribute__((always_inline))

#if defined(XRAY)

/* do not call __XRayAnnotate* directly; instead use the */
/* XRayAnnotate() macros below. */
XRAY_NO_INSTRUMENT void __XRayAnnotate(const char *str, ...)
  __attribute__ ((format(printf, 1, 2)));
XRAY_NO_INSTRUMENT void __XRayAnnotateFiltered(const uint32_t filter,
  const char *str, ...) __attribute__ ((format(printf, 2, 3)));

/* == this is the beginning of the public XRay API == */
XRAY_NO_INSTRUMENT void XRayInit(int stack_size, int buffer_size,
                                 int frame_count, const char *mapfilename);
XRAY_NO_INSTRUMENT void XRayShutdown();
XRAY_NO_INSTRUMENT void XRayStartFrame();
XRAY_NO_INSTRUMENT void XRayEndFrame();
XRAY_NO_INSTRUMENT void XRaySetAnnotationFilter(uint32_t filter);
XRAY_NO_INSTRUMENT void XRaySaveReport(const char *filename, float cutoff);
#if defined(XRAY_ANNOTATE)
#define XRayAnnotate(...) __XRayAnnotate(__VA_ARGS__)
#define XRayAnnotateFiltered(...) __XRayAnnotateFiltered(__VA_ARGS__)
#else
#define XRayAnnotate(...)
#define XRayAnnotateFiltered(...)
#endif
/* == this is the end of the public XRay API == */

#else  /* defined(XRAY) */

/* builds that don't define XRAY will use these 'null' functions instead */

#define XRayAnnotate(...)
#define XRayAnnotateFiltered(...)

XRAY_INLINE void XRayInit(int stack_size, int buffer_size,
                          int frame_count, const char *mapfilename) { ; }
XRAY_INLINE void XRayShutdown() { ; }
XRAY_INLINE void XRayStartFrame() { ; }
XRAY_INLINE void XRayEndFrame() { ; }
XRAY_INLINE void XRaySetAnnotationFilter(uint32_t filter) { ; }
XRAY_INLINE void XRaySaveReport(const char *filename, float cutoff) { ; }

#endif  /* defined(XRAY) */

#ifdef __cplusplus
}
#endif

#endif  /* NATIVE_CLIENT_TOOLS_XRAY_XRAY_H_ */
