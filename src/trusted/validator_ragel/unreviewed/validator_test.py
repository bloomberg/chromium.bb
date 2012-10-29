# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

# This validator test was taken from branch "x86-64" in the repo:
#   https://github.com/mseaborn/x86-decoder
# The test was modified to:
#   * address intentional validator differences
#   * to take the external validating executable as a command-line argument

import optparse
import os
import subprocess


def WriteFile(filename, data):
  fh = open(filename, "w")
  try:
    fh.write(data)
  finally:
    fh.close()


BITS = 64
TEST_CASES = []
NCVAL_EXECUTABLE = None


def TestCase(asm, accept):
  def Func():
    print '* test %r' % asm
    full_asm = asm + '\n.p2align 5, 0x90\n'
    try:
      WriteFile('scons-out/tmp.S', full_asm)
      subprocess.check_call(['gcc', '-m%i' % BITS, '-c', 'scons-out/tmp.S',
                             '-o', 'scons-out/tmp.o'])
      rc = subprocess.call([NCVAL_EXECUTABLE, 'scons-out/tmp.o'])
    finally:
      os.unlink('scons-out/tmp.S')
      os.unlink('scons-out/tmp.o')
    if accept:
      assert rc == 0, rc
    else:
      assert rc == 1, rc
  TEST_CASES.append(Func)


# Check some simple allowed instructions.
TestCase(accept=True, asm="""
nop
hlt
mov $0x12345678, %rax
mov $0x1234567812345678, %rax
""")

# Check a disallowed instruction.
TestCase(accept=False, asm="""
nop
int $0x80
""")

TestCase(accept=False, asm='ret')

TestCase(accept=False, asm='syscall')

# Instruction bundle overflow.
TestCase(accept=False, asm="""
mov $0x1234567812345678, %rax
mov $0x1234567812345678, %rax
mov $0x1234567812345678, %rax
mov $0x1234567812345678, %rax
""")

# Forwards and backwards jumps.
TestCase(accept=True, asm="""
nop
jmp label2
label1:
jmp label1
jmp label1
label2:
jmp label1
""")

# Out-of-range unaligned jump.
TestCase(accept=False, asm="""
label:
jmp label - 1
""")

# Out-of-range unaligned jump.
TestCase(accept=False, asm="""
jmp label + 1
.p2align 5
label:
""")

# Jump into instruction.
TestCase(accept=False, asm="""
label:
mov $0x1234567812345678, %rax
jmp label + 1
""")


# Unmasked indirect jumps are disallowed.
TestCase(accept=False, asm='jmp *%rax')
TestCase(accept=False, asm='jmp *(%rax)')
TestCase(accept=False, asm='call *%rax')
TestCase(accept=False, asm='call *(%rax)')

# Masking instructions on their own are allowed.
TestCase(accept=True, asm='and $~31, %eax')
TestCase(accept=True, asm='and $~31, %ebx')
TestCase(accept=True, asm='and $~31, %rax')
TestCase(accept=True, asm='and $~31, %rbx')
TestCase(accept=True, asm='and $~31, %eax; add %r15, %rax')
TestCase(accept=True, asm='and $~31, %ebx; add %r15, %rbx')

# Masked indirect jumps are allowed.
TestCase(accept=True, asm='and $~31, %eax; add %r15, %rax; jmp *%rax')
TestCase(accept=True, asm='and $~31, %ebx; add %r15, %rbx; call *%rbx')

# The registers must match up for the mask and the jump.
TestCase(accept=False, asm='and $~31, %ebx; add %r15, %rax; jmp *%rax')
TestCase(accept=False, asm='and $~31, %eax; add %r15, %rbx; jmp *%rax')
TestCase(accept=False, asm='and $~31, %eax; add %r15, %rax; jmp *%rbx')
TestCase(accept=False, asm='and $~31, %eax; add %r15, %rbx; jmp *%rbx')
TestCase(accept=False, asm='and $~31, %ebx; add %r15, %rbx; jmp *%rax')

# The mask and the jump must be adjacent.
TestCase(accept=False, asm='and $~31, %eax; nop; add %r15, %rax; jmp *%rax')
TestCase(accept=False, asm='and $~31, %eax; add %r15, %rax; nop; jmp *%rax')

# Jumping into the middle of the superinstruction must be rejected.
TestCase(accept=False, asm="""
and $~31, %eax
add %r15, %rax
label:
jmp *%rax
jmp label
""")
TestCase(accept=False, asm="""
and $~31, %eax
label:
add %r15, %rax
jmp *%rax
jmp label
""")

# Read-only access to special registers is allowed.
TestCase(accept=True, asm='push %rax')
TestCase(accept=True, asm='push %rbp')
TestCase(accept=True, asm='push %rsp')
TestCase(accept=True, asm='push %r15')
TestCase(accept=True, asm='mov %rsp, %rax')
# But write access is not.
TestCase(accept=True, asm='pop %rax')
TestCase(accept=False, asm='pop %rbp')
TestCase(accept=False, asm='pop %rsp')
TestCase(accept=False, asm='pop %r15')

# Memory accesses.
TestCase(accept=True, asm="""
mov %eax, %eax
mov (%r15, %rax), %ebx
""")
# Test for a top-bit-set register.
TestCase(accept=True, asm="""
mov %r12d, %r12d
mov (%r15, %r12), %ebx
""")
# Check %edi and %esi because the first 'mov' also begins superinstructions.
TestCase(accept=True, asm="""
mov %edi, %edi
mov (%r15, %rdi), %ebx
""")
TestCase(accept=True, asm="""
mov %esi, %esi
mov (%r15, %rsi), %ebx
""")
# Check mask on its own.
TestCase(accept=True, asm="""
mov %eax, %eax
""")
TestCase(accept=False, asm="""
mov (%r15, %rax), %ebx
""")
TestCase(accept=False, asm="""
mov %eax, %eax
label:
mov (%r15, %rax), %ebx
jmp label
""")

# Check that post-conditions do not leak from a superinstruction.  To
# share DFT states, the first instruction of the nacljmp, "and $~31,
# %eax", records a post-condition, just as when it is used on its own.
# Although the code below is safe, we don't really want the
# post-condition to leak through.
TestCase(accept=False, asm="""
and $~31, %eax
add %r15, %rax
jmp *%rax
// %rax should not be regarded as zero-extended here.
mov (%r15, %rax), %ebx
""")
TestCase(accept=False, asm="""
mov %edi, %edi
lea (%r15, %rdi), %rdi
rep stos %al, %es:(%rdi)
// %rdi should not be regarded as zero-extended here.
mov (%r15, %rdi), %ebx
""")
TestCase(accept=False, asm="""
mov %esi, %esi
lea (%r15, %rsi), %rsi
mov %edi, %edi
lea (%r15, %rdi), %rdi
rep movsb %ds:(%rsi), %es:(%rdi)
// %rsi should not be regarded as zero-extended here.
mov (%r15, %rsi), %ebx
""")

# Non-%r15-based memory accesses.
TestCase(accept=True, asm='mov 0x1234(%rip), %eax')
TestCase(accept=True, asm='mov 0x1234(%rsp), %eax')
TestCase(accept=True, asm='mov 0x1234(%rbp), %eax')
TestCase(accept=False, asm='mov 0x1234(%rsp, %rbx), %eax')
TestCase(accept=False, asm='mov 0x1234(%rbp, %rbx), %eax')
TestCase(accept=True, asm='mov %ebx, %ebx; mov 0x1234(%rsp, %rbx), %eax')
TestCase(accept=True, asm='mov %ebx, %ebx; mov 0x1234(%rbp, %rbx), %eax')

# 'lea' is not a memory access.
TestCase(accept=True, asm='lea (%rbx, %rcx, 4), %rax')

# Stack operations.
TestCase(accept=True, asm='mov %rsp, %rbp')
TestCase(accept=True, asm='mov %rbp, %rsp')
TestCase(accept=True, asm="""
add $8, %ebp
add %r15, %rbp
""")
TestCase(accept=False, asm="""
add $8, %ebp
label:
add %r15, %rbp
jmp label
""")
# A stack fixup on its own is not allowed.
TestCase(accept=False, asm='add %r15, %rbp')
TestCase(accept=False, asm='add %r15, %rsp')
TestCase(accept=False, asm='add %r15, %r15')

# Sandboxing is not required on prefetch instructions.
TestCase(accept=True, asm='prefetchnta (%rax)')

# Segment registers manipulations are forbidden
TestCase(accept=False, asm='mov %rax, %es')
TestCase(accept=False, asm='mov %es, %rax')


def Main():
  global NCVAL_EXECUTABLE
  parser = optparse.OptionParser()
  (options, args) = parser.parse_args()
  assert len(args) == 1
  NCVAL_EXECUTABLE = args[0]
  for test_case in TEST_CASES:
    test_case()
  print 'PASS'


if __name__ == '__main__':
  Main()
