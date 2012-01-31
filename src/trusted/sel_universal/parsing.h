
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

struct NaClDesc;
struct NaClSrpcArg;
class NaClCommandLoop;

// Parse a line of input data into tokens.
// This pretty much breaks tokens at whitespace except for some
// built-in knowledge of quotes for string.
// Also it does know about escape sequences but does not rewrite them.
void Tokenize(string line, vector<string>* tokens);

// Read argument from a string into an srpc arg
// Returns true iff successful.
// Note: this functions allocates memory which should be freed using
// FreeArrayArg

bool ParseArg(NaClSrpcArg* arg,
              string token,
              bool input,
              NaClCommandLoop* ncl);
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

// Get arg as string
string DumpArg(const NaClSrpcArg* arg, NaClCommandLoop* ncl);

// Initialize the array of NaClSrpcArg pointers from an array of NaClSrpcArg.
void BuildArgVec(NaClSrpcArg* argv[], NaClSrpcArg arg[], size_t count);

// Free memory allocated by ParseArg.
void FreeArrayArg(NaClSrpcArg* arg);

// Free memory allocated by ParseArgs.
void FreeArrayArgs(NaClSrpcArg** args);

// Substitute variables in string s.
string SubstituteVars(string s, NaClCommandLoop* ncl);


// remove typing markup, e.g. i(1234) -> 1234
string GetPayload(string token);

// convert strings 5 or i(5) to int32
int32_t ExtractInt32(string token);

// convert strings 5 or l(5) to int64
int64_t ExtractInt64(string token);

NaClDesc* ExtractDesc(string token, NaClCommandLoop* ncl);
#endif  /* NATIVE_CLIENT_SRC_TRUSTED_SEL_UNIVERSAL_PARSING_H_ */
