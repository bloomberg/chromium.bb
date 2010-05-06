/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

#include "native_client/src/trusted/plugin/srpc/nexe_arch.h"

#include <stdio.h>
#include <string>

#include "native_client/src/include/portability.h"
#include "native_client/src/trusted/platform_qualify/nacl_os_qualify.h"
#include "native_client/src/trusted/plugin/srpc/utility.h"

using std::string;

namespace nacl_srpc {

// Returns the kind of SFI sandbox implemented by sel_ldr on this
// platform: ARM, x86-32, x86-64.
//
// This is a function of the current CPU, OS, browser, installed
// sel_ldr(s).  It is not sufficient to derive the result only from
// build-time parameters since, for example, an x86-32 plugin is
// capable of launching a 64-bit NaCl sandbox if a 64-bit sel_ldr is
// installed (and indeed, may only be capable of launching a 64-bit
// sandbox).
//
// TODO(adonovan): this function changes over time as we deploy
// different things.  It should be defined once, authoritatively,
// instead of guessed at where it's needed, e.g. here.
static string GetSandbox();

#if NACL_ARCH_CPU_X86_FAMILY

static string GetSandbox() {
#if !defined(NACL_STANDALONE) && NACL_ARCH_CPU_X86_64 && NACL_LINUX
  return "x86-64";  // 64-bit Chrome on Linux
#else
  return NaClOsIs64BitWindows() == 1
      ? "x86-64"   // 64-bit Windows (Chrome, Firefox)
      : "x86-32";  // everything else.
#endif
}

#elif NACL_ARCH_CPU_ARM_FAMILY

static string GetSandbox() { return "ARM"; }

#else /* hopefully unreachable */

#error GetSandbox() missing on this platform

#endif

// Removes leading and trailing ASCII whitespace from |*s|.
static string TrimWhitespace(const string& s) {
  size_t start = s.find_first_not_of(" \t");
  size_t end = s.find_last_not_of(" \t");
  return start == string::npos
      ? ""
      : s.substr(start, end + 1 - start);
}

// Parse one line of the nexes attribute.
// Returns true iff it's a match, and writes URL to |*url|.
static bool ParseOneNexeLine(const string& nexe_line,
                             const string& sandbox,
                             string* url) {
  size_t semi = nexe_line.find(':');
  if (semi == string::npos) {
    dprintf(("missing colon in line of 'nexes' attribute"));
    return false;
  }
  string attr_sandbox = TrimWhitespace(nexe_line.substr(0, semi));
  string attr_url = TrimWhitespace(nexe_line.substr(semi + 1));
  bool match = sandbox == attr_sandbox;
  dprintf(("ParseOneNexeLine %s %s: match=%d\n",
           attr_sandbox.c_str(), attr_url.c_str(), match));
  if (match) *url = attr_url;
  return match;
}

// Parses an <embed nexes='...'> attribute and returns the URL of the
// nexe matching |sandbox|.  Returns true and assigns |*result| to the
// URL on success; otherwise returns false.
//
// nexes       ::= <nexe-line> ( '\n' <nexes> )*
// nexe-line   ::= <sandbox> ':' <nexe-url>
//
// Spaces surrounding <sandbox> and <nexe-url> are ignored, but
// <nexe-url> may contain internal spaces.
static bool FindNexeForSandbox(const string& nexes_attr,
                               const string& sandbox,
                               string* url) {
  size_t pos, oldpos;
  for (oldpos = 0, pos = nexes_attr.find('\n', oldpos);
       pos != string::npos;
       oldpos = pos +  1, pos = nexes_attr.find('\n', oldpos)) {
    if (ParseOneNexeLine(nexes_attr.substr(oldpos, pos - oldpos), sandbox,
                         url)) {
      return true;
    }
  }
  if (oldpos < nexes_attr.size() - 1) {  /* attr doesn't end in newline */
    if (ParseOneNexeLine(nexes_attr.substr(oldpos), sandbox, url)) return true;
  }
  return false;
}

// See nexe_arch.h.
// An entry point for testing.
bool GetNexeURL(const char* nexes_attr, string* result) {
  const string sandbox = GetSandbox();
  dprintf(("GetNexeURL(): sandbox='%s' nexes='%s'.\n",
           sandbox.c_str(), nexes_attr));
  if (FindNexeForSandbox(nexes_attr, sandbox, result)) return true;
  *result = "No Native Client executable was provided for the " + sandbox
      + " sandbox.";
  return false;
}

}  // namespace nacl_srpc
