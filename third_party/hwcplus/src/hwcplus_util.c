// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include <hardware/hardware.h>
#include <cutils/properties.h>

#define LOG_BUF_SIZE 1024

static int default_log_fn(int prio, const char* tag, const char* msg);

static hwcplus_log_fn_t hwcplus_log_fn = default_log_fn;

void hwcplus_set_log_fn(hwcplus_log_fn_t fn) {
    hwcplus_log_fn = fn;
}

#ifndef HAVE_STRLCPY
size_t strlcpy(char* dst, const char* src, size_t siz) {
    char* d = dst;
    const char* s = src;
    size_t n = siz;

    /* Copy as many bytes as will fit */
    if (n != 0) {
        while (--n != 0) {
            if ((*d++ = *s++) == '\0')
                break;
        }
    }

    /* Not enough room in dst, add NUL and traverse rest of src */
    if (n == 0) {
        if (siz != 0)
            *d = '\0'; /* NUL-terminate dst */
        while (*s++) {
        }
    }

    return(s - src - 1); /* count does not include NUL */
}
#endif

static int default_log_fn(int prio, const char* tag, const char* msg) {
    fprintf(stderr, "<%d> %s %s\n", prio, tag, msg);
}

int __android_log_write(int prio, const char* tag, const char* msg) {
    hwcplus_log_fn(prio, tag, msg);
}

int __android_log_print(int prio, const char* tag, const char* fmt, ...) {
    va_list ap;
    char buf[LOG_BUF_SIZE];

    va_start(ap, fmt);
    vsnprintf(buf, LOG_BUF_SIZE, fmt, ap);
    va_end(ap);

    return __android_log_write(prio, tag, buf);
}

int property_get(const char* key, char* value, const char* default_value) {
    printf("property_get %s\n", key);
    const char* r = default_value;
    if (!r)
        r = "";
    strncpy(value, r, PROPERTY_VALUE_MAX);
    return strlen(r);
}

