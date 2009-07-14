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
// This header file is the private internal interface

#ifndef NATIVE_CLIENT_TOOLS_XRAY_XRAY_PRIV_H_
#define NATIVE_CLIENT_TOOLS_XRAY_XRAY_PRIV_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "native_client/tools/xray/xray.h"

#ifdef __cplusplus
extern "C" {
#endif

#if defined(XRAY)

#define XRAY_FORCE_INLINE __attribute__((always_inline))

#define XRAY_TRACE_STACK_SIZE (256)
#define XRAY_TRACE_ANNOTATION_LENGTH (1024)
#define XRAY_TRACE_BUFFER_SIZE (1048576)
#define XRAY_ANNOTATION_STACK_SIZE ((XRAY_TRACE_STACK_SIZE) * \
                                    (XRAY_TRACE_ANNOTATION_LENGTH))
#define XRAY_STRING_POOL_NODE_SIZE (32768)
#define XRAY_FRAME_MARKER (0xFFFFFFFF)
#define XRAY_NULL_ANNOTATION (0x0)
#define XRAY_FUNCTION_ALIGNMENT_BITS (4)
#define XRAY_ADDR_MASK (0xFFFFFF00)
#define XRAY_ADDR_SHIFT (4)
#define XRAY_DEPTH_MASK (0x000000FF)
#define XRAY_SYMBOL_TABLE_MAX_RATIO (0.66f)
#define XRAY_LINE_SIZE (1024)
#define XRAY_MAX_FRAMES (60)
#define XRAY_MAX_LABEL (64)
#define XRAY_DEFAULT_SYMBOL_TABLE_SIZE (4096)
#define XRAY_SYMBOL_POOL_NODE_SIZE (1024)
#define XRAY_GUARD_VALUE (0x12345678)
#define XRAY_EXTRACT_ADDR(x) (((x) & XRAY_ADDR_MASK) >> XRAY_ADDR_SHIFT)
#define XRAY_EXTRACT_DEPTH(x) ((x) & XRAY_DEPTH_MASK)
#define XRAY_PACK_ADDR(x) (((x) << XRAY_ADDR_SHIFT) & XRAY_ADDR_MASK)
#define XRAY_PACK_DEPTH(x) ((x) & XRAY_DEPTH_MASK)
#define XRAY_PACK_DEPTH_ADDR(d, a) (XRAY_PACK_DEPTH(d) | XRAY_PACK_ADDR(a))
#define XRAY_ALIGN64 __attribute((aligned(64)))

struct XRayStringPool;
struct XRayHashTable;
struct XRaySymbolPool;
struct XRaySymbol;
struct XRaySymbolTable;

/* important - don't instrument xray itself, so use       */
/*             XRAY_NO_INSTRUMENT on all xray functions   */

XRAY_NO_INSTRUMENT char* XRayStringPoolAppend(struct XRayStringPool *pool,
  const char *src);
XRAY_NO_INSTRUMENT struct XRayStringPool* XRayStringPoolCreate();
XRAY_NO_INSTRUMENT void XRayStringPoolFree(struct XRayStringPool *pool);

XRAY_NO_INSTRUMENT void* XRayHashTableLookup(struct XRayHashTable *table,
    uint32_t addr);
XRAY_NO_INSTRUMENT void* XRayHashTableInsert(struct XRayHashTable *table,
    void *data, uint32_t addr);
XRAY_NO_INSTRUMENT void* XRayHashTableAtIndex(
  struct XRayHashTable *table, int i);
XRAY_NO_INSTRUMENT int XRayHashTableGetSize(struct XRayHashTable *table);
XRAY_NO_INSTRUMENT struct XRayHashTable* XRayHashTableCreate(int size);
XRAY_NO_INSTRUMENT void XRayHashTableFree(struct XRayHashTable *table);
XRAY_NO_INSTRUMENT void XRayHashTableHisto(FILE *f);

XRAY_NO_INSTRUMENT struct XRaySymbol* XRaySymbolPoolAlloc(
    struct XRaySymbolPool *sympool);
XRAY_NO_INSTRUMENT struct XRaySymbolPool* XRaySymbolPoolCreate();
XRAY_NO_INSTRUMENT void XRaySymbolPoolFree(struct XRaySymbolPool *sympool);

XRAY_NO_INSTRUMENT const char* XRaySymbolGetName(struct XRaySymbol *symbol);
XRAY_NO_INSTRUMENT struct XRaySymbol* XRaySymbolCreate(
    struct XRaySymbolPool *sympool, const char *name);
XRAY_NO_INSTRUMENT void XRaySymbolFree(struct XRaySymbol *symbol);
XRAY_NO_INSTRUMENT int XRaySymbolCount(struct XRaySymbolTable *symtab);

XRAY_NO_INSTRUMENT void XRaySymbolTableParseMapfile(
    struct XRaySymbolTable *symbols, const char *mapfilename);
XRAY_NO_INSTRUMENT int XRaySymbolTableGetSize(struct XRaySymbolTable *symtab);
XRAY_NO_INSTRUMENT struct XRaySymbol* XRaySymbolTableLookup(
    struct XRaySymbolTable *symbols, uint32_t addr);
XRAY_NO_INSTRUMENT struct XRaySymbol* XRaySymbolTableAtIndex(
    struct XRaySymbolTable *symbols, int i);
XRAY_NO_INSTRUMENT struct XRaySymbolTable* XRaySymbolTableCreate(int size);
XRAY_NO_INSTRUMENT void XRaySymbolTableFree(struct XRaySymbolTable *symbtab);

XRAY_NO_INSTRUMENT void* XRayMalloc(size_t t);
XRAY_NO_INSTRUMENT void XRayFree(void *data);
XRAY_NO_INSTRUMENT void XRaySetMaxStackDepth(int stack_depth);
XRAY_NO_INSTRUMENT int XRayGetLastFrame();
XRAY_NO_INSTRUMENT void XRayDisableCapture();
XRAY_NO_INSTRUMENT void XRayEnableCapture();
XRAY_NO_INSTRUMENT void XRayLoadMapfile(const char *mapfilename);

#endif  /* defined(XRAY) */

#ifdef __cplusplus
}
#endif

#endif  /* NATIVE_CLIENT_TOOLS_XRAY_XRAY_PRIV_H_ */
