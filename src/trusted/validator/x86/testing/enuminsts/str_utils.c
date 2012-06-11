 /*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * str_utils.h
 *
 * Defines support string routines for the instruction enumerator.
 */
#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB.")
#endif

#include "native_client/src/trusted/validator/x86/testing/enuminsts/str_utils.h"

#include <ctype.h>
#include <string.h>

#include "native_client/src/trusted/validator/x86/testing/enuminsts/enuminsts.h"

/* If string s begins with string prefix, return a pointer to the
 * first byte after the prefix. Else return s.
 */
char *SkipPrefix(char *s, const char *prefix) {
  size_t plen = strlen(prefix);
  if (strncmp(s, prefix, plen) == 0) {
    return &s[plen + 1];
  }
  return s;
}

/* Return a pointer to s with leading spaces removed.
 */
const char *strip(const char *s) {
  while (*s == ' ') s++;
  return s;
}

/* Updates the string by removing trailing spaces/newlines. */
void rstrip(char *s) {
  size_t slen = strlen(s);
  while (slen > 0) {
    --slen;
    if (!isspace(s[slen])) return;
    s[slen] = '\0';
  }
}

/* Find substring ss in string s. Returns a pointer to the substring
 * on success, NULL on failure.
 */
const char *strfind(const char *s, const char *ss) {
  size_t i;
  size_t slen = strlen(s);
  size_t sslen = strlen(ss);

  for (i = 0; i < slen; i++) {
    if (s[i] == ss[0]) {
      if (strncmp(&s[i], ss, sslen) == 0) {
        return &s[i];
      }
    }
  }
  return NULL;
}

/* If string ss appears in string s, return a pointer to the first byte
 * after ss. Otherwise return NULL.
 */
const char *strskip(const char *s, const char *ss) {
  const char *tmp = strfind(s, ss);
  if (tmp != NULL) {
    return &tmp[strlen(ss)];
  }
  return NULL;
}

/* Remove all instances of character c in string s. */
void strnzapchar(char *s, const char c) {
  size_t i, nskip;
  const size_t slen = strlen(s);

  if (0 == c) return;
  nskip = 0;
  for (i = 0; i + nskip <= slen; i++) {
    while (s[i + nskip] == c) nskip += 1;
    s[i] = s[i + nskip];
  }
}

/* Remove all instances of substring ss in string s, modifying s in place.
 */
void CleanString(char *s, const char *ss) {
  size_t sslen = strlen(ss);
  char *fs = (char*) strfind(s, ss);

  while (fs != NULL) {
    do {
      fs[0] = fs[sslen];
      fs++;
    } while (*fs != '\0');
    fs = (char*) strfind(s, ss);
  }
}

/* Copies source to dest, like strncpy, but replaces the last
 * byte of the dest (based on buffer size) with a null character,
 * so that we are guaranteed to have a valid string.
 */
void cstrncpy(char *dest, const char *src, size_t n) {
  strncpy(dest, src, n);
  dest[n-1] = '0';
}

/* Copy src to dest, stoping at character c.
 */
void strncpyto(char *dest, const char *src, size_t n, char c) {
  size_t i;

  i = 0;
  while (1) {
    dest[i] = src[i];
    if (dest[i] == c) {
      dest[i] = 0;
      break;
    }
    if (dest[i] == '\0') break;
    i++;
    if (i == n) InternalError("really long opcode");
  }
}
