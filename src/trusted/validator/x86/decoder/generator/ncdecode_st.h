/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/* ncdecode_st.h - Implements a (simple) hashtable used by the
 * instruction table generator.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DECODER_GENERATOR_NCDECODE_ST_H_
#define NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DECODER_GENERATOR_NCDECODE_ST_H_

#ifndef NACL_TRUSTED_BUT_NOT_TCB
#error("This file is not meant for use in the TCB")
#endif

#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/validator/x86/decoder/generator/ncdecode_forms.h"

struct NaClSymbolTable;
struct NaClStValue;
struct Gio;

/* The set of possible values that can be put in a symbol table. */
typedef enum {
  nacl_byte,       /* (unsigned) byte value. */
  nacl_text,       /* char* value. */
  nacl_int,        /* integer value. */
  nacl_defop,      /* a NaClDefOperand function pointer. */
} NaClStValueKind;

/* Returns a printable name for the given kind. */
const char* NaClStValueKindName(NaClStValueKind kind);

/* Models the set of possible values that can appear in a symbol table. */
typedef struct NaClStValue {
  NaClStValueKind kind;  /* descriminant of the kind of value. */
  union NaClStValueUnion {
    /* kind == nacl_byte */
    uint8_t byte_value;
    /* kind == nacl_int */
    int int_value;
    /* kind == nacl_text */
    const char* text_value;
    /* kind == nacl_defop */
    NaClDefOperand defop_value;
  } value;
} NaClStValue;

/* Copies the contents of the RHS to the LHS. Note: Does a shallow copy
 * only (I.e. only copies the int / char* / pointer of the union.
 */
void NaClStValueAssign(
    NaClStValue* lhs,
    NaClStValue* rhs);

/* Prints out the contents of a value to the given file. */
void NaClStValuePrint(struct Gio* g, NaClStValue* value);

/* Dynamically creates a symbol table, with the expected size,
 * and the given calling context (NULL implies top-level).
 * Must be destroyed with NaClSymbolTableDestroy.
 */
struct NaClSymbolTable* NaClSymbolTableCreate(
    size_t capacity,
    struct NaClSymbolTable* calling_context);

/* Adds the name value pair into the given symbol table. Value
 * must be non-NULL.
 */
void NaClSymbolTablePut(
    const char* name,
    struct NaClStValue* value,
    struct NaClSymbolTable* st);

void NaClSymbolTablePutByte(const char* name,
                            uint8_t byte,
                            struct NaClSymbolTable* st);

void NaClSymbolTablePutText(const char* name,
                            const char* value,
                            struct NaClSymbolTable* st);

void NaClSymbolTablePutInt(const char* name,
                           int value,
                           struct NaClSymbolTable* st);

/* Returns the value associated with the name, or NULL if
 * no such value exists (in the given symbol table, or any
 * of its calling contexts).
 */
struct NaClStValue* NaClSymbolTableGet(
    const char* name,
    struct NaClSymbolTable* st);

/* Print out the set of symbol bindings in a symbol table. */
void NaClSymbolTablePrint(struct Gio* g, struct NaClSymbolTable* st);

/* Destroys the given symbol table. */
void NaClSymbolTableDestroy(struct NaClSymbolTable* st);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_VALIDATOR_X86_DECODER_GENERATOR_NCDECODE_ST_H_ */
