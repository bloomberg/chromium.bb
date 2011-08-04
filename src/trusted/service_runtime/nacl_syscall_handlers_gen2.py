#!/usr/bin/python
#
# Copyright 2008 The Native Client Authors.  All rights reserved.  Use
# of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
#  This script extracts  "nacl syscall" prototypes from a c file
#  given on stdin and then produces wrapped versions of the syscalls.
#
#  Note that NaCl modules are always ILP32 and compiled with a
#  compiler that passes all arguments on the stack, but the service
#  runtime may be ILP32, LP64, or P64.  So we cannot use just char *
#  in the structure that we overlay on top of the stack memory to
#  extract system call arguments.  All syscall arguments are extracted
#  as uint32_t first, then cast to the desired type -- pointers are
#  not valid until they have been translated from user addresses to
#  system addresses, and this is the responsibility of the actual
#  system call handlers.
#
import getopt
import re
import StringIO
import sys

AUTOGEN_COMMENT = """\
/*
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 *
 * Automatically generated code.  See nacl_syscall_handlers_gen.py
 *
 * NaCl Server Runtime Service Call abstractions
 */
"""


TABLE_INITIALIZER = """\

/* auto generated */

void NaClSyscallTableInit() {
  int i;
  for (i = 0; i < NACL_MAX_SYSCALLS; ++i) {
     nacl_syscall[i].handler = &NotImplementedDecoder;
  }

%s
}
"""


IMPLEMENTATION_SKELETON = """\
/* this function was automagically generated */
static int32_t %(name)sDecoder(struct NaClAppThread *natp) {
%(members)s\
  return %(name)s(natp%(arglist)s);
}
"""

# Integer/pointer registers used in x86-64 calling convention.  NB:
# the untrusted code is compiled using gcc, and follows the AMD64
# calling covention, not the Windows x64 convention.  The service
# runtime may be compiled with either compiler, so our assembly code
# for context switch must take care to handle the differences.
ARG_REGISTERS = {
    'x86-32': [],
    'x86-64': [],
    # 'x86-64': ['rdi', 'rsi', 'rdx', 'rcx', 'r8', 'r9'],
    'arm-32': [],
    # 'arm-32': [ 'r0',  'r1',  'r2',  'r3'],
    # while the arm calling convention passes arguments in registers,
    # the arm trampoline code pushes them onto the untrusted stack first
    # before transferring control to trusted code.  this uses up some
    # more untrusted stack space / memory/cache bandwidth, but we had
    # to save these arguments somewhere, either in a processor context
    # or on-stack, anyway.
    }

# Our syscall handling code, in nacl_syscall.S, always pushes the
# syscall arguments onto the untrusted user stack.  The syscall
# arguments get snapshotted from there into a per-syscall structure,
# to eliminate time-of-check vs time-of-use issues -- we always only
# refer to syscall arguments from this snapshot, rather than from the
# untrusted memory locations.


def FunctionNameToSyscallDefine(name):
  assert name.startswith("NaClSys")
  name = name[7:]
  return "NACL_sys_" + name.lower()


# Syscall arguments MUST be declared in a simple-to-parse manner!
# They must match the following regexp:

ARG_RE = r'\s*((\w+\s+)+\**)\s*(\w+)'

# where matchobj.group(1) is the type, and matchobj.group(3) is the
# identifer.


def CollapseWhitespaces(s):
  return re.sub(r'\s+', ' ', s)


def ExtractVariable(decl):
  type_or_idents = re.match(ARG_RE, decl)
  return type_or_idents.group(3)


def ExtractType(decl):
  type_or_idents = re.match(ARG_RE, decl)
  return CollapseWhitespaces(type_or_idents.group(1)).strip()


def TypeIsPointer(typestr):
  return '*' in typestr


def ArgList(architecture, alist):
  if not alist:
    return ''

  extractedargs = []
  for argnum, arg in enumerate(alist):
    t = ExtractType(arg)
    extra_cast = ''
    # avoid cast to pointer from integer of different size
    if TypeIsPointer(t):
      extra_cast = '(uintptr_t)'
    if argnum >= len(ARG_REGISTERS[architecture]):
      extractedargs += ['(' + t + ') ' + extra_cast
                        + ' p.' + ExtractVariable(arg)]
    else:
      extractedargs += ['(' + t + ') ' + extra_cast
                        +' natp->user.' +
                        ARG_REGISTERS[architecture][argnum]]

  return ', ' + ', '.join(extractedargs)


def MemoryArgStruct(architecture, name, alist):
  if not alist:
    return ''

  # Note: although this return value might be computed more
  # concisely with a list comprehension, we instead use a
  # for statement to maintain python 2.3 compatibility.
  margs = []
  for argnum, arg in enumerate(alist):
    if argnum >= len(ARG_REGISTERS[architecture]):
      margs += ['    uint32_t %s' % ExtractVariable(arg)]
  values = {
      'name': name,
      'members' : ';\n'.join(margs) + ';'
      }

  if len(margs) == 0:
    return ''

  return """\
  struct %(name)sArgs {
%(members)s
  } p = *(struct %(name)sArgs *) natp->syscall_args;

""" % values


def PrintSyscallTableInitializer(protos, ostr):
  assign = []
  for name, _ in protos:
    syscall = FunctionNameToSyscallDefine(name)
    assign.append("  NaClAddSyscall(%s, &%sDecoder);" % (syscall, name))
  print >>ostr, TABLE_INITIALIZER % "\n".join(assign)


def PrintImplSkel(architecture, protos, ostr):
  print >>ostr, AUTOGEN_COMMENT;
  for name, alist in protos:
    values = { 'name' : name,
               'arglist' : ArgList(architecture, alist[1:]),
               'members' : MemoryArgStruct(architecture, name, alist[1:]),
               }
    print >>ostr, IMPLEMENTATION_SKELETON % values


def GetProtoArgs(s, fin):
  if "{" not in s:
    for line in fin:
      s += line
      if "{" in line:
        break
    else:
      raise Exception('broken input')
  pos = s.rfind(")")
  assert pos >= 0
  # prune stuff after )
  s = s[0:pos]
  args = [a.strip() for a in s.split(",")]
  return args


def ParseFileToBeWrapped(fin):
  protos = []
  for line in fin:
    match = re.search(r"^int32_t (NaClSys[_a-zA-Z0-9]+)[(](.*)$", line)
    if not match:
      continue
    name = match.group(1)
    args = GetProtoArgs(match.group(2), fin)
    protos.append( (name, args) )
  return protos


def main(argv):
  usage='Usage: nacl_syscall_handlers_gen2.py [-f regex] [-c] [-d] [-a arch]'
  mode = "dump"
  input_src = sys.stdin
  output_dst = sys.stdout
  arch = 'x86'
  subarch = '32'
  try:
    opts, pargs = getopt.getopt(argv[1:], 'a:cdi:o:s:')
  except getopt.error, e:
    print >>sys.stderr, 'Illegal option:', str(e)
    print >>sys.stderr, usage
    return 1

  for opt, val in opts:
    if opt == '-a':
      arch = val
    elif opt == '-d':
      mode = "dump"
    elif opt == '-c':
      mode = "codegen"
    elif opt == '-i':
      input_src = open(val, 'r')
    elif opt == '-o':
      output_dst = open(val, 'w')
    elif opt == '-s':
      subarch = val
    else:
      raise Exception('Unknown option: %s' % opt)

  print 'arch =', arch, 'subarch =', subarch
  if subarch != '':
    arch = arch + '-' + subarch

  print 'arch =', arch
  if not ARG_REGISTERS.has_key(arch):
    raise Exception()

  data = input_src.read()
  protos = ParseFileToBeWrapped(StringIO.StringIO(data))
  if mode == "dump":
    for f, a in  protos:
      print >>output_dst, f
      print >>output_dst, "\t", a
  elif mode == "codegen":
    print >>output_dst, data
    PrintImplSkel(arch, protos, output_dst)
    PrintSyscallTableInitializer(protos, output_dst)


  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
