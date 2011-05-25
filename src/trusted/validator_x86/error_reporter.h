/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVALIDATE_ERROR_H__
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVALIDATE_ERROR_H__

/*
 * Defines an API for reporting error messages by the validator. The
 * API is a struct of function pointers (i.e. virtuals) which will
 * be called by the validator to print out error messages.
 *
 * The typical error message is generated using the form:
 *
 * error_reporter->printf(format, ...)
 *
 * Note: Levels are assumed to be defined by the constants
 * LOG_INFO, LOG_WARNING, LOG_ERROR, and LOG_FATAL defined
 * by header file native_client/src/shared/platform/nacl_log.h"
 */

#include <stdarg.h>
#include "native_client/src/include/portability.h"

EXTERN_C_BEGIN

struct NaClErrorReporter;
struct NaClInstState;

/* Method called to start an error message. */
typedef void (*NaClStartError)(struct NaClErrorReporter* self);

/* Method called to end an error message. */
typedef void (*NaClEndError)(struct NaClErrorReporter* self);

/* Method to start the next line of a multi-line error message. Adds
 * any useful tagging information.
 */
typedef void (*NaClTagNewLine)(struct NaClErrorReporter* self);

/* Method to print out a formatted string. */
typedef void (*NaClPrintfMessage)(
    struct NaClErrorReporter* self, const char* format, ...);

/* Method to print out a formatted string. */
typedef void (*NaClPrintfVMessage)(struct NaClErrorReporter* self,
                                   const char* format,
                                   va_list ap);

/* Method to print out an instruction.
 * Note: inst is either of type NaClInstState* (sfi model), or
 * NCDecoderInst* (segment model).
 */
typedef void (*NaClPrintInst)(struct NaClErrorReporter* self,
                              struct NaClInstState* inst);

/* The virtual (base) class of virtual printing methods. */
typedef struct NaClErrorReporter {
  NaClPrintfMessage  printf;
  NaClPrintfVMessage printf_v;
  NaClPrintInst      print_inst;
} NaClErrorReporter;

EXTERN_C_END

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_NCVALIDATE_ERROR_H__ */
