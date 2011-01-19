
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

// Read arguments from the tokens array into an srpc arg vector.
// Returns true iff successful.
// Note: this functions allocates memory which should be freed using
// FreeArrayArgs
bool ParseArgs(NaClSrpcArg** arg,
               const vector<string>& tokens,
               size_t start,
               bool input,
               NaClCommandLoop* ncl);

// Compare two arg vector for value equality
bool AllArgsEqual(NaClSrpcArg** arg1, NaClSrpcArg** arg2);

// Dump set of args to stdout.
void DumpArgs(const NaClSrpcArg* const* args, NaClCommandLoop *ncl);

// Initialize the array of NaClSrpcArg pointers from an array of NaClSrpcArg.
void BuildArgVec(NaClSrpcArg* argv[], NaClSrpcArg arg[], size_t count);

// Free memory allocated by ParseArgs.
void FreeArrayArgs(NaClSrpcArg** args);
#endif  /* NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERSAL_PARSING_H_ */
