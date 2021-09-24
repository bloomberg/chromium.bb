// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_PROFILER_CHROME_UNWIND_TABLE_ANDROID_H_
#define BASE_PROFILER_CHROME_UNWIND_TABLE_ANDROID_H_

#include <stdint.h>

#include "base/base_export.h"
#include "base/profiler/register_context.h"

namespace base {

enum UnwindInstructionResult {
  COMPLETED,                    // Signals the end of unwind process.
  INSTRUCTION_PENDING,          // Continues to unwind next instruction.
  STACK_POINTER_OUT_OF_BOUNDS,  // Stack pointer is out of bounds after
                                // execution of unwind instruction.
};

// Execute a single unwind instruction on the given thread_context,
// and moves `instruction` to point to next instruction right after the executed
// instruction if the executed result is `INSTRUCTION_PENDING`.
//
// See Exception handling ABI for the ARM architecture ABI, §9.3.
// https://developer.arm.com/documentation/ihi0038/b.
// for details in unwind instruction encoding.
// Only following instruction encodings are handled:
// - 00xxxxxx
// - 01xxxxxx
// - 1001nnnn
// - 10100nnn
// - 10101nnn
// - 10110000
// - 10110010 uleb128
//
// Unwind instruction table is expected to have following memory layout:
// +----------------+
// | <--1 byte--->  |
// +----------------+
// | INST_PENDING   | <- FUNC1 offset 10
// +----------------+
// | INST_PENDING   | <- FUNC1 offset 4
// +----------------+
// | COMPLETE       | <- FUNC1 offset 0
// +----------------+
// | INST_PENDING   | <- FUNC2 offset 8
// +----------------+
// | ...            |
// +----------------+
// Because we are unwinding the function, the next unwind instruction to
// execute always have smaller function offset.
// The function offsets are often discontinuous as not all instructions in
// the function have corresponding unwind instructions.
BASE_EXPORT UnwindInstructionResult
ExecuteUnwindInstruction(const uint8_t*& instruction,
                         RegisterContext* thread_context);

// Given
//  - function_offset_table_byte_index
//  - instruction_offset_from_function_start
// finds the instruction to execute on unwind instruction table.
//
// Function offset table is expected to have following memory layout:
// +---------------------+---------------------+
// | <-----ULEB128-----> | <-----ULEB128-----> |
// +---------------------+---------------------+
// | Offset              | Unwind Index        |
// +---------------------+---------------------+-----
// | 8                   | XXX                 |  |
// +---------------------+---------------------+  |
// | 3                   | YYY                 |Function 1
// +---------------------+---------------------+  |
// | 0                   | ZZZ                 |  |
// +---------------------+---------------------+-----
// | 5                   | AAA                 |  |
// +---------------------+---------------------+Function 2
// | 0                   | BBB                 |  |
// +---------------------+---------------------+-----
// | ...                 | ....                |
// +---------------------+---------------------+
//
// The function offset table contains [offset, unwind index] pairs, where
// - offset: offset from function start address of an instruction that affects
//           the unwind state, measured in two-byte instructions.
// - unwind index: unwind instruction location on unwind instruction table.
//
// Note:
// - Each function always ends at 0 offset, which correspond to a COMPLETE
//   instruction on unwind instruction table.
// - Within each function section, offset strictly decreases. By doing so,
//   each function's own COMPLETE instruction will serve as termination
//   condition when searching in the table.
//
// Arguments:
//  unwind_instruction_table: The table that stores a list of unwind
//                             instructions
//  function_offset_table: Explained above.
//  function_offset_table_byte_index: The byte index of the first offset for the
//                                    function in the function offset table.
//  instruction_offset_from_function_start: (pc - function_start_address) >> 1.
BASE_EXPORT const uint8_t* GetFirstUnwindInstructionFromInstructionOffset(
    const uint8_t* unwind_instruction_table,
    const uint8_t* function_offset_table,
    uint16_t function_offset_table_byte_index,
    uint32_t instruction_offset_from_function_start);
}  // namespace base

#endif  // BASE_PROFILER_CHROME_UNWIND_TABLE_ANDROID_H_
