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
 * NaCl error codes.
 */

#ifndef SERVICE_RUNTIME_NACL_ERROR_CODE_H__
#define SERVICE_RUNTIME_NACL_ERROR_CODE_H__   1

#ifdef __cplusplus
extern "C" {
#endif

typedef enum NaClErrorCode {
  LOAD_OK,
  LOAD_STATUS_UNKNOWN,  /* load status not available yet */
  LOAD_UNSUPPORTED_OS_PLATFORM,
  LOAD_INTERNAL,
  LOAD_READ_ERROR,
  LOAD_BAD_ELF_MAGIC,
  LOAD_NOT_32_BIT,
  LOAD_BAD_ABI,
  LOAD_NOT_EXEC,
  LOAD_BAD_MACHINE,
  LOAD_BAD_ELF_VERS,
  LOAD_TOO_MANY_SECT,
  LOAD_BAD_SECT,
  LOAD_NO_MEMORY,
  LOAD_SECT_HDR,
  LOAD_ADDR_SPACE_TOO_SMALL,
  LOAD_ADDR_SPACE_TOO_BIG,
  LOAD_DATA_OVERLAPS_STACK_SECTION,
  LOAD_UNLOADABLE,
  LOAD_BAD_ELF_TEXT,
  LOAD_TEXT_SEG_TOO_BIG,
  LOAD_DATA_SEG_TOO_BIG,
  LOAD_MPROTECT_FAIL,
  LOAD_MADVISE_FAIL,
  LOAD_TOO_MANY_SYMBOL_STR,
  LOAD_SYMTAB_ENTRY_TOO_SMALL,
  LOAD_NO_SYMTAB,
  LOAD_NO_SYMTAB_STRINGS,
  LOAD_SYMTAB_ENTRY,
  LOAD_UNKNOWN_SYMBOL_TYPE,
  LOAD_SYMTAB_DUP,
  LOAD_REL_ERROR,
  LOAD_REL_UNIMPL,
  LOAD_UNDEF_SYMBOL,
  LOAD_BAD_SYMBOL_DATA,
  LOAD_BAD_FILE,
  LOAD_BAD_ENTRY,
  LOAD_SEGMENT_OUTSIDE_ADDRSPACE,
  LOAD_DUP_SEGMENT,
  LOAD_SEGMENT_BAD_LOC,
  LOAD_BAD_SEGMENT,
  LOAD_REQUIRED_SEG_MISSING,
  LOAD_SEGMENT_BAD_PARAM,
  LOAD_VALIDATION_FAILED,
  /*
   * service runtime errors (post load, during startup phase)
   */
  SRT_NO_SEG_SEL
} NaClErrorCode;
#define NACL_ERROR_CODE_MAX (SRT_NO_SEG_SEL+1)

char const  *NaClErrorString(NaClErrorCode  errcode);

/* deprecated */
char const  *NaClAppLoadErrorString(NaClErrorCode errcode);

#ifdef __cplusplus
}  /* end of extern "C" */
#endif

#endif
