
/*
 * Copyright (c) 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#ifndef NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERSAL_PARSING_H_
#define NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERSAL_PARSING_H_

#include <string>
#include <vector>

using std::string;
using std::vector;

struct NaClSrpcArg;
class NaClCommandLoop;

// Parse a line of input data into tokens.
// This pretty much breaks tokens at whitespace except for some
// built-in knowledge of quotes for string.
// Also it does know about escape sequences but does not rewrite them.
void Tokenize(string line, vector<string>* tokens);

// Create an argument from a token string.  Returns true iff successful
bool ParseArg(NaClSrpcArg* arg, string token, bool input, NaClCommandLoop* ncl);
// Dump an argument do stdout
void DumpArg(const NaClSrpcArg* arg, NaClCommandLoop *ncl);

#endif  /* NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERSAL_PARSING_H_ */
