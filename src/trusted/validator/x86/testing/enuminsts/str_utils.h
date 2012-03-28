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

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_TESTING_STR_UTILS_H__
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_TESTING_STR_UTILS_H__

#include "native_client/src/include/nacl_macros.h"

/* If string s begins with string prefix, return a pointer to the
 * first byte after the prefix. Else return s.
 */
char *SkipPrefix(char *s, const char *prefix);

/* Return a pointer to s with leading spaces removed. */
const char *strip(const char *s);

/* Updates the string by removing trailing spaces/newlines. */
void rstrip(char *s);

/* Find substring ss in string s. Returns a pointer to the substring
 * on success, NULL on failure.
 */
const char *strfind(const char *s, const char *ss);

/* If string ss appears in string s, return a pointer to the first byte
 * after ss. Otherwise return NULL.
 */
const char *strskip(const char *s, const char *ss);

/* Copies source to dest, like strncpy, but replaces the last
 * byte of the dest (based on buffer size) with a null character,
 * so that we are guaranteed to have a valid string.
 */
void cstrncpy(char *dest, const char *src, size_t n);

/* Copy src to dest, stoping at character c. */
void strncpyto(char *dest, const char *src, size_t n, char c);

/* Remove all instances of substring ss in string s, modifying s in place. */
void CleanString(char *s, const char *ss);

/* Remove all instances of character c in string s. */
void strnzapchar(char *s, const char c);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_TESTING_STR_UTILS_H__ */
