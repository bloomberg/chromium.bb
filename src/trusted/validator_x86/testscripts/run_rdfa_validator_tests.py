#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import glob
import optparse
import os
import re
import struct
import subprocess
import sys
import tempfile

import test_format


BUNDLE_SIZE = 32


def AssertEquals(actual, expected):
  if actual != expected:
    raise AssertionError('\nEXPECTED:\n"""\n%s"""\n\nACTUAL:\n"""\n%s"""'
                         % (expected, actual))


def ParseHex(hex_content):
  """Parse content of @hex section and return binary data

  Args:
    hex_content: Content of @hex section as a string.

  Yields:
    Chunks of binary data corresponding to lines of given @hex section (as
    strings). If line ends with r'\\', chunk is continued on the following line.
  """

  bytes = []
  for line in hex_content.split('\n'):
    line, sep, comment = line.partition('#')
    line = line.strip()
    if line == '':
      continue

    if line.endswith(r'\\'):
      line = line[:-2]
      continuation = True
    else:
      continuation = False

    for byte in line.split():
      assert len(byte) == 2
      bytes.append(chr(int(byte, 16)))

    if not continuation:
      assert len(bytes) > 0
      yield ''.join(bytes)
      bytes = []

  assert bytes == [], r'r"\\" should not appear on the last line'


def CreateElfContent(bits, text_segment):
  e_ident = {
      32: '\177ELF\1',
      64: '\177ELF\2'}[bits]
  e_machine = {
      32: 3,
      64: 62}[bits]

  e_phoff = 256
  e_phnum = 1
  e_phentsize = 0

  elf_header_fmt = {
      32: '<16sHHIIIIIHHHHHH',
      64: '<16sHHIQQQIHHHHHH'}[bits]

  elf_header = struct.pack(
      elf_header_fmt,
      e_ident, 0, e_machine, 0, 0, e_phoff, 0, 0, 0,
      e_phentsize, e_phnum, 0, 0, 0)

  p_type = 1  # PT_LOAD
  p_flags = 5  # r-x
  p_filesz = len(text_segment)
  p_memsz = p_filesz
  p_vaddr = 0
  p_offset = 512
  p_align = 0
  p_paddr = 0

  pheader_fmt = {
      32: '<IIIIIIII',
      64: '<IIQQQQQQ'}[bits]

  pheader_fields = {
      32: (p_type, p_offset, p_vaddr, p_paddr,
           p_filesz, p_memsz, p_flags, p_align),
      64: (p_type, p_flags, p_offset, p_vaddr,
           p_paddr, p_filesz, p_memsz, p_align)}[bits]

  pheader = struct.pack(pheader_fmt, *pheader_fields)

  result = elf_header
  assert len(result) <= e_phoff
  result += '\0' * (e_phoff - len(result))
  result += pheader
  assert len(result) <= p_offset
  result += '\0' * (p_offset - len(result))
  result += text_segment

  return result


def RunRdfaValidator(options, data):
  # Add nops to make it bundle-sized.
  data += (-len(data) % BUNDLE_SIZE) * '\x90'
  assert len(data) % BUNDLE_SIZE == 0

  tmp = tempfile.NamedTemporaryFile(
      prefix='tmp_legacy_validator_', mode='wb', delete=False)
  try:
    tmp.write(CreateElfContent(options.bits, data))
    tmp.close()

    proc = subprocess.Popen([options.rdfaval, tmp.name],
                            stdout=subprocess.PIPE,
                            stderr=subprocess.PIPE)
    stdout, stderr = proc.communicate()
    assert stderr == '', stderr
    return_code = proc.wait()
  finally:
    tmp.close()
    os.remove(tmp.name)

  # Remove the carriage return characters that we get on Windows.
  stdout = stdout.replace('\r', '')
  return return_code, stdout


def ParseRdfaMessages(stdout):
  """Get (offset, message) pairs from rdfa validator output.

  Args:
    stdout: Output of rdfa validator as string.

  Yields:
    Pairs (offset, message).
  """
  for line in stdout.split('\n'):
    line = line.strip()
    if line == '':
      continue
    if re.match(r"(Valid|Invalid)\.$", line):
      continue

    m = re.match(r'([0-9a-f]+): (.*)$', line, re.IGNORECASE)
    assert m is not None, "can't parse line '%s'" % line
    offset = int(m.group(1), 16)
    message = m.group(2)

    if not message.startswith('warning - '):
      yield offset, message


def RunRdfaWithNopPatching(options, data_chunks):
  r"""Run RDFA validator with NOP patching for better error reporting.

  If the RDFA validator encounters an invalid instruction, it resumes validation
  from the beginning of the next bundle, while the original, non-DFA-based
  validators skip maybe one or two bytes and recover. And there are plenty of
  tests where there are more than one error in a single bundle. To mitigate such
  spurious disagreements, the following procedure is used: when RDFA complaints
  that particular piece can't be decoded, the problematic line in @hex section
  (which usually corresponds to one instruction) is replaced with NOPs and the
  validator is rerun from the beginning. This process may take several
  iterations (it seems it always converges in practice). All errors reported on
  all such runs (sans duplicate ones) are taken as validation result. So, in a
  sense, this trick is to emulate line-level recovery as opposed to bundle-
  level. In practice it turns out ok, and lots of spurious errors are
  eliminated. To each error message we add the stage at which it was produced,
  so we can destinguish 'primary' errors from additional ones.

  Example. Suppose DE AD and BE EF machine codes correspond to invalid
  instructions. Lets take a look at what happens when we invoke
  RunRdfaWithNopPatching(options, ['\de\ad', '\be\ef']). First the RDFA
  validator is run on the code '\de\ad\be\ef\90\90\90...'. It encounters an
  undecipherable instruction, produces an error message at offset zero and
  stops. Now we replace what is at offset zero ('\de\ad') with corresponding
  amount of nops, and run the RDFA validator again on
  '\90\90\be\ef\90\90\90...'. This time it decodes first two NOPs sucessfully
  and reports problem at offset 2. In the next iteration of NOP patching BE EF
  is replaced with 90 90 as well, no decoding errors are reported on the next
  run so the whole process stops. Finally the combined output looks like
  following:

    0: [0] unrecognized instruction  <- produced at stage 0
    2: [1] unrecognized instruction  <- produced at stage 1
    return code: 1                   <- return code at stage 0

  Args:
    options: Options as produced by optparse.
        Relevant fields are .bits and .update.
    data_chunks: List of strings containing binary data. For the described
        heuristic to work better it is desirable (although not absolutelty
        required) that strings correspond to singular instructions, as it
        usually happens in @hex section.

  Returns:
    String representing combined output from all stages. Error messages are
    of the form
      <offset in hex>: [<stage>] <message>
  """

  data_chunks = list(data_chunks)

  offset_to_chunk = {}
  offset = 0
  for i, chunk in enumerate(data_chunks):
    offset_to_chunk[offset] = i
    offset += len(chunk)

  first_return_code = None
  messages = []  # list of triples (offset, stage, message)
  messages_set = set()  # set of pairs (offset, message)
  stage = 0

  while True:
    return_code, stdout = RunRdfaValidator(options, ''.join(data_chunks))
    if first_return_code is None:
      first_return_code = return_code

    nop_patched = False

    for offset, message in ParseRdfaMessages(stdout):
      if (offset, message) in messages_set:
        continue
      messages.append((offset, stage, message))
      messages_set.add((offset, message))

      if offset in offset_to_chunk and message == 'unrecognized instruction':
        chunk_no = offset_to_chunk[offset]
        nops_chunk = '\x90' * len(data_chunks[chunk_no])
        if nops_chunk != data_chunks[chunk_no]:
          data_chunks[chunk_no] = nops_chunk
          nop_patched = True

    if not nop_patched:
      break
    stage += 1

  messages.sort(key=lambda (offset, stage, _): (offset, stage))

  result = ''.join('%x: [%d] %s\n' % (offset, stage, message)
                   for offset, stage, message in messages)
  result += 'return code: %d\n' % first_return_code
  return result


def CheckValidJumpTargets(options, data_chunks):
  """
  Check that the validator infers valid jump targets correctly.

  This test checks that the validator identifies instruction boundaries and
  superinstructions correctly. In order to do that, it attempts to append a jump
  to each byte at the end of the given code. Jump should be valid if and only if
  it goes to the boundary between data chunks.

  Note that the same chunks as in RunRdfaWithNopPatching are used, but here they
  play a different role. In RunRdfaWithNopPatching the partitioning into chunks
  is only relevant when the whole snippet is invalid. Here, on the other hand,
  we only care about valid snippets, and we use chunks to mark valid jump
  targets.

  Args:
    options: Options as produced by optparse.
    data_chunks: List of strings containing binary data. Each such chunk is
        expected to correspond to indivisible instruction or superinstruction.

  Returns:
    None.
  """
  data = ''.join(data_chunks)
  # Add nops to make it bundle-sized.
  data += (-len(data) % BUNDLE_SIZE) * '\x90'
  assert len(data) % BUNDLE_SIZE == 0

  # Since we check validity of jump target by adding jump and validating
  # resulting piece, we rely on validity of original snippet.
  return_code, _ = RunRdfaValidator(options, data)
  assert return_code == 0, 'Can only validate jump targets on valid snippet'

  valid_jump_targets = set()
  pos = 0
  for data_chunk in data_chunks:
    valid_jump_targets.add(pos)
    pos += len(data_chunk)
  valid_jump_targets.add(pos)

  for i in range(pos + 1):
    # Encode JMP with 32-bit relative target.
    jump = '\xe9' + struct.pack('<i', i - (len(data) + 5))
    return_code, _ = RunRdfaValidator(options, data + jump)
    if return_code == 0:
      assert i in valid_jump_targets, (
          'Offset 0x%x was reported valid jump target' % i)
    else:
      assert i not in valid_jump_targets, (
          'Offset 0x%x was reported invalid jump target' % i)


def Test(options, items_list):
  info = dict(items_list)

  if 'rdfa_output' in info:
    data_chunks = list(ParseHex(info['hex']))
    stdout = RunRdfaWithNopPatching(options, data_chunks)
    print '  Checking rdfa_output field...'
    if options.update:
      if stdout != info['rdfa_output']:
        print '  Updating rdfa_output field...'
        info['rdfa_output'] = stdout
    else:
      AssertEquals(stdout, info['rdfa_output'])

    last_line = re.search('return code: (-?\d+)\n$', info['rdfa_output'])
    expected_return_code = int(last_line.group(1))

    # This test only works for valid snippets, see CheckValidJumpTargets
    # for details.
    if expected_return_code == 0:
      print '  Checking jump targets...'
      CheckValidJumpTargets(options, data_chunks)

  # Update field values, but preserve their order.
  items_list = [(field, info[field]) for field, _ in items_list]

  return items_list


def main(args):
  parser = optparse.OptionParser()
  parser.add_option('--rdfaval', default='validator_test',
                    help='Path to the ncval validator executable')
  parser.add_option('--bits',
                    type=int,
                    help='The subarchitecture to run tests against: 32 or 64')
  parser.add_option('--update',
                    default=False,
                    action='store_true',
                    help='Regenerate golden fields instead of testing')

  options, args = parser.parse_args(args)

  if options.bits not in [32, 64]:
    parser.error('specify --bits 32 or --bits 64')

  if len(args) == 0:
    parser.error('No test files specified')
  processed = 0
  for glob_expr in args:
    test_files = sorted(glob.glob(glob_expr))
    if len(test_files) == 0:
      raise AssertionError(
          '%r matched no files, which was probably not intended' % glob_expr)
    for test_file in test_files:
      print 'Testing %s...' % test_file
      tests = test_format.LoadTestFile(test_file)
      tests = [Test(options, test) for test in tests]
      if options.update:
        test_format.SaveTestFile(tests, test_file)
      processed += 1
  print '%s test files were processed.' % processed


if __name__ == '__main__':
  main(sys.argv[1:])
