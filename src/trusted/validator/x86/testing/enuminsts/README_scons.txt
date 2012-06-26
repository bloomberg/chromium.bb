Copyright (c) 2012 The Native Client Authors. All rights reserved.
Use of this source code is governed by a BSD-style license that be
found in the LICENSE file.

************************************************************************
 NOTE: The Scons version of enuminst is documented in this file.
 It has less functionality than the Makefile version.
************************************************************************

Exhaustive instruction enumeration test for x86 Native Client decoder.
Limited testing of validator.

SCONS now builds the 32- and 64-bit versions of enuminst. The scons
build incorporates NaCl and Ragel-Deterministic Finite Automata (R-DFA)
validators. It does not include Xed.

The binaries are available in (for example)
    scons-out/opt-linux-x86-64/staging/enuminst
    scons-out/opt-linux-x86-32/staging/enuminst

Some suggestions on running enuminst:
  enuminst --legalnacl --legal=nacl --legal=ragel
    Compare lengths of instructions that decode for nacl.
    Filters out most NaCl illegal instructions.

  enuminst --illegal=nacl --legal=ragel --print=ragel
    Identifies instructions legal for R-DFA but not nacl.

  enuminst --nacllegal --legal=nacl --illegal=ragel --print=nacl
    Identifies instructions legal for nacl but not R-DFA.

The NaCl validator supports a partial-validation mode, which for a
a single instruction determines if it could or could not appear in
a legal Native Client module. Instructions such as "ret" can never
appear in a valid NaCl module, and are rejected. Instructions such
as "jmp *%eax" can appear, so  they are accepted.
