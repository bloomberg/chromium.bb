/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * ncdecode_st.c - Implements nested symbol tables.
 *
 * Note: We use linear lists
 */

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/trusted/validator/x86/decoder/generator/ncdecode_st.h"

#include "native_client/src/include/nacl_macros.h"
#include "native_client/src/include/portability.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/trusted/validator/x86/decoder/generator/ncdecode_tablegen.h"

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

const char* NaClStValueKindName(NaClStValueKind kind) {
  static const char* name[] = {
    "nacl_byte",
    "nacl_text",
    "nacl_int",
    "nacl_defop",
  };
  return kind < NACL_ARRAY_SIZE(name) ? name[kind] : "???";
}

void NaClStValueAssign(
    NaClStValue* lhs,
    NaClStValue* rhs) {
  memcpy(lhs, rhs, sizeof(NaClStValue));
}

void NaClStValuePrint(struct Gio* g, NaClStValue* value) {
  gprintf(g, "<%s : ", NaClStValueKindName(value->kind));
  switch (value->kind) {
    case nacl_byte:
      gprintf(g, "0x%02"NACL_PRIx8, value->value.byte_value);
      break;
    case nacl_int:
      gprintf(g, "%d", value->value.int_value);
      break;
    case nacl_text:
      gprintf(g, "'%s'", value->value.text_value);
      break;
    case nacl_defop:
      gprintf(g, "<defop>");
      break;
    default:
      gprintf(g, "???");
      break;
  }
  gprintf(g, ">");
}

/* Element in the symbol table. */
typedef struct NaClSymbolTablePair {
  const char* name;          /* The name of the symbol. */
  NaClStValue value;   /* The value associated with the symbol. */
} NaClSymbolTablePair;

/* Simple implementation of a symbol table using an array of size capacity. */
typedef struct NaClSymbolTable {
  /* The calling context defines the outer scope for processing
   * nested scopes.
   */
  struct NaClSymbolTable* calling_context;
  /* The current size of the symbol table. */
  size_t size;
  /* The maximum size of the symbol table. */
  size_t capacity;
  /* The array holding the contents of the symbol table. */
  NaClSymbolTablePair* values;
} NaClSymbolTable;

NaClSymbolTable* NaClSymbolTableCreate(
    size_t capacity,
    NaClSymbolTable* calling_context) {
  NaClSymbolTable* st = (NaClSymbolTable*) malloc(sizeof(NaClSymbolTable));
  assert(NULL != st);
  st->calling_context = calling_context;
  st->size = 0;
  st->capacity = capacity;
  st->values = (NaClSymbolTablePair*)
      calloc(capacity, sizeof(NaClSymbolTablePair));
  assert(NULL != st->values);
  DEBUG(NaClLog(LOG_INFO,
                "NaClSymbolTableCreate(%"NACL_PRIdS") = %p\n",
                capacity, (void*) st));
  return st;
}

void NaClSymbolTableDestroy(NaClSymbolTable* st) {
  DEBUG(NaClLog(LOG_INFO, "NaClSymbolTableDestroy(%p)\n", (void*) st));
  free(st);
}

void NaClSymbolTablePut(
    const char* name,
    struct NaClStValue* value,
    NaClSymbolTable* st) {
  size_t i;
  DEBUG(NaClLog(LOG_INFO,
                "NaClSymbolTablePut('%s', ", name);
        NaClStValuePrint(NaClLogGetGio(), value);
        gprintf(NaClLogGetGio(), ", %p)\n", (void*) st));
  /* First see if already in the symbol table. */
  for (i = 0; i < st->size; ++i) {
    if (0 == strcmp(name, st->values[i].name)) {
      NaClStValueAssign(&(st->values[i].value), value);
      return;
    }
  }
  /* If reached, not in symbol table, add. */
  assert(st->size < st->capacity);
  st->values[st->size].name = strdup(name);
  assert(NULL != st->values[st->size].name);
  NaClStValueAssign(&(st->values[st->size++].value), value);
}

void NaClSymbolTablePutByte(const char* name,
                            uint8_t byte,
                            struct NaClSymbolTable* st) {
  NaClStValue value;
  value.kind = nacl_byte;
  value.value.byte_value = byte;
  NaClSymbolTablePut(name, &value, st);
}

void NaClSymbolTablePutText(const char* name,
                            const char* text,
                            struct NaClSymbolTable* st) {
  NaClStValue value;
  value.kind = nacl_text;
  value.value.text_value = text;
  NaClSymbolTablePut(name, &value, st);
}

void NaClSymbolTablePutInt(const char* name,
                           int ival,
                           struct NaClSymbolTable* st) {
  NaClStValue value;
  value.kind = nacl_int;
  value.value.int_value = ival;
  NaClSymbolTablePut(name, &value, st);
}

struct NaClStValue* NaClSymbolTableGet(
    const char* name,
    struct NaClSymbolTable* st) {
  size_t i;
  NaClStValue* result;
  DEBUG(NaClLog(LOG_INFO,
                "-> NaClSymbolTableGet('%s', %p):\n", name, (void*) st));
  while (NULL != st) {
    /* First see if already in the symbol table. */
    for (i = 0; i < st->size; ++i) {
      if (0 == strcmp(name, st->values[i].name)) {
        result = &(st->values[i].value);
        DEBUG(NaClLog(LOG_INFO, "<- NaClSymbolTableGet = ");
              NaClStValuePrint(NaClLogGetGio(), result);
              gprintf(NaClLogGetGio(), ")\n"));
        return result;
      }
    }
    /* If reached, not in this symbol table. Try calling context. */
    st = st->calling_context;
  }
  /* If reached, not defined in any calling context. */
  DEBUG(NaClLog(LOG_INFO, "<- NaClSymbolTableGet = NULL\n"));
  return NULL;
}
