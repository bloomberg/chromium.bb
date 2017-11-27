// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_CYGPROFILE_LIGHTWEIGHT_CYGPROFILE_H_
#define TOOLS_CYGPROFILE_LIGHTWEIGHT_CYGPROFILE_H_

#include <cstdint>
#include <vector>

namespace base {
class FilePath;
}

namespace cygprofile {
constexpr size_t kStartOfTextForTesting = 1000;
constexpr size_t kEndOfTextForTesting = kStartOfTextForTesting + 1000 * 1000;

// Stop recording.
void Disable();

// CHECK()s that the offsets are correctly set up.
void SanityChecks();

// Stops recording, and dump the results to |path|.
void StopAndDumpToFile(const base::FilePath& path);

// Record an |address|, if recording is enabled. Only for testing.
void RecordAddressForTesting(size_t address);

// Resets the state. Only for testing.
void ResetForTesting();

// Returns an ordered list of reached offsets. Only for testing.
std::vector<size_t> GetOrderedOffsetsForTesting();
}  // namespace cygprofile

#endif  // TOOLS_CYGPROFILE_LIGHTWEIGHT_CYGPROFILE_H_
