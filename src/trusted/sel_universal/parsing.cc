/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// This module contains parsing code to be used with sel_universal.
// Since sel_universal is a testing tool and parsing in C++ is no fun,
// this code merely aims at being maitainable.
// Efficiency and proper dealing with bad input are non-goals.

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "native_client/src/include/build_config.h"

#if NACL_WINDOWS
#include <float.h>
#define NACL_ISNAN(d) _isnan(d)
#else
/* Windows doesn't have the following header files. */
# include <math.h>
#define NACL_ISNAN(d) isnan(d)
#endif  /* NACL_WINDOWS */

#include <iomanip>
#include <string>
#include <vector>
#include <sstream>

#include "native_client/src/include/portability_string.h"

#include "native_client/src/trusted/sel_universal/rpc_universal.h"
#include "native_client/src/trusted/sel_universal/parsing.h"
#include "native_client/src/shared/platform/nacl_log.h"

using std::stringstream;


static uint32_t StringToUint32(string s, size_t pos = 0) {
  return strtoul(s.c_str() + pos, 0, 0);
}


static int32_t StringToInt32(string s, size_t pos = 0) {
  return strtol(s.c_str() + pos, 0, 0);
}


static int64_t StringToInt64(string s, size_t pos = 0) {
  return STRTOLL(s.c_str() + pos, 0, 0);
}


static double StringToDouble(string s, size_t pos = 0) {
  return strtod(s.c_str() + pos, 0);
}

// remove typing markup, e.g. i(1234) -> 1234
string GetPayload(string token) {
  return token.substr(2, token.size() - 3);
}

// convert strings 5 or i(5) to int32
int32_t ExtractInt32(string token) {
  if (token[0] == 'i' && token[1] == '(') {
    return StringToInt32(token, 2);
  }
  return StringToInt32(token);
}

// convert strings 5 or l(5) to int64
int64_t ExtractInt64(string token) {
  if (token[0] == 'l' && token[1] == '(') {
    return StringToInt64(token, 2);
  }
  return StringToInt64(token);
}


NaClDesc* ExtractDesc(string token, NaClCommandLoop* ncl) {
  if (token[0] == 'h' && token[1] == '(') {
    token = GetPayload(token);
  }
  return ncl->FindDescByName(token);
}


static int HandleEscapedOctal(const string s, size_t* pos) {
  if (*pos + 2 >= s.size()) {
    NaClLog(LOG_ERROR, "malformed octal escape");
    return -1;
  }

  int ival =  s[*pos] - '0';
  ++*pos;
  ival = ival * 8 + s[*pos] - '0';
  ++*pos;
  ival = ival * 8 + s[*pos] - '0';
  ++*pos;
  return ival;
}


static int HexDigit(char c) {
  if (isdigit(c)) return c - '0';
  else return toupper(c) - 'A' + 10;
}


static int HandleEscapedHex(const string s, size_t* pos) {
  if (*pos + 1 >= s.size()) {
    NaClLog(LOG_ERROR, "malformed hex escape");
    return -1;
  }
  int ival = HexDigit(s[*pos]);
  ++*pos;
  ival = ival * 16 + HexDigit(s[*pos]);
  ++*pos;
  return ival;
}


// This should be kept in sync with the SRPC logging escaping done in
// src/shared/srpc/rpc_log.c to allow easy capture of logs for testing.
static int HandleEscapedChar(const string s, size_t* pos) {
  if (*pos >= s.size()) return -1;
  switch (s[*pos]) {
    case '\\':
      ++*pos;
      return '\\';
    case '\"':
      ++*pos;
      return '\"';
    case '\'':
      ++*pos;
      return '\'';
    case 'a':
      ++*pos;
      return '\a';
    case 'b':
      ++*pos;
      return '\b';
    case 'f':
      ++*pos;
      return '\f';
    case 'n':
      ++*pos;
      return '\n';
    case 'r':
      ++*pos;
      return '\r';
    case 't':
      ++*pos;
      return '\t';
    case 'v':
      ++*pos;
      return '\v';

    case '0':
    case '1':
    case '2':
    case '3':
    case '4':
    case '5':
    case '6':
    case '7':
      return HandleEscapedOctal(s, pos);

    case 'x':
    case 'X':
      ++*pos;
      return HandleEscapedHex(s, pos);

    default:
      NaClLog(LOG_ERROR, "bad escape\n");
      return -1;
  }
}


// Reads one char from *p and advances *p to the next read point in the input.
// This handles some meta characters ('\\', '\"', '\b', '\f', '\n', '\t',
// and '\v'), \ddd, where ddd is interpreted as an octal character value, and
// \[xX]dd, where dd is interpreted as a hexadecimal character value.
static int ReadOneChar(const string s, size_t* pos) {
  if (*pos >= s.size()) {
    return -1;
  }

  if (s[*pos] == '\\') {
    ++*pos;
    return HandleEscapedChar(s, pos);
  }

  int ival = s[*pos];
  ++*pos;
  return ival;
}

// expects "from" to point to leading \" and returns offset to trailing \"
// a zero return value indicates an error
static size_t ScanEscapeString(const string s, size_t from) {
  // skip initial quotes
  size_t pos = from + 1;
  int ival = ReadOneChar(s, &pos);
  while (-1 != ival) {
    if ('\"' == ival) {
      return pos;
    }
    ival = ReadOneChar(s, &pos);
  }
  NaClLog(LOG_ERROR, "unterminated string\n");
  return 0;
}

// input looks like:
//   rpc fib i(1) i(1) * I(10)
//   rpc rubyEval s("3.times{ puts 'lala' }") * s("")
//   rpc double_array D(5,3.1,1.4,4.1,1.5,5.9) * D(5)
//   rpc invalid_handle h(-1) *
//   rpc char_array C(9,A\b\f\n\t\"\"\\\x7F) * C(9)
void Tokenize(string line, vector<string>* tokens) {
  size_t pos_start = 0;

  while (pos_start < line.size()) {
    // skip over leading white space
    while (pos_start < line.size()) {
      const char c = line[pos_start];
      if (isspace((unsigned char)c)) {
        pos_start++;
      } else {
        break;
      }
    }

    if (pos_start >= line.size()) break;  //  <<< LOOP EXIT

    size_t pos_end = pos_start;
    while (pos_end < line.size()) {
      const char c = line[pos_end];

      if (isspace((unsigned char)c)) {
        break;
      } else if (c == '\"') {
        // NOTE: quotes are only really relevant in s("...").
        size_t end = ScanEscapeString(line, pos_end);
        if (end == 0) {
          NaClLog(LOG_ERROR, "unterminated string constant\n");
          return;
        }

        pos_end = end;
      } else {
        pos_end++;
      }
    }

    tokens->push_back(line.substr(pos_start, pos_end - pos_start));

    pos_start = pos_end + 1;
  }
}


static size_t FindComma(string s, size_t start) {
  size_t i;
  for (i = start; i < s.size(); ++i) {
    if (s[i] == ',') {
      break;
    }
  }
  return i;
}

//  input looks like:
//     I(5,1,2,3,4,5)
//     L(5,1,2,3,4,5)
static uint32_t SplitArray(string s,
                           vector<string>* tokens,
                           bool input,
                           bool check_size = true) {
  tokens->clear();
  uint32_t dim = StringToUint32(s, 2);

  if (!input) return dim;

  size_t i = 1 + FindComma(s, 2);
  size_t start = i;

  for (; i < s.size(); ++i) {
    if (s[i] == ',' || s[i] == ')') {
      tokens->push_back(s.substr(start, i - start));
      start = i + 1;
    }
  }

  if (check_size && dim != tokens->size()) {
    NaClLog(LOG_ERROR, "array token number mismatch %d vs %d\n",
            static_cast<int>(dim), static_cast<int>(tokens->size()));
    return 0;
  }

  return dim;
}

//  input looks like:
//     C(12,\110\145\154\154\157,\x77\x6f\x72\x6C\x64\X2E)
//     C(9,A\b\f\n\t\"\"\\\x7F)
static uint32_t SplitArrayChar(string s, vector<string>* tokens, bool input) {
  tokens->clear();
  uint32_t dim = StringToUint32(s, 2);

  if (!input) return dim;

  size_t i = 1 + FindComma(s, 2);
  size_t start = i;

  while (i < s.size() && s[i] != ')') {
     ReadOneChar(s, &i);
     tokens->push_back(s.substr(start, i - start));
     start = i;
  }

  if (dim != tokens->size()) {
    NaClLog(LOG_ERROR, "array token number mismatch %d vs %d\n",
            static_cast<int>(dim), static_cast<int>(tokens->size()));
    return 0;
  }

  return dim;
}


// substitute ${var_name} strings in s
string SubstituteVars(string s, NaClCommandLoop* ncl) {
  string result("");
  string var("");
  bool scanning_var_name = false;
  for (size_t i = 0; i < s.size(); ++i) {
    if (scanning_var_name) {
      if (s[i] == '{') {
        continue;
      } else if (s[i] == '}') {
        result.append(ncl->GetVariable(var));
        scanning_var_name = false;
      } else {
        var.push_back(s[i]);
      }
    } else {
      if (s[i] == '$') {
        scanning_var_name = true;
        var = "";
      } else {
        result.push_back(s[i]);
      }
    }
  }

  return result;
}


static string UnescapeString(string s) {
  string result("");
  size_t i = 3;
  while (i < s.size() && s[i] != '"') {
    int val = ReadOneChar(s, &i);
    result.push_back(val);
  }

  return result;
}

// initialize a single srpc arg using the information found in token.
// input indicates whether this is an input our an output arg.
// Output args are handled slightly different especially in the array case
// where the merely allocate space but do not initialize it.
bool ParseArg(NaClSrpcArg* arg,
              string token,
              bool input,
              NaClCommandLoop* ncl) {
  if (token.size() <= 2) {
    NaClLog(LOG_ERROR, "parameter too short: %s\n", token.c_str());
    return false;
  }

  const char type = token[0];
  vector<string> array_tokens;

  // Initialize the argument slot.  This enables freeing on failures.
  memset(arg, 0, sizeof(*arg));

  NaClLog(3, "TOKEN %s\n", token.c_str());
  if (token[1] != '(' || token[token.size() - 1] != ')') {
    NaClLog(LOG_ERROR, "malformed token '%s'\n", token.c_str());
    return false;
  }

  int dim;
  switch (type) {
    case NACL_SRPC_ARG_TYPE_INVALID:
      arg->tag = NACL_SRPC_ARG_TYPE_INVALID;
      break;
    case NACL_SRPC_ARG_TYPE_BOOL:
      arg->tag = NACL_SRPC_ARG_TYPE_BOOL;
      arg->u.bval = StringToInt32(token, 2);
      break;
    case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
      arg->tag = NACL_SRPC_ARG_TYPE_CHAR_ARRAY;
      dim = SplitArrayChar(token, &array_tokens, input);
      arg->arrays.carr = static_cast<char*>(calloc(dim, sizeof(char)));
      if (NULL == arg->arrays.carr) {
        NaClLog(LOG_ERROR, "alloc problem\n");
        return false;
      }
      arg->u.count = dim;
      if (input) {
        for (int i = 0; i < dim; ++i) {
          size_t dummy = 0;
          arg->arrays.carr[i] = ReadOneChar(array_tokens[i], &dummy);
        }
      }
      break;
    // This is alternative representation for CHAR_ARRAY:
    // R stands for "record".
    // example: R(8,1:0x44,2:1999,4:0,"a")
    // NOTE: commas inside of strings must currently be escaped
    case 'R':
      arg->tag = NACL_SRPC_ARG_TYPE_CHAR_ARRAY;
      dim = SplitArray(token, &array_tokens, input, false);
      arg->arrays.carr = static_cast<char*>(calloc(dim, sizeof(char)));
      if (NULL == arg->arrays.carr) {
        NaClLog(LOG_ERROR, "alloc problem\n");
        return false;
      }
      arg->u.count = dim;
      if (input) {
        int curr = 0;
        for (size_t i = 0; i < array_tokens.size(); ++i) {
          string s = array_tokens[i];
          if (s[0] == '"') {
            // the format of the token is: "string"
            size_t p = 1;
            while (p < s.size() && s[p] != '"') {
              if (curr >= dim) {
                NaClLog(LOG_ERROR, "size overflow in 'R' string parameter\n");
                return false;
              }
              int val = ReadOneChar(s, &p);
              arg->arrays.carr[curr] = (char) val;
              ++curr;
            }
          } else {
            // the format of the  token is: <num_bytes_single_digit>:<value>
            int num_bytes = s[0] - '0';
            if (s.size() < 3 || num_bytes < 1 || 8 < num_bytes || s[1] != ':') {
              NaClLog(LOG_ERROR, "poorly formatted 'R' parameter\n");
              return false;
            }
            int64_t val = StringToInt64(s, 2);
            while (num_bytes) {
              --num_bytes;
              if (curr >= dim) {
                NaClLog(LOG_ERROR, "size overflow in 'R' int parameter\n");
                return false;
              }
              arg->arrays.carr[curr] = val & 0xff;
              ++curr;
              val >>= 8;
            }
          }
        }

        if (curr != dim) {
           NaClLog(LOG_ERROR, "size mismatch in 'R' parameter\n");
           return false;
        }
      }
      break;
    case NACL_SRPC_ARG_TYPE_DOUBLE:
      arg->tag = NACL_SRPC_ARG_TYPE_DOUBLE;
      arg->u.dval = StringToDouble(token, 2);
      break;
    case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
      arg->tag = NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY;
      dim = SplitArray(token, &array_tokens, input);
      arg->arrays.darr = static_cast<double*>(calloc(dim, sizeof(double)));
      if (NULL == arg->arrays.darr) {
        NaClLog(LOG_ERROR, "alloc problem\n");
        return false;
      }
      arg->u.count = dim;
      if (input) {
        for (int i = 0; i < dim; ++i) {
          arg->arrays.darr[i] = StringToDouble(array_tokens[i]);
        }
      }
      break;
    case NACL_SRPC_ARG_TYPE_HANDLE:
      arg->tag = NACL_SRPC_ARG_TYPE_HANDLE;
      if (input) {
        arg->u.hval = ncl->FindDescByName(GetPayload(token));
      }
      break;
    case NACL_SRPC_ARG_TYPE_INT:
      arg->tag = NACL_SRPC_ARG_TYPE_INT;
      arg->u.ival = StringToInt32(token, 2);
      break;
    case NACL_SRPC_ARG_TYPE_INT_ARRAY:
      arg->tag = NACL_SRPC_ARG_TYPE_INT_ARRAY;
      dim = SplitArray(token, &array_tokens, input);
      arg->arrays.iarr = static_cast<int32_t*>(calloc(dim, sizeof(int32_t)));
      if (NULL == arg->arrays.iarr) {
        return false;
      }
      arg->u.count = dim;
      if (input) {
        for (int i = 0; i < dim; ++i) {
          arg->arrays.iarr[i] = StringToInt32(array_tokens[i]);
        }
      }
      break;
    case NACL_SRPC_ARG_TYPE_LONG:
      arg->tag = NACL_SRPC_ARG_TYPE_LONG;
      arg->u.lval = StringToInt64(token, 2);
      break;
    case NACL_SRPC_ARG_TYPE_LONG_ARRAY:
      arg->tag = NACL_SRPC_ARG_TYPE_LONG_ARRAY;
      dim = SplitArray(token, &array_tokens, input);
      arg->arrays.larr = static_cast<int64_t*>(calloc(dim, sizeof(int64_t)));
      if (NULL == arg->arrays.larr) {
        NaClLog(LOG_ERROR, "alloc problem\n");
        return false;
      }
      arg->u.count = dim;
      if (input) {
        for (int i = 0; i < dim; ++i) {
          arg->arrays.larr[i] = StringToInt64(array_tokens[i]);
        }
      }
      break;
    case NACL_SRPC_ARG_TYPE_STRING:
      arg->tag = NACL_SRPC_ARG_TYPE_STRING;
      arg->arrays.str =
        strdup(UnescapeString(token).c_str());
      if (NULL == arg->arrays.str) {
        NaClLog(LOG_ERROR, "alloc problem\n");
        return false;
      }
      break;
      /*
       * The two cases below are added to avoid warnings, they are only used
       * in the plugin code
       */
    case NACL_SRPC_ARG_TYPE_OBJECT:
    case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
    default:
      NaClLog(LOG_ERROR, "unsupported srpc arg type\n");
      return false;
  }

  return true;
}


bool OneArgEqual(NaClSrpcArg* arg1, NaClSrpcArg* arg2) {
  if (arg1->tag != arg2->tag) return false;

  switch (arg1->tag) {
    case NACL_SRPC_ARG_TYPE_INVALID:
      return true;
    case NACL_SRPC_ARG_TYPE_BOOL:
      return arg1->u.bval == arg2->u.bval;
    case NACL_SRPC_ARG_TYPE_DOUBLE:
      // TOOD(robertm): tolerate small deltas
      return arg1->u.dval == arg2->u.dval;
    case NACL_SRPC_ARG_TYPE_HANDLE:
      return arg1->u.hval == arg2->u.hval;
    case NACL_SRPC_ARG_TYPE_INT:
      return arg1->u.ival == arg2->u.ival;
    case NACL_SRPC_ARG_TYPE_LONG:
      return arg1->u.lval == arg2->u.lval;
    case NACL_SRPC_ARG_TYPE_STRING:
      return string(arg1->arrays.str) == string(arg2->arrays.str);

    case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
      if (arg1->u.count != arg2->u.count) return false;
      for (size_t i = 0; i < arg1->u.count; ++i) {
        if ( arg1->arrays.carr[i] != arg2->arrays.carr[i]) return false;
      }
      return true;
    case NACL_SRPC_ARG_TYPE_INT_ARRAY:
      if (arg1->u.count != arg2->u.count) return false;
      for (size_t i = 0; i < arg1->u.count; ++i) {
        if ( arg1->arrays.iarr[i] != arg2->arrays.iarr[i]) return false;
      }
      return true;
    case NACL_SRPC_ARG_TYPE_LONG_ARRAY:
      if (arg1->u.count != arg2->u.count) return false;
      for (size_t i = 0; i < arg1->u.count; ++i) {
        if ( arg1->arrays.larr[i] != arg2->arrays.larr[i]) return false;
      }
      return true;
    case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
      if (arg1->u.count != arg2->u.count) return false;
      for (size_t i = 0; i < arg1->u.count; ++i) {
        // TOOD(robertm): tolerate small deltas
        if ( arg1->arrays.darr[i] != arg2->arrays.darr[i]) return false;
      }
      return true;
    case NACL_SRPC_ARG_TYPE_OBJECT:
    case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
    default:
      NaClLog(LOG_FATAL, "unsupported srpc arg type\n");
      return false;
  }
}


bool AllArgsEqual(NaClSrpcArg** arg1, NaClSrpcArg** arg2) {
  if (NULL != *arg1 && NULL != *arg2) {
    if (!OneArgEqual(*arg1, *arg2)) return false;
    return AllArgsEqual(++arg1, ++arg2);
  } else if (0 == *arg1 && 0 == *arg2) {
    return true;
  } else {
    return false;
  }
}


bool ParseArgs(NaClSrpcArg** args,
               const vector<string>& tokens,
               size_t start,
               bool input,
               NaClCommandLoop* ncl) {
  for (size_t i = 0; args[i] != NULL; ++i) {
    if (!ParseArg(args[i], tokens[start + i], input, ncl)) {
      return false;
    }
  }
  return true;
}


static string StringifyOneChar(unsigned char c) {
  switch (c) {
    case '\"':
      return "\\\"";
    case '\b':
      return "\\b";
    case '\f':
      return "\\f";
    case '\n':
      return "\\n";
    case '\t':
      return "\\t";
    case '\v':
      return "\\v";
    case '\\':
      return "\\\\";
    default:
     // play it safe and escape closing parens which could
     // cause problems when this string is read back
     if (c < ' ' || 126 < c || c == ')') {
          stringstream result;
          result << "\\x" << std::hex << std::setw(2) <<
            std::setfill('0') << int(c);
          return result.str();
      } else {
        stringstream result;
        result << c;
        return result.str();
      }
  }
}


static string DumpDouble(const double* dval) {
  if (NACL_ISNAN(*dval)) {
    return "NaN";
  } else {
    stringstream result;
    result << *dval;
    return result.str();
  }
}


string DumpArg(const NaClSrpcArg* arg, NaClCommandLoop* ncl) {
  stringstream result;
  uint32_t count;
  uint32_t i;
  char* p;
  switch (arg->tag) {
    case NACL_SRPC_ARG_TYPE_INVALID:
      return "X()";
    case NACL_SRPC_ARG_TYPE_BOOL:
      result << "b(" << arg->u.bval << ")";
      return result.str();
    case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
      count = arg->u.count;
      result << "C(" << count << ",";
      for (i = 0; i < arg->u.count; ++i)
        result << StringifyOneChar(arg->arrays.carr[i]);
      result << ")";
      return result.str();
    case NACL_SRPC_ARG_TYPE_DOUBLE:
      result << "d(" << DumpDouble(&arg->u.dval) << ")";
      return result.str();
    case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
      count = arg->u.count;
      result << "D(" << count;
      for (i = 0; i < count; ++i) {
        result << "," << DumpDouble(&(arg->arrays.darr[i]));
      }
      result << ")";
      return result.str();
    case NACL_SRPC_ARG_TYPE_HANDLE:
      result << "h(" <<  ncl->AddDescUniquify(arg->u.hval, "imported") << ")";
      return result.str();
    case NACL_SRPC_ARG_TYPE_INT:
      result << "i(" << arg->u.ival << ")";
      return result.str();
    case NACL_SRPC_ARG_TYPE_INT_ARRAY:
      count = arg->u.count;
      result << "I(" << count;
      for (i = 0; i < count; ++i) {
        result << "," << arg->arrays.iarr[i];
      }
      result << ")";
      return result.str();
    case NACL_SRPC_ARG_TYPE_LONG:
      result << "l(" << arg->u.lval << ")";
      return result.str();
    case NACL_SRPC_ARG_TYPE_LONG_ARRAY:
      count = arg->u.count;
      result << "L(" << count;
      for (i = 0; i < count; ++i) {
        result << "," << arg->arrays.larr[i];
      }
      result << ")";
      return result.str();
    case NACL_SRPC_ARG_TYPE_STRING:
      result << "s(\"";
      for (p = arg->arrays.str; '\0' != *p; ++p)
        result << StringifyOneChar(*p);
      result << "\")";
      return result.str();
    case NACL_SRPC_ARG_TYPE_OBJECT:
    case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
    default:
      NaClLog(LOG_ERROR, "unknown or unsupported type '%c'\n", arg->tag);
      return "";
  }
}


void BuildArgVec(NaClSrpcArg* argv[], NaClSrpcArg arg[], size_t count) {
  for (size_t i = 0; i < count; ++i) {
    NaClSrpcArgCtor(&arg[i]);
    argv[i] = &arg[i];
  }
  argv[count] = NULL;
}


void FreeArrayArg(NaClSrpcArg* arg) {
  switch (arg->tag) {
    case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
      free(arg->arrays.carr);
      break;
    case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
      free(arg->arrays.darr);
      break;
    case NACL_SRPC_ARG_TYPE_INT_ARRAY:
      free(arg->arrays.iarr);
      break;
    case NACL_SRPC_ARG_TYPE_LONG_ARRAY:
      free(arg->arrays.larr);
      break;
    case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
    case NACL_SRPC_ARG_TYPE_OBJECT:
      NaClLog(LOG_ERROR, "unsupported srpc arg type\n");
      break;
    case NACL_SRPC_ARG_TYPE_STRING:
      free(arg->arrays.str);
      break;
    case NACL_SRPC_ARG_TYPE_INVALID:
    case NACL_SRPC_ARG_TYPE_BOOL:
    case NACL_SRPC_ARG_TYPE_DOUBLE:
    case NACL_SRPC_ARG_TYPE_HANDLE:
    case NACL_SRPC_ARG_TYPE_INT:
    case NACL_SRPC_ARG_TYPE_LONG:
    default:
      break;
  }
}


void FreeArrayArgs(NaClSrpcArg** args) {
  for (size_t i = 0; args[i] != NULL; ++i) {
    FreeArrayArg(args[i]);
  }
}
