Copyright (c) 2012 The Native Client Authors. All rights reserved.
Use of this source code is governed by a BSD-style license that be
found in the LICENSE file.

Exhaustive instruction enumeration test for x86 Native Client decoder.

This test performs an exhaustive enumeration of x86 instructions,
comparing the NaCl decoder to the xed decoder from Intel. It currently
compares:

   * instruction validity (still being worked on in x86-32).
   * instruction length
   * opcodes (64-bit only).

This projectd is currently built using make, as defined by Makefile.
It builds a 64-bit decoder checker by default.  To build a 32-bit
decoder checker it should be enough to provide BITS=32 command line
parameter when building with make.  You also need to build the correct
version of NaCl libraries. By default, it assumes that you have
downloaded a (Pin) version of xed. See variable PINV in Makefile to
see the version assumed.  You can specify the xed on the command line,
allowing use other versions of xed.

Before building this test you must download Intel's xed decoder,
distributed as part of PIN. It is available for free from

   http://www.pintool.org.

NOTE: The executable enuminsts-64-6688 is a frozen version of
enuminsts-64 that uses the x86-64 validator/decoder associated with
nacl revision 6688. It is intended to be used as a base line to
compare decoders in later revisions. Currently, there isn't an
executable enuminst-32-6688.

Enumerating NaCl instructions that aren't legal xed instructions
----------------------------------------------------------------

One source of severe problems with a NaCl validator is that it might
accept instructions that are illegal, but none the less get validated
as legal. To test this, run (where XX in { 32 , 64 }):

    ./enuminsts-64 --illegal=xed --print=nacl --nacllegal

Enumerating Instructions That are xed and NaCl
----------------------------------------------

To generate an enumeration of (potentially) valid xed instructions,
that are also legal in the NaCl decoder, run the following (where XX
in { 32 , 64}):

    ./enuminsts-XX --print=xed --nacl --printenum > tmp-XX.txt
    sort -b -u tmp-XX.txt > XedGood-XX.txt

Note: The code will not print out legal xed instructions if the
corresponding NaCl decoder/validator can determine it is
illegal. Warning: this test is incomplete and some instructions may be
accepted even though they shouldn't validate.

To run manual tests to see what instructions decode (using the x86-64
decoder) the same way as xed does, run the following commands:

    ./enuminsts-64-6688 --print=nacl --opcode_bytes > bytes.base
    ./enuminsts-64 --xed --print=nacl --opcode_bytes > bytes.new
    sort -u bytes.base > bytes-sorted.base
    sort -u bytes.new > bytes-sorted.new
    diff bytes-sorted.base bytes-sorted.new

The first two lines generates a large sample of opcode byte sequences
that are handled the same way for both xed and the x86-64 decoder.
Each line in the generated file is a matching opcode byte sequence.

The second two lines canonicalizes the output, and removes
duplicates. This is done to improve the comparison between the two
decoders.

The last line does a diff to find matching opcode sequences that have
changed. Diff is used to verify that there are no differences in the
set of matching opcode sequences (and hence, no changes to the
corresponding decoder).

To run manual tests to see what instructions are decoded differently
than xed, using the x86-64 decoder, run the following commands:

    ./enuminsts-64-6688 --checkoperands --nacllegal --nops \
          --xedimplemented > problems.base
    ./enuminsts-64 --xed --nacl --checkoperands --nacllegal --nops \
          --xedimplemented > problems.new
    diff problems.base problems.new

The first two lines generate error reports for differences between xed
and the x86-64 decoder.

The --xed argument defines that the xed decoder should be used while
the --nacl argument defines that the nacl decoder should also be used.
By default, unless specified otherwise, only opcode byte sequences
that are legal by each decoder will be processed.

The --checkoperands argument forces additional checks.  By default,
only the bytes of the instruction, and the opcode name is
compared. When this flag is added, the (decode) arguments to the
instruction are also compared. Note that for x86-32, this option
should not be used, since the NaCl x86-32 decoder doesn't know how to
properly print arguments.

The --nacllegal check turns off reporting instructions that are known
to be illegal in the NaCl validator, no matter what context they
appear in. Since they will be illegal, we don't worry about any
additional problems with decoding the instruction.

The --nops flag doesn't bother to compare arguments between xed and
the NaCl decoder for nops, since it is assumed that the operation is
a nop.

The --xedimplemented flag turns off checking instructions for which
xed is known to not understand.

To provide a basis for what problems.base and problems.new should
contain, file enum-out.txt contains what should be expected for
problems.base.

To run manual tests to see what instructions are decoded differently
than xed, using the NaCl x86-32 decoder, run the following command:

    ./enuminsts-32 --xed --nacl --nacllegal --nops --xedimplemented

Note that the command line arguments are the same as for
./enuminst-64, other than --checkoperands is not used. The reason for
this is that the NaCl x86-32 disassembler does not fully decode
command line arguments, and hence, has many errors. This only effects
decoding and does not effect validation.
