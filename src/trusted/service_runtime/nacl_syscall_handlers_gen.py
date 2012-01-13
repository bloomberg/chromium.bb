#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
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


SYSCALL_LIST = [
    ('NaClSysNull', []),
    ('NaClSysNameService', ['int *desc_in_out']),
    ('NaClSysDup', ['int oldfd']),
    ('NaClSysDup2', ['int oldfd', 'int newfd']),
    ('NaClSysOpen', ['char *pathname', 'int flags', 'int mode']),
    ('NaClSysClose', ['int d']),
    ('NaClSysRead', ['int d', 'void *buf', 'size_t count']),
    ('NaClSysWrite', ['int d', 'void *buf', 'size_t count']),
    ('NaClSysLseek', ['int d', 'nacl_abi_off_t *offp', 'int whence']),
    ('NaClSysIoctl', ['int d', 'int request', 'void *arg']),
    ('NaClSysFstat', ['int d', 'struct nacl_abi_stat *nasp']),
    ('NaClSysStat', ['const char *path', 'struct nacl_abi_stat *nasp']),
    ('NaClSysGetdents', ['int d', 'void *buf', 'size_t count']),
    ('NaClSysSysbrk', ['void *new_break']),
    ('NaClSysMmap', ['void *start', 'size_t length', 'int prot',
                     'int flags', 'int d', 'nacl_abi_off_t *offp']),
    ('NaClSysMunmap', ['void *start', 'size_t length']),
    ('NaClSysExit', ['int status']),
    ('NaClSysGetpid', []),
    ('NaClSysThread_Exit', ['int32_t *stack_flag']),
    ('NaClSysGetTimeOfDay', ['struct nacl_abi_timeval *tv',
                             'struct nacl_abi_timezone *tz']),
    ('NaClSysClock', []),
    ('NaClSysNanosleep', ['struct nacl_abi_timespec *req',
                          'struct nacl_abi_timespec *rem']),
    ('NaClSysImc_MakeBoundSock', ['int32_t *sap']),
    ('NaClSysImc_Accept', ['int d']),
    ('NaClSysImc_Connect', ['int d']),
    ('NaClSysImc_Sendmsg', ['int d', 'struct NaClAbiNaClImcMsgHdr *nanimhp',
                            'int flags']),
    ('NaClSysImc_Recvmsg', ['int d', 'struct NaClAbiNaClImcMsgHdr *nanimhp',
                            'int flags']),
    ('NaClSysImc_Mem_Obj_Create', ['size_t size']),
    ('NaClSysTls_Init', ['void *thread_ptr']),
    ('NaClSysThread_Create', ['void *prog_ctr', 'void *stack_ptr',
                              'void *thread_ptr', 'void *second_thread_ptr']),
    ('NaClSysTls_Get', []),
    ('NaClSysThread_Nice', ['const int nice']),
    ('NaClSysMutex_Create', []),
    ('NaClSysMutex_Lock', ['int32_t mutex_handle']),
    ('NaClSysMutex_Unlock', ['int32_t mutex_handle']),
    ('NaClSysMutex_Trylock', ['int32_t mutex_handle']),
    ('NaClSysCond_Create', []),
    ('NaClSysCond_Wait', ['int32_t cond_handle', 'int32_t mutex_handle']),
    ('NaClSysCond_Signal', ['int32_t cond_handle']),
    ('NaClSysCond_Broadcast', ['int32_t cond_handle']),
    ('NaClSysCond_Timed_Wait_Abs', ['int32_t cond_handle',
                                    'int32_t mutex_handle',
                                    'struct nacl_abi_timespec *ts']),
    ('NaClSysImc_SocketPair', ['int32_t *d_out']),
    ('NaClSysSem_Create', ['int32_t init_value']),
    ('NaClSysSem_Wait', ['int32_t sem_handle']),
    ('NaClSysSem_Post', ['int32_t sem_handle']),
    ('NaClSysSem_Get_Value', ['int32_t sem_handle']),
    ('NaClSysSched_Yield', []),
    ('NaClSysSysconf', ['int32_t name', 'int32_t *result']),
    ('NaClSysDyncode_Create', ['uint32_t dest', 'uint32_t src',
                               'uint32_t size']),
    ('NaClSysDyncode_Modify', ['uint32_t dest', 'uint32_t src',
                               'uint32_t size']),
    ('NaClSysDyncode_Delete', ['uint32_t dest', 'uint32_t size']),
    ('NaClSysSecond_Tls_Set', ['uint32_t new_value']),
    ('NaClSysSecond_Tls_Get', []),
    ('NaClSysException_Handler', ['uint32_t handler_addr',
                                  'uint32_t old_handler']),
    ('NaClSysException_Stack', ['uint32_t stack_addr', 'uint32_t stack_size']),
    ('NaClSysException_Clear_Flag', []),
    ('NaClSysTest_InfoLeak', []),
    ]


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
               'arglist' : ArgList(architecture, alist),
               'members' : MemoryArgStruct(architecture, name, alist),
               }
    print >>ostr, IMPLEMENTATION_SKELETON % values


def CanonicalizeSpaces(string):
  return ' '.join(string.split())


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
  args = [CanonicalizeSpaces(a) for a in s.split(",")]
  return args


def ParseFileToBeWrapped(fin):
  protos = []
  for line in fin:
    match = re.search(r"^int32_t (NaClSys[_a-zA-Z0-9]+)[(](.*)$", line)
    if not match:
      continue
    name = match.group(1)
    args = GetProtoArgs(match.group(2), fin)
    # We discard the first argument because it is always
    # "struct NaClAppThread *natp".
    protos.append((name, args[1:]))
  return protos


def main(argv):
  usage='Usage: nacl_syscall_handlers_gen.py [-f regex] [-c] [-d] [-a arch]'
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
  # TODO(mseaborn): Make SYSCALL_LIST the authoritative list of system
  # calls, and remove the code for scraping the C files.
  assert protos == SYSCALL_LIST, protos
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
