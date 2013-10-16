// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_GN_ESCAPE_H_
#define TOOLS_GN_ESCAPE_H_

#include <iosfwd>

#include "base/strings/string_piece.h"

enum EscapingMode {
  // No escaping.
  ESCAPE_NONE,

  // Ninja string escaping.
  ESCAPE_NINJA = 1,

  // Shell string escaping.
  ESCAPE_SHELL = 2,

  // For writing shell commands to ninja files.
  ESCAPE_NINJA_SHELL = ESCAPE_NINJA | ESCAPE_SHELL,

  ESCAPE_JSON = 4,
};

struct EscapeOptions {
  EscapeOptions()
      : mode(ESCAPE_NONE),
        convert_slashes(false),
        inhibit_quoting(false) {
  }

  EscapingMode mode;

  // When set, converts forward-slashes to system-specific path separators.
  bool convert_slashes;

  // When the escaping mode is ESCAPE_SHELL, the escaper will normally put
  // quotes around things with spaces. If this value is set to true, we'll
  // disable the quoting feature and just add the spaces.
  //
  // This mode is for when quoting is done at some higher-level. Defaults to
  // false.
  bool inhibit_quoting;
};

// Escapes the given input, returnining the result.
//
// If needed_quoting is non-null, whether the string was or should have been
// (if inhibit_quoting was set) quoted will be written to it. This value should
// be initialized to false by the caller and will be written to only if it's
// true (the common use-case is for chaining calls).
std::string EscapeString(const base::StringPiece& str,
                         const EscapeOptions& options,
                         bool* needed_quoting);

// Same as EscapeString but writes the results to the given stream, saving a
// copy.
void EscapeStringToStream(std::ostream& out,
                          const base::StringPiece& str,
                          const EscapeOptions& options);

#endif  // TOOLS_GN_ESCAPE_H_
