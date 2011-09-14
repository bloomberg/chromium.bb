/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <stdio.h>

#include "native_client/src/trusted/validator/x86/ncinstbuffer.h"

/* Constant CLEAR_CACHE controls the behaviour of the buffer containing
 * the sequence of parsed bytes. Turn it on (1) to fill unused bytes with the
 * constant zero, and to allow access to all bytes in the sequence of parsed
 * bytes. Turn it off (0) to force access to only include the actual parsed
 * bytes.
 *
 * Note: Ideally, we would like to turn this feature off. However, the current
 * instruction parser (in ncdecode.c) and corresponding printer (in
 * ncdis_util.c) are problematic. The parser allows a partial match of
 * an instruction, without verifying that ALL necessary bytes are there. The
 * corresponding printer, assumes that only complete matches (during parsing)
 * were performed. The result is that the code sometimes assumes that many
 * more bytes were parsed than were actually parsed.
 *
 * To quickly fix the code so that it doesn't do illegal memory accesses, but
 * has consistent behaviour, the flag is currently sets CLEAR_CACHE to 1.
 *
 * To debug this problem, set the flag CLEAR_CACHE to 0.
 *
 * TODO(karl) Fix the parser/printer so that CLEAR_CACHE can be set to 0.
 */
#define CLEAR_CACHE 1

/* To turn on debugging of instruction decoding, change value of
 * DEBUGGING to 1.
 */
#define DEBUGGING 0

#include "native_client/src/shared/utils/debugging.h"

/* The constant to return if memory overflow occurs. */
# define NC_MEMORY_OVERFLOW 0

/* Returns the next byte in memory, or 0x00 if there are no more
 * bytes in memory.
 */
static uint8_t NCRemainingMemoryPeek(NCRemainingMemory* memory) {
  return (memory->cur_pos >= memory->mlimit)
      ? NC_MEMORY_OVERFLOW : *(memory->cur_pos);
}

void NCRemainingMemoryAdvance(NCRemainingMemory* memory) {
  memory->mpc = memory->cur_pos;
  memory->read_length = 0;
  memory->overflow_count = 0;
}

void NCRemainingMemoryReset(NCRemainingMemory* memory) {
  memory->cur_pos = memory->mpc;
  memory->next_byte = NCRemainingMemoryPeek(memory);
  memory->read_length = 0;
  memory->overflow_count = 0;
}

const char* NCRemainingMemoryErrorMessage(NCRemainingMemoryError error) {
  switch (error) {
    case NCRemainingMemoryOverflow:
      return "Read past end of memory segment occurred.";
    case NCInstBufferOverflow:
      return "Internal error: instruction buffer overflow.";
    case NCUnknownMemoryError:
    default:
      return "Unknown memory error occurred.";
  }
}

void NCRemainingMemoryReportError(NCRemainingMemoryError error,
                                  NCRemainingMemory* memory) {
  fprintf(stdout, "%s\n", NCRemainingMemoryErrorMessage(error));
}

void NCRemainingMemoryInit(uint8_t* memory_base, NaClMemorySize size,
                           NCRemainingMemory* memory) {
  memory->mpc = memory_base;
  memory->cur_pos = memory->mpc;
  memory->mlimit = memory_base + size;
  memory->next_byte = NCRemainingMemoryPeek(memory);
  memory->error_fn = NCRemainingMemoryReportError;
  memory->error_fn_state = NULL;
  NCRemainingMemoryAdvance(memory);
}

uint8_t NCRemainingMemoryLookahead(NCRemainingMemory* memory, ssize_t n) {
  if ((memory->cur_pos + n) < memory->mlimit) {
    return memory->cur_pos[n];
  } else {
    return NC_MEMORY_OVERFLOW;
  }
}

uint8_t NCRemainingMemoryRead(NCRemainingMemory* memory) {
  uint8_t byte = memory->next_byte;
  if (memory->cur_pos == memory->mlimit) {
    /* If reached, next_byte already set to 0 by last read. */
    if (0 == memory->overflow_count) {
      memory->error_fn(NCRemainingMemoryOverflow, memory);
    }
    memory->overflow_count++;
  } else {
    memory->read_length++;
    memory->cur_pos++;
    memory->next_byte = NCRemainingMemoryPeek(memory);
  }
  DEBUG(printf("memory read: %02x\n", byte));
  return byte;
}

void NCInstBytesInitMemory(NCInstBytes* bytes, NCRemainingMemory* memory) {
#if CLEAR_CACHE
  int i;
  for (i = 0; i < MAX_INST_LENGTH; ++i) {
    bytes->byte[i] = 0;
  }
#endif
  bytes->memory = memory;
  bytes->length = 0;
}

void NCInstBytesReset(NCInstBytes* buffer) {
#if CLEAR_CACHE
  int i;
  for (i = 0; i < MAX_INST_LENGTH; ++i) {
    buffer->byte[i] = 0;
  }
#endif
  NCRemainingMemoryReset(buffer->memory);
  buffer->length = 0;
}

void NCInstBytesInit(NCInstBytes* buffer) {
#if CLEAR_CACHE
  int i;
  for (i = 0; i < MAX_INST_LENGTH; ++i) {
    buffer->byte[i] = 0;
  }
#endif
  NCRemainingMemoryAdvance(buffer->memory);
  buffer->length = 0;
}

uint8_t NCInstBytesPeek(NCInstBytes* bytes, ssize_t n) {
  return NCRemainingMemoryLookahead(bytes->memory, n);
}

uint8_t NCInstByte(NCInstBytes* bytes, ssize_t n) {
  if (n < bytes->length)  {
    return bytes->byte[n];
  } else {
    return NCRemainingMemoryLookahead(bytes->memory, n - bytes->length);
  }
}

uint8_t NCInstBytesRead(NCInstBytes* bytes) {
  uint8_t byte = NCRemainingMemoryRead(bytes->memory);
  if (bytes->length < MAX_INST_LENGTH) {
    bytes->byte[bytes->length++] = byte;
  } else {
    bytes->memory->error_fn(NCInstBufferOverflow, bytes->memory);
  }
  return byte;
}

void NCInstBytesReadBytes(ssize_t n, NCInstBytes* bytes) {
  ssize_t i;
  for (i = 0; i < n; ++i) {
    NCInstBytesRead(bytes);
  }
}

#if CLEAR_CACHE
#define BYTES_LENGTH(bytes) MAX_INST_LENGTH
#else
#define BYTES_LENGTH(bytes) (bytes)->length
#endif

static void NCInstBytesPtrInitPos(NCInstBytesPtr* ptr, const NCInstBytes* bytes,
                        int pos) {
  ptr->bytes = bytes;
  if (pos <= BYTES_LENGTH(bytes)) {
    ptr->pos = (uint8_t) pos;
  } else {
    bytes->memory->error_fn(NCInstBufferOverflow, bytes->memory);
    ptr->pos = bytes->length;
  }
}

void NCInstBytesPtrInit(NCInstBytesPtr* ptr, const NCInstBytes* bytes) {
  NCInstBytesPtrInitPos(ptr, bytes, 0);
}

void NCInstBytesPtrInitInc(NCInstBytesPtr* ptr, const NCInstBytesPtr* base,
                           int pos) {
  NCInstBytesPtrInitPos(ptr, base->bytes, base->pos + pos);
}

uint8_t NCInstBytesPos(const NCInstBytesPtr* ptr) {
  return ptr->pos;
}

uint8_t NCInstBytesByte(const NCInstBytesPtr* ptr, int n) {
  int index = ptr->pos + n;
  if (index < BYTES_LENGTH(ptr->bytes)) {
    return ptr->bytes->byte[index];
  } else {
    ptr->bytes->memory->error_fn(NCInstBufferOverflow, ptr->bytes->memory);
    return 0;
  }
}

int32_t NCInstBytesInt32(const NCInstBytesPtr* ptr) {
  return (NCInstBytesByte(ptr, 0) +
          (NCInstBytesByte(ptr, 1) << 8) +
          (NCInstBytesByte(ptr, 2) << 16) +
          (NCInstBytesByte(ptr, 3) << 24));
}

void NCInstBytesAdvance(NCInstBytesPtr* ptr, int n) {
  int index = ptr->pos + n;
  if (index < BYTES_LENGTH(ptr->bytes)) {
    ptr->pos = index;
  } else {
    ptr->bytes->memory->error_fn(NCInstBufferOverflow, ptr->bytes->memory);
  }
}

int NCInstBytesLength(const NCInstBytesPtr* ptr) {
  return (int) ptr->bytes->length - (int) ptr->pos;
}
