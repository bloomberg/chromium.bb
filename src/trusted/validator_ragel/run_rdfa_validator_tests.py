#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

import os
import re
import struct
import subprocess
import sys
import tempfile

import test_format


BUNDLE_SIZE = 32


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

  tmp = tempfile.NamedTemporaryFile(mode='wb', delete=False)
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


class RdfaTestRunner(test_format.TestRunner):

  SECTION_NAME = 'rdfa_output'

  def CommandLineOptions(self, parser):
    parser.add_option('--rdfaval', default='validator_test',
                      help='Path to the ncval validator executable')

  def GetSectionContent(self, options, hex_content):
    data_chunks = list(test_format.ParseHex(hex_content))
    result = RunRdfaWithNopPatching(options, data_chunks)

    last_line = re.search('return code: ((-)?\d+)\n$', result)
    return_code = int(last_line.group(1))

    if return_code == 0:
      print '  Checking jump targets...'
      CheckValidJumpTargets(options, data_chunks)

    return result


def main(argv):
  RdfaTestRunner().Run(argv)


if __name__ == '__main__':
  main(sys.argv[1:])
