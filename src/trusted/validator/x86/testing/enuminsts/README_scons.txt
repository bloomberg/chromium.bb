Copyright (c) 2012 The Native Client Authors. All rights reserved.
Use of this source code is governed by a BSD-style license that be
found in the LICENSE file.

************************************************************************
 NOTE: The Scons version of enuminst is documented in this file.
 It has less functionality than the Makefile version.
************************************************************************

Exhaustive instruction enumeration test for x86 Native Client decoder.
Limited testing of validator.

Currently SCONS builds only the 64-bit version of enuminst. The scons
build incorporates NaCl and Ragel-Deterministic Finite Automata (R-DFA)
validators. It does not include Xed.

The binary is available in (for example)
    scons-out/opt-linux-x86-64/staging/enuminst
The 32-bit version will be completed while 64-bit debugging continues.

Some suggestions on running enuminst:
  enuminst --legal=nacl --legal=ragel
    Compare lengths of instructions that decode for both nacl and R-DFA.
    As of June 2012 there were about 4953178 of these. Note that, as enuminst
    is not careful about how it enumerates instructions, there are a
    huge number of duplicates in this count.

  enuminst --illegal=nacl --legal=ragel --print=ragel
    Identifies instructions decoded by R-DFA but not by nacl
    As of June 2012 there were about 1300 of these.

  enuminst --legal=nacl --illegal=ragel --print=nacl
    Identifies instructions decoded by nacl but not by R-DFA
    As of June 2012 there were about 6000000 of these.

The NaCl validator supports a partial-validation mode, which for a
a single instruction determines if it could or could not appear in
a legal Native Client module. Instructions such as "ret" can never
appear in a valid NaCl module, and are rejected. Instructions such
as "jmp *%eax" can appear, so  they are accepted.

When R-DFA supports a partial-validation mode, checking if a single
instruction might be legal (ignoring inter-instruction rules) these
tests should be useful:

  enuminst --nacllegal --legal=nacl --illegal=ragel --print=ragel
      Prints instructions accepted by NaCl but not R-DFA

  enuminst --nacllegal --legal=ragel --illegal=nacl print=nacl
      Prints instructions legal for R-DFA but not NaCl.
      Currently, no problems!

  enuminst --nacl --ragel
      Compares instruction length only.

