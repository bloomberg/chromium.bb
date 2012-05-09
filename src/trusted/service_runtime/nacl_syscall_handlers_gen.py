#!/usr/bin/python
# Copyright (c) 2012 The Native Client Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.
#
#  This script produces wrapped versions of syscall implementation
#  functions.  The wrappers extract syscall arguments from where the
#  syscall trampoline saves them.
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
    nacl_syscall[i].handler = &NaClSysNotImplementedDecoder;
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

/*
 * Check that the function being wrapped has the same type as the type
 * declared in SYSCALL_LIST.
 */
static INLINE void AssertSameType_%(name)s() {
  /* This assignment will give an error if the argument types don't match. */
  int32_t (*dummy)(%(arg_type_list)s) = %(name)s;
  /* 'dummy' is not actually a parameter but this suppresses the warning. */
  UNREFERENCED_PARAMETER(dummy);
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
    ('NACL_sys_null', 'NaClSysNull', []),
    ('NACL_sys_nameservice', 'NaClCommonSysNameService', ['int *desc_in_out']),
    ('NACL_sys_dup', 'NaClCommonSysDup', ['int oldfd']),
    ('NACL_sys_dup2', 'NaClCommonSysDup2', ['int oldfd', 'int newfd']),
    ('NACL_sys_open', 'NaClCommonSysOpen',
     ['char *pathname', 'int flags', 'int mode']),
    ('NACL_sys_close', 'NaClCommonSysClose', ['int d']),
    ('NACL_sys_read', 'NaClCommonSysRead',
     ['int d', 'void *buf', 'size_t count']),
    ('NACL_sys_write', 'NaClCommonSysWrite',
     ['int d', 'void *buf', 'size_t count']),
    ('NACL_sys_lseek', 'NaClCommonSysLseek',
     ['int d', 'nacl_abi_off_t *offp', 'int whence']),
    ('NACL_sys_ioctl', 'NaClCommonSysIoctl',
     ['int d', 'int request', 'void *arg']),
    ('NACL_sys_fstat', 'NaClCommonSysFstat',
     ['int d', 'struct nacl_abi_stat *nasp']),
    ('NACL_sys_stat', 'NaClCommonSysStat',
     ['const char *path', 'struct nacl_abi_stat *nasp']),
    ('NACL_sys_getdents', 'NaClCommonSysGetdents',
     ['int d', 'void *buf', 'size_t count']),
    ('NACL_sys_sysbrk', 'NaClSetBreak', ['uintptr_t new_break']),
    ('NACL_sys_mmap', 'NaClCommonSysMmap',
     ['void *start', 'size_t length', 'int prot',
      'int flags', 'int d', 'nacl_abi_off_t *offp']),
    ('NACL_sys_munmap', 'NaClSysMunmap', ['void *start', 'size_t length']),
    ('NACL_sys_exit', 'NaClCommonSysExit', ['int status']),
    ('NACL_sys_getpid', 'NaClSysGetpid', []),
    ('NACL_sys_thread_exit', 'NaClCommonSysThreadExit',
     ['int32_t *stack_flag']),
    ('NACL_sys_gettimeofday', 'NaClSysGetTimeOfDay',
     ['struct nacl_abi_timeval *tv', 'struct nacl_abi_timezone *tz']),
    ('NACL_sys_clock', 'NaClSysClock', []),
    ('NACL_sys_nanosleep', 'NaClSysNanosleep',
     ['struct nacl_abi_timespec *req', 'struct nacl_abi_timespec *rem']),
    ('NACL_sys_clock_getres', 'NaClCommonSysClockGetRes',
     ['int clk_id', 'uint32_t tsp']),
    ('NACL_sys_clock_gettime', 'NaClCommonSysClockGetTime',
     ['int clk_id', 'uint32_t tsp']),
    ('NACL_sys_imc_makeboundsock', 'NaClCommonSysImc_MakeBoundSock',
     ['int32_t *sap']),
    ('NACL_sys_imc_accept', 'NaClCommonSysImc_Accept', ['int d']),
    ('NACL_sys_imc_connect', 'NaClCommonSysImc_Connect', ['int d']),
    ('NACL_sys_imc_sendmsg', 'NaClCommonSysImc_Sendmsg',
     ['int d', 'struct NaClAbiNaClImcMsgHdr *nanimhp', 'int flags']),
    ('NACL_sys_imc_recvmsg', 'NaClCommonSysImc_Recvmsg',
     ['int d', 'struct NaClAbiNaClImcMsgHdr *nanimhp', 'int flags']),
    ('NACL_sys_imc_mem_obj_create', 'NaClCommonSysImc_Mem_Obj_Create',
     ['size_t size']),
    ('NACL_sys_tls_init', 'NaClCommonSysTls_Init', ['void *thread_ptr']),
    ('NACL_sys_thread_create', 'NaClCommonSysThread_Create',
     ['void *prog_ctr', 'void *stack_ptr',
      'void *thread_ptr', 'void *second_thread_ptr']),
    ('NACL_sys_tls_get', 'NaClCommonSysTlsGet', []),
    ('NACL_sys_thread_nice', 'NaClCommonSysThread_Nice', ['const int nice']),
    ('NACL_sys_mutex_create', 'NaClCommonSysMutex_Create', []),
    ('NACL_sys_mutex_lock', 'NaClCommonSysMutex_Lock',
     ['int32_t mutex_handle']),
    ('NACL_sys_mutex_unlock', 'NaClCommonSysMutex_Unlock',
     ['int32_t mutex_handle']),
    ('NACL_sys_mutex_trylock', 'NaClCommonSysMutex_Trylock',
     ['int32_t mutex_handle']),
    ('NACL_sys_cond_create', 'NaClCommonSysCond_Create', []),
    ('NACL_sys_cond_wait', 'NaClCommonSysCond_Wait',
     ['int32_t cond_handle', 'int32_t mutex_handle']),
    ('NACL_sys_cond_signal', 'NaClCommonSysCond_Signal',
     ['int32_t cond_handle']),
    ('NACL_sys_cond_broadcast', 'NaClCommonSysCond_Broadcast',
     ['int32_t cond_handle']),
    ('NACL_sys_cond_timed_wait_abs', 'NaClCommonSysCond_Timed_Wait_Abs',
     ['int32_t cond_handle', 'int32_t mutex_handle',
      'struct nacl_abi_timespec *ts']),
    ('NACL_sys_imc_socketpair', 'NaClCommonSysImc_SocketPair',
     ['uint32_t descs_out']),
    ('NACL_sys_sem_create', 'NaClCommonSysSem_Create', ['int32_t init_value']),
    ('NACL_sys_sem_wait', 'NaClCommonSysSem_Wait', ['int32_t sem_handle']),
    ('NACL_sys_sem_post', 'NaClCommonSysSem_Post', ['int32_t sem_handle']),
    ('NACL_sys_sem_get_value', 'NaClCommonSysSem_Get_Value',
     ['int32_t sem_handle']),
    ('NACL_sys_sched_yield', 'NaClSysSched_Yield', []),
    ('NACL_sys_sysconf', 'NaClSysSysconf', ['int32_t name', 'int32_t *result']),
    ('NACL_sys_dyncode_create', 'NaClTextSysDyncode_Create',
     ['uint32_t dest', 'uint32_t src', 'uint32_t size']),
    ('NACL_sys_dyncode_modify', 'NaClTextSysDyncode_Modify',
     ['uint32_t dest', 'uint32_t src', 'uint32_t size']),
    ('NACL_sys_dyncode_delete', 'NaClTextSysDyncode_Delete',
     ['uint32_t dest', 'uint32_t size']),
    ('NACL_sys_second_tls_set', 'NaClSysSecond_Tls_Set',
     ['uint32_t new_value']),
    ('NACL_sys_second_tls_get', 'NaClSysSecond_Tls_Get', []),
    ('NACL_sys_exception_handler', 'NaClCommonSysException_Handler',
     ['uint32_t handler_addr', 'uint32_t old_handler']),
    ('NACL_sys_exception_stack', 'NaClCommonSysException_Stack',
     ['uint32_t stack_addr', 'uint32_t stack_size']),
    ('NACL_sys_exception_clear_flag', 'NaClCommonSysException_Clear_Flag', []),
    ('NACL_sys_test_infoleak', 'NaClCommonSysTest_InfoLeak', []),
    ('NACL_sys_test_crash', 'NaClSysTestCrash', ['int crash_type']),
    ]


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
    return '  NaClCopyInDropLock(natp->nap);'

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
  } p;
  if (!NaClCopyInFromUserAndDropLock(natp->nap, &p, natp->usr_syscall_args,
                                     sizeof p)) {
    return -NACL_ABI_EFAULT;
  }

""" % values


def PrintSyscallTableInitializer(protos, ostr):
  assign = []
  for syscall_number, func_name, alist in protos:
    assign.append("  NaClAddSyscall(%s, &%sDecoder);" %
                  (syscall_number, func_name))
  print >>ostr, TABLE_INITIALIZER % "\n".join(assign)


def PrintImplSkel(architecture, protos, ostr):
  print >>ostr, AUTOGEN_COMMENT;
  for syscall_number, func_name, alist in protos:
    values = { 'name' : func_name,
               'arglist' : ArgList(architecture, alist),
               'arg_type_list' :
                   ', '.join(['struct NaClAppThread *natp'] + alist),
               'members' : MemoryArgStruct(architecture, func_name, alist),
               }
    print >>ostr, IMPLEMENTATION_SKELETON % values


def main(argv):
  usage = 'Usage: nacl_syscall_handlers_gen.py [-f regex] [-a arch]'
  input_src = sys.stdin
  output_dst = sys.stdout
  arch = 'x86'
  subarch = '32'
  try:
    opts, pargs = getopt.getopt(argv[1:], 'a:i:o:s:')
  except getopt.error, e:
    print >>sys.stderr, 'Illegal option:', str(e)
    print >>sys.stderr, usage
    return 1

  for opt, val in opts:
    if opt == '-a':
      arch = val
    elif opt == '-i':
      input_src = open(val, 'r')
    elif opt == '-o':
      output_dst = open(val, 'w')
    elif opt == '-s':
      subarch = val
    else:
      raise Exception('Unknown option: %s' % opt)

  if subarch != '':
    arch = arch + '-' + subarch

  if not ARG_REGISTERS.has_key(arch):
    raise Exception()

  data = input_src.read()
  protos = SYSCALL_LIST
  print >>output_dst, data
  PrintImplSkel(arch, protos, output_dst)
  PrintSyscallTableInitializer(protos, output_dst)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
