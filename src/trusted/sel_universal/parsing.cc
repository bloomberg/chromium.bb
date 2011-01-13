/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

// This module contains parsing code to be used with sel_universal.
// Since sel_universal is a testing tool and parsing in C++ is no fun,
// this code merely aims at being maitainable.
// Efficiency and proper dealing with bad input are non-goals.

#include <string>
#include <vector>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if NACL_WINDOWS
#include <float.h>
#define NACL_ISNAN(d) _isnan(d)
#else
/* Windows doesn't have the following header files. */
# include <math.h>
#define NACL_ISNAN(d) isnan(d)
#endif  /* NACL_WINDOWS */

#include "native_client/src/include/portability_string.h"

#include "native_client/src/trusted/sel_universal/rpc_universal.h"
#include "native_client/src/trusted/sel_universal/parsing.h"
#include "native_client/src/shared/platform/nacl_log.h"


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

static int HandleEscapedChar(const string s, size_t* pos) {
  if (*pos >= s.size()) return -1;
  switch (s[*pos]) {
    case '\\':
      ++*pos;
      return '\\';
    case '\"':
      ++*pos;
      return '\"';
    case 'b':
      ++*pos;
      return '\b';
    case 'f':
      ++*pos;
      return '\f';
    case 'n':
      ++*pos;
      return '\n';
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

// expects from to ;point' to leading \" and returns offset to trailing \"
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
    /* skip leading white space */
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
          return;
        }

        pos_end = end;
      }
      pos_end++;
    }

    tokens->push_back(line.substr(pos_start, pos_end - pos_start));

    pos_start = pos_end + 1;
  }
}


//  input looks like:
//     I(5,1,2,3,4,5)
//     L(5,1,2,3,4,5)
static uint32_t SplitArray(string s, vector<string>* tokens, bool input) {
  tokens->clear();
  uint32_t dim = StringToUint32(s, 2);

  if (!input) return dim;

  size_t i;
  for (i = 2; i < s.size(); ++i) {
    if (s[i] == ',') {
      ++i;
      break;
    }
  }

  size_t start = i;
  for (; i < s.size(); ++i) {
    if (s[i] == ',' || s[i] == ')') {
      tokens->push_back(s.substr(start, i - start));
      start = i + 1;
    }
  }

  if (dim != tokens->size()) {
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

  size_t i;
  for (i = 2; i < s.size(); ++i) {
    if (s[i] == ',') {
      ++i;
      break;
    }
  }

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
bool ParseArg(
  NaClSrpcArg* arg, string token, bool input, NaClCommandLoop* ncl) {
  int dim;

  if (token.size() <= 2) {
    NaClLog(LOG_ERROR, "parameter too short: %s\n", token.c_str());
    return false;
  }

  const char type = token[0];
  vector<string> array_tokens;

  // Initialize the argument slot.  This enables freeing on failures.
  memset(arg, 0, sizeof(*arg));

  NaClLog(1, "TOKEN %s\n", token.c_str());
  if (token[1] != '(' || token[token.size() - 1] != ')') {
    NaClLog(LOG_ERROR, "malformed token");
    return false;
  }

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
        arg->u.hval = ncl->FindDescByName(token.substr(2, token.size() - 3));
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
      arg->arrays.str = strdup(UnescapeString(token).c_str());
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


static void PrintOneChar(char c) {
  switch (c) {
    case '\"':
      printf("\\\"");
      break;
    case '\b':
      printf("\\b");
      break;
    case '\f':
      printf("\\f");
      break;
    case '\n':
      printf("\\n");
      break;
    case '\t':
      printf("\\t");
      break;
    case '\v':
      printf("\\v");
      break;
    case '\\':
      printf("\\\\");
      break;
    default:
      if (' ' > c || 127 == c) {
        printf("\\x%02x", c);
      } else {
        printf("%c", c);
      }
  }
}


static void DumpDouble(const double* dval) {
  if (NACL_ISNAN(*dval)) {
    printf("NaN");
  } else {
    printf("%f", *dval);
  }
}


void DumpArg(const NaClSrpcArg* arg, NaClCommandLoop* ncl) {
  uint32_t count;
  uint32_t i;
  char* p;

  switch (arg->tag) {
    case NACL_SRPC_ARG_TYPE_INVALID:
      printf("X()");
      break;
    case NACL_SRPC_ARG_TYPE_BOOL:
      printf("b(%d)", arg->u.bval);
      break;
    case NACL_SRPC_ARG_TYPE_CHAR_ARRAY:
      for (i = 0; i < arg->u.count; ++i)
        PrintOneChar(arg->arrays.carr[i]);
      break;
    case NACL_SRPC_ARG_TYPE_DOUBLE:
      printf("d(");
      DumpDouble(&arg->u.dval);
      printf(")");
      break;
    case NACL_SRPC_ARG_TYPE_DOUBLE_ARRAY:
      count = arg->u.count;
      printf("D(%"NACL_PRIu32"", count);
      for (i = 0; i < count; ++i) {
        printf(",");
        DumpDouble(&(arg->arrays.darr[i]));
      }
      printf(")");
      break;
    case NACL_SRPC_ARG_TYPE_HANDLE:
      printf("h(%s)", ncl->AddDescUniquify(arg->u.hval, "imported").c_str());
      break;
    case NACL_SRPC_ARG_TYPE_INT:
      printf("i(%"NACL_PRId32")", arg->u.ival);
      break;
    case NACL_SRPC_ARG_TYPE_INT_ARRAY:
      count = arg->u.count;
      printf("I(%"NACL_PRIu32"", count);
      for (i = 0; i < count; ++i)
        printf(",%"NACL_PRId32, arg->arrays.iarr[i]);
      printf(")");
      break;
    case NACL_SRPC_ARG_TYPE_LONG:
      printf("l(%"NACL_PRId64")", arg->u.lval);
      break;
    case NACL_SRPC_ARG_TYPE_LONG_ARRAY:
      count = arg->u.count;
      printf("L(%"NACL_PRIu32"", count);
      for (i = 0; i < count; ++i)
        printf(",%"NACL_PRId64, arg->arrays.larr[i]);
      printf(")");
      break;
    case NACL_SRPC_ARG_TYPE_STRING:
      printf("s(\"");
      for (p = arg->arrays.str; '\0' != *p; ++p)
        PrintOneChar(*p);
      printf("\")");
      break;
      /*
       * The two cases below are added to avoid warnings, they are only used
       * in the plugin code
       */
    case NACL_SRPC_ARG_TYPE_OBJECT:
      /* this is a pointer that NaCl module can do nothing with */
      printf("o(%p)", arg->arrays.oval);
      break;
    case NACL_SRPC_ARG_TYPE_VARIANT_ARRAY:
      count = arg->u.count;
      printf("A(%"NACL_PRIu32"", count);
      for (i = 0; i < count; ++i) {
        printf(",");
        DumpArg(&arg->arrays.varr[i], ncl);
      }
      printf(")");
      break;
    default:
      break;
  }
}
