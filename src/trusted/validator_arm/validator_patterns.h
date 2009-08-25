/*
 * Copyright 2009, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Model validation paterns. Used to validate Arm code meets
 * Native Client Rules.
 */

#ifndef NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_VALIDATOR_PATTERNS_H__
#define NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_VALIDATOR_PATTERNS_H__

#include <vector>
#include <string>
#include "native_client/src/trusted/validator_arm/ncdecode.h"

/*
 * Recognizes particular sequences of instructions and determines whether they
 * are safe.
 */
class ValidatorPattern {
 public:
  /*
   * Creates a pattern with the given name (used for reporting).
   *
   * The length is the length of the entire pattern, in instructions.
   *
   * The reporting_index is the index within the length where the pattern
   * may return true for MayBeUnsafe.  This is used to calculate the pattern's
   * critical region for CFG analysis.
   */
  ValidatorPattern(const std::string &name, int length, int reporting_index)
      : _name(name), _length(length), _reporting_index(reporting_index) {}

  virtual ~ValidatorPattern();

  /*
   * Checks whether this pattern applies at the current position.  Returns
   * true if the pattern is present and branches into this region must be
   * restricted; call IsUnsafe to determine whether the pattern is fully
   * satisfied.
   */
  virtual bool MayBeUnsafe(const NcDecodeState &state) = 0;

  /*
   * Returns true iff the instruction pointed to by 'state' is safe,
   * according to this particular pattern.
   *
   * Note: if the pattern doesn't apply to 'state' (as checked by calling
   * MayBeUnsafe above), the results are undefined.
   */
  virtual bool IsSafe(const NcDecodeState &state) = 0;

  // Prints out a validate error for this pattern when unsafe.
  // Defaults to printing out reporting instruction.
  virtual void ReportUnsafeError(const NcDecodeState &state);

  // Prints out a validate error for this pattern when the
  // current instruction does an unsafe branch into the pattern.
  virtual void ReportUnsafeBranchTo(const NcDecodeState &state,
      uint32_t address);

  // Prints out a validate error for this pattern when it crosses
  // block boundaries.
  virtual void ReportCrossesBlockBoundary(const NcDecodeState &state);

  /*
   * Returns the address of the first instruction in this pattern, in the
   * context of the given state.
   *
   * Note: if the pattern doesn't apply to 'state' (as checked by calling
   * MayBeUnsafe above), the results are undefined.
   */
  virtual uint32_t StartPc(const NcDecodeState &) const;

  /*
   * Returns the address just past the last instruction of this pattern,
   * context of the given state.
   *
   * Note: if the pattern doesn't apply to 'state' (as checked by calling
   * MayBeUnsafe above), the results are undefined.
   */
  virtual uint32_t EndPc(const NcDecodeState &) const;

  // Returns the name of the pattern.
  const std::string& GetName() {
    return _name;
  }

 protected:
  // Holds the name for the pattern.
  const std::string _name;
  // The overall length of this pattern, in instructions.
  const int _length;
  // The index within the sequence of _length instructions where MayBeUnsafe
  // will fire.
  const int _reporting_index;
};

// Register a validator pattern (patterns must be registered before validating
// the code).
void RegisterValidatorPattern(ValidatorPattern* pattern);

// Return an iterator over the set of patterns to validate.
std::vector<ValidatorPattern*>::iterator RegisteredValidatorPatternsBegin();

// Return an iterator over the set of patterns to validate.
std::vector<ValidatorPattern*>::iterator RegisteredValidatorPatternsEnd();

#endif  // NATIVE_CLIENT_PRIVATE_TOOLS_NCV_ARM_VALIDATOR_PATTERNS_H__
