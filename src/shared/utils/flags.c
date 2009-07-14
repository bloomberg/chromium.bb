/*
 * Copyright 2009, Google Inc.
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
 * Some useful routines to handle command line arguments.
 */

#include <stdlib.h>
#include <string.h>

#include "native_client/src/shared/utils/flags.h"

/*
 * Given a pointer to a flag name to be
 * checked against the pointer to the corresponding
 * argument, return true iff the argument begins with
 * the flag name. During the match, advance both pointers
 * to the maximal matched prefix.
 */
static Bool MatchFlagPrefix(const char** name,
                            const char** arg) {
  while (*(*name)) {
    if (*(*name) != *(*arg)) {
      return FALSE;
    }
    ++(*name);
    ++(*arg);
  }
  return TRUE;
}

/*
 * Given a pointer to a flag name to be checked
 * against the pointer to the corresponding argument, return true
 * iff the argument begins with the flag name, followed by
 * and equals sign. During the match, advance both pointers
 * to the maximal matched prefix.
 */
static Bool MatchFlagPrefixEquals(const char** name,
                                  const char** arg) {
  const char* equals = "=";
  return MatchFlagPrefix(name, arg) &&
      MatchFlagPrefix(&equals, arg);
}

Bool GrokBoolFlag(const char* name,
                  const char* arg,
                  Bool* flag) {
  if (strcmp(name, arg) == 0) {
    *flag = TRUE;
    return TRUE;
  } else if (MatchFlagPrefixEquals(&name, &arg)) {
    if (strcmp(arg, "true") == 0) {
      *flag = TRUE;
      return TRUE;
    } else if (strcmp(arg, "false") == 0) {
      *flag = FALSE;
      return TRUE;
    }
  }
  return FALSE;
}

static Bool IsZero(const char* arg) {
  while (*arg) {
    if ('0' != *arg) {
      return FALSE;
    }
    ++arg;
  }
  return TRUE;
}

Bool GrokIntFlag(const char* name,
                 const char* arg,
                 int* flag) {
  if (MatchFlagPrefixEquals(&name, &arg)) {
    int value = atoi(arg);
    /* verify that arg is all zeros. Otherwise,
     * assume that 0 was returned due to an error.
     */
    if (0 == value && !IsZero(arg)) return FALSE;
    *flag = value;
    return TRUE;
  }
  return FALSE;
}

Bool GrokUint32HexFlag(const char* name,
                       const char* arg,
                       uint32_t* flag) {
  if (MatchFlagPrefixEquals(&name, &arg)) {
    unsigned long value = strtoul(arg, NULL, 16);
    /* Verify that arg is all zeros. Otherwise,
     * assume that 0 was returned due to an error.
     */
    if (0L == value && !IsZero(arg)) return FALSE;
    *flag = (uint32_t) value;
    return TRUE;
  }
  return FALSE;
}


Bool GrokCstringFlag(const char* name,
                     const char* arg,
                     char** flag) {
  if (MatchFlagPrefixEquals(&name, &arg)) {
    *flag = (char*) arg;
    return TRUE;
  }
  return FALSE;
}
