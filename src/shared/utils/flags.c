/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
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
