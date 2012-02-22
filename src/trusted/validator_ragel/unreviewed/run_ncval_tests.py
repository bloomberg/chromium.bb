#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

"""Runs in-tree NaCl x86 validator tests against the DFA-based validator.

Takes *.hex files as input bytes.  Each test output is a union of all errors
occurred when running the input bytes through the DFA-based validator.  The
latter can only detect one error per bundle without making mistakes about
offending instruction.  After each run the invalid instruction is replaced with
a sequence of NOPs of the same length until the code passes the validator.

The output from each test is then compared to golden/ files that parse_hex.py
produced.
"""

import optparse
import os
import re
import string
import subprocess
import sys


def WriteFile(filename, data):
  fh = open(filename, "w")
  try:
    fh.write(data)
  finally:
    fh.close()


def ReadFile(filename):
  try:
    file = open(filename, 'r')
  except IOError, e:
    print >> sys.stderr, ('Error reading file %s: %s' %
                          (filename, e.strerror))
    return None
  contents = file.read()
  file.close()
  return contents


def PrintError(msg):
  print >> sys.stderr, 'error: %s' % msg


class InstByteSequence:
  """Parses a sequence of instructions, generates code pieces out of them.

  Each instruction comes as a sequence of bytes in the input.  It is required
  that the input source has the information of instruction boundaries in the
  byte stream.

  """

  def __init__(self):
    self.inst_bytes = []
    self.offsets = {}

  def Parse(self, hexfile):
    """Read instruction bytes.

    Args:
      hexfile: Name of file with instruction descriptions. Each line is a
          a sequence of hex-encoded bytes separated by spaces or a comment.
    """
    off = 0
    inst_begin = 0
    for line in open(hexfile, 'r').readlines():
      inst_begin = off
      if line.startswith('#'):
        continue
      for word in line.rstrip().split(' '):
        if re.match(r'^\s*$', word):
          continue
        assert(re.match(r'[0-9a-zA-Z][0-9a-zA-Z]', word))
        self.inst_bytes.append(word)
        off += 1
      self.offsets[inst_begin] = off

  def HasOffset(self, offset):
    """Tells if the given offset contains the first byte of some instruction."""
    return offset in self.offsets

  def InstInBundle(self, inst_offset, bundle_start):
    assert((bundle_start + inst_offset) in self.offsets)
    if bundle_start + 32 >= self.offsets[bundle_start + inst_offset]:
      return True
    return False

  def OffsetBelongsToInst(self, offset, inst_start):
    """Detects whether the byte at given offset is a part of an instruction.

    Args:
        offset: An integer offset, the address of the given byte.
        inst_start: An integer offset of the beginning of the instruction.
    """
    assert(inst_start in self.offsets)
    if offset == inst_start:
      return True
    for i in xrange(inst_start, len(self.inst_bytes)):
      if self.HasOffset(i):
        return False
      if i == offset:
        return True
    return False

  def StuboutInst(self, offset):
    """Fill the instruction at offset with NOP bytes."""
    assert(offset in self.offsets)
    for off in xrange(offset, self.offsets[offset]):
      self.inst_bytes[off] = '90'

  def GenAsmBundle(self, start_offset):
    """Generates 32 bytes of the original instructions suitable for assembler.

    May start from arbitrary offsets, which is useful when we have replaced a
    bundle-crossing instruction with NOPs.  Append enough NOPs to form 32 bytes
    if there are not enough instructions.

    Args:
        start_offset: the offset of the first byte to output
    Returns:
        A pair of (asm, has_next), where:
          asm: text representing code for the bundle suitable as assembler input
          has_next: boolean value indicating presence of instruction bytes after
            the bundle
    """
    off = start_offset
    asm = '.text\n'
    bytes_written = 0

    # Allow to start from offset that does not start an instruction.
    sep = '.byte 0x'
    while off < len(self.inst_bytes):
      if off in self.offsets:
        break
      asm += sep + self.inst_bytes[off]
      sep = ', 0x'
      bytes_written += 1
      off += 1
    if bytes_written > 0:
      asm += '\n'

    # Write the bytes from our source.
    while bytes_written != 32 and off != len(self.inst_bytes):
      sep = '.byte 0x'
      inst_fully_written = True
      for i in xrange(off, self.offsets[off]):
        asm += sep + self.inst_bytes[i]
        bytes_written += 1
        sep = ', 0x'
        if bytes_written == 32:
          inst_fully_written = False
          break
      asm += '\n'
      if inst_fully_written:
        off = self.offsets[off]

    has_next = True
    if off == len(self.inst_bytes):
      has_next = False

    # Write NOPs if we did not get generate enough bytes yet.
    for i in xrange((32 - (bytes_written % 32)) % 32):
      asm += 'nop\n'
    assert(asm)
    return (asm, has_next)

  def GenAsm(self):
    """Generates text for all instructions suitable for assembler."""
    asm = '.text\n'
    off = 0
    while True:
      sep = '.byte 0x'
      for i in xrange(off, self.offsets[off]):
        asm += sep + self.inst_bytes[i]
        sep = ', 0x'
      off = self.offsets[off]
      asm += '\n'
      if off == len(self.inst_bytes):
        break
    return asm


class TestRunner:
  """Knows about naming tests, files, placement of golden files, etc."""

  def __init__(self, tmpdir, gas, decoder, validator):
    self.tmp = tmpdir
    self.gas = gas
    self.decoder = decoder
    self.validator = validator

  def CheckDecoder(self, asm, hexfile):
    """Test if we are decoding correctly.

    Generate binary code from given text, disassembly it with the DFA-based
    decoder, check correctness.

    Args:
        asm: the code to feed into assembler
        hexfile: the original file name, where asm was extracted from, useful
            for grouping all artifacts from each test under the same name
            prefix.
    Returns:
        True iff the test passes.
    """
    basename = os.path.basename(hexfile[:-4])
    asmfile = os.path.join(self.tmp, basename + '.all.s')
    objfile = os.path.join(self.tmp, basename + '.o')
    WriteFile(asmfile, asm)
    gas_cmd = [self.gas, asmfile, '-o', objfile]
    if subprocess.call(gas_cmd) != 0:
      PrintError('assembler failed to execute command: %s' % gas_cmd)
      return False
    decoder_process = subprocess.Popen([self.decoder, objfile],
                                       stdout=subprocess.PIPE)
    (decode_out, decode_err) = decoder_process.communicate()
    WriteFile(os.path.join(self.tmp, basename + '.all.decode.out'), decode_out)
    # TODO(pasko): Compare output with objdump or a golden file.
    return True

  def CheckAsm(self, asm, hexfile, run_id):
    """Extract the first error offset from the validator on given code.

    Args:
        asm: The code to feed into assembler and then the tested validator.
        hexfile: Original input file name, where the code was extracted from.
        run_id: An integer identifier of the certain testing run, must be
            distinct from one invocation to another.

    Returns:
        A pair of (non_fatal, error_offset), where:
          non_fatal: True iff testing steps did not reveal any fatal errors.
          error_offset: The offset of the first instruction that the validator
              rejected.
    """
    asmfile = os.path.basename(hexfile[:-4]) + ('_part%03d.s' % run_id)
    asmfile = os.path.join(self.tmp, asmfile)
    WriteFile(asmfile, asm)
    basename = asmfile[:-2]
    objfile = basename + '.o'
    if subprocess.call([self.gas, asmfile, '-o', objfile]) != 0:
      return (False, None)
    validator_process = subprocess.Popen([self.validator, objfile],
                                         stdout=subprocess.PIPE)
    (val_out, val_err) = validator_process.communicate()
    offsets = []
    for line in string.split(val_out, '\n'):
      re_match = re.match(r'offset ([^:]+):.+', line)
      if not re_match:
        continue
      offsets.append(int(re_match.group(1), 16))
    assert(len(offsets) < 2)
    if len(offsets) == 0:
      return (True, None)
    return (True, offsets[0])

  def CompareOffsets(self, off_info, hexfile):
    """Check for correctness the knowledge from analysing a single test.

    Args:
      off_info: A dict mapping an integer offset to a list of string errors
          encountered for this offset.  The order of errors is important.
      hexfile: Original input file name, where the code was extracted from.
    Returns:
        True iff the comparison with the golden file succeeds.
    """
    output = ''
    for off, msg_list in sorted(off_info.iteritems()):
      for msg in msg_list:
        output += 'offset 0x%x: %s\n' % (off, msg)
    basename = os.path.basename(hexfile[:-4])
    output_file = os.path.join(self.tmp , basename + '.val.out')
    WriteFile(output_file, output)
    golden_file = os.path.join('golden', basename + '.val.ref')
    golden = ReadFile(golden_file)
    if output == golden:
      return True
    PrintError('files differ: %s %s' % (golden_file, output_file))
    return False

  def RunTest(self, test):
    """Runs the test by name.  Checks the decoder and the validator.

    Each test contains a sequence of instructions described as individual hex
    bytes.  Checks the decoder by feeding it with the whole code sequence of the
    test.

    Checks the validator by separating the input code into 32-byte chunks,
    asking the validator to try validate every piece, compare the answers
    against the golden output.

    Args:
        test: the name of the test, used only to construct the names of the .hex
            and the golden file.
    Returns:
        True iff the test passes.
    """
    hexfile = 'testdata/64/%s.hex' % test
    if not os.path.exists(hexfile):
      PrintError('%s: no such file' % hexfile)
      return False

    # Check disassembling of the whole input.
    hex_instructions = InstByteSequence()
    hex_instructions.Parse(hexfile)
    if not self.CheckDecoder(hex_instructions.GenAsm(), hexfile):
      return False

    # Cut the input instruction sequence in bundles and run a test for each
    # bundle.  For instructions that cross a bundle run an additional
    # test that starts from this instruction.
    start_pos = 0
    runs = 0
    top_errors = {}  # Mapping of offset to a list of error strings.
    has_next = True
    while has_next:
      (asm, has_next) = hex_instructions.GenAsmBundle(start_pos)
      # Collect validation reject offsets, stub them out, repeat until no error.
      while True:
        (status, err_in_bundle) = self.CheckAsm(asm, hexfile, runs)
        runs += 1
        if not status:
          return False
        if err_in_bundle == None:
          break
        err_offset = start_pos + err_in_bundle
        if not hex_instructions.HasOffset(err_offset):
          PrintError('validator returned error on offset that is not a ' +
                     'start of an instruction: 0x%x' % err_offset)
          return False
        if hex_instructions.InstInBundle(err_in_bundle, start_pos):
          top_errors[err_offset] = ['validation error']
          hex_instructions.StuboutInst(err_offset)
          (asm, _) = hex_instructions.GenAsmBundle(start_pos)
        else:
          # If the instruction crosses the bundle boundary, we check if it gets
          # validated as placed at address 0mod32, then go processing the next
          # bundle.  Stubout the instruction if necessary.
          top_errors[err_offset] = ['crosses boundary']
          (asm, _) = hex_instructions.GenAsmBundle(err_offset)
          (status, cross_err_off) = self.CheckAsm(asm, hexfile, runs)
          runs += 1
          if not status:
            return False
          if cross_err_off != None:
            if hex_instructions.OffsetBelongsToInst(err_offset + cross_err_off,
                                                    err_offset):
              top_errors[err_offset].append('validation error')
          hex_instructions.StuboutInst(err_offset)
          break
      start_pos += 32

    # Compare the collected offsets with the golden file.
    if not self.CompareOffsets(top_errors, hexfile):
      return False
    return True


def Main():
  parser = optparse.OptionParser()
  parser.add_option(
      '-t', '--tests', dest='tests',
# new validator allows unaligned calls:
#      default='call_not_aligned',
#      default='call_not_aligned_16',
# reports error on instruction that follows the xchg esp, ebp, replacing it does
#   not help causing an infinite loop
#      default='stack_regs',
#      default='mov-lea-rbp-bad-1',
#      default='mov-lea-rbp-bad-2',
#      default='mov-lea-rbp-bad-3',
#      default='mov-lea-rbp-bad-4',
#      default='mv_ebp_alone',
# the @ expansion is not yet parsed:
#      default='call0',
#      default='call1',
#      default='call_long',
#      default='call_short',
#      default='jmp0',
#      default='jump_not_atomic',
#      default='jump_not_atomic_1',
#      default='jump_overflow',
#      default='jump_underflow',
#      default='mv_ebp_add_crossing',
#      default='return',
#      default='segment_aligned',
#      default='segment_not_aligned',
#      default='update-rsp',
# needs a tiny fix in old validator input file:
#      default='legacy',
# http://code.google.com/p/nativeclient/issues/detail?id=2529
#      default='maskmov_test',
# http://code.google.com/p/nativeclient/issues/detail?id=2603
#      default='bsf-mask',
#      default='bsr-mask',
# http://code.google.com/p/nativeclient/issues/detail?id=2606
#      default='extensions',
# http://code.google.com/p/nativeclient/issues/detail?id=2607
#      default='indirect_jmp_masked',
#      default='jump_atomic',
# super-instruction crosses boundary, small instruction does not:
#      default='fpu',
# have .hex, but not .rval:
#      default='data66prefix,rdmsr,stubseq,test_alias,test_insts,wrmsr',
# need more investigation:
#      default='jump_outside,mmx,movs_test,prefix-2,prefix-single,strings,sse',
# these tests pass:
      default='3DNow,add_cs_gs_prefix,add_mult_prefix,addrex,AhNotSubRsp,bt,call_aligned,call-ex,cmpxchg,cpuid,dup-prefix,hlt,incno67,indirect_jmp_not_masked,invalid_base,invalid_base_store,invalid_width_index,jmp-16,lea,lea-add-rsp,lea-rsp,mov-esi-nop-use,mov_esp_add_rsp_r15,mov-lea-rbp,mov-lea-rsp,movlps-ex,mov_rbp_2_rsp,movsbw,mv_ebp_add_rbp_r15,nops,pop-rbp,prefix-3,push-memoff,rbp67,read_const_ptr,rep_tests,rex_invalid,rex_not_last,rip-relative,segment_assign,stosd,stosd67,stosd-bad,stosdno67,sub-add-rsp,sub-rsp,ud2,valid_and_store,valid_base_only,valid_lea_store,x87,add_rsp_r15,addrex2,ambig-segment,bad66,fs_use,inc67,mov-lea-rbp-bad-5,nacl_illegal,rip67,segment_store,change-subregs,ambig-segment',
      help='a comma-separated list of tests')
  parser.add_option(
      '-a', '--gas', dest='gas',
      default=None,
      help='path to assembler')
  parser.add_option(
      '-d', '--decoder', dest='decoder',
      default=None,
      help='path to decoder')
  parser.add_option(
      '-v', '--validator', dest='validator',
      default=None,
      help='path to validator')
  parser.add_option(
      '-p', '--tmp', dest='tmp',
      default=None,
      help='a directory for storing temporary files')
  opt, args = parser.parse_args()
  if (args or
      not opt.tmp or
      not opt.gas or
      not opt.decoder or
      not opt.validator):
    parser.error('invalid arguments')
  no_failures = True
  tester = TestRunner(opt.tmp, opt.gas, opt.decoder, opt.validator)
  for tst in string.split(opt.tests, ','):
    if tester.RunTest(tst):
      print '%s: PASS' % tst
    else:
      print '%s: FAIL' % tst
      no_failures = False
  if no_failures:
    print 'All tests PASSed'
  else:
    print 'Some tests FAILed'
    return 1
  return 0


if __name__ == '__main__':
  sys.exit(Main())
