/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * Model patterns(s) to recognize nops.
 */

#ifndef NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_NOP_PATTERNS_H__
#define NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_NOP_PATTERNS_H__

#include "native_client/src/shared/utils/types.h"

// Defines whether to generate warnings for each counted instruction.
extern Bool FLAGS_show_counted_instructions;

// Installs patterns to count kinds of instructions.
void InstallInstructionCounterPatterns();

#endif  // NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_NOP_PATTERNS_H__
