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

AUTOGEN_HEADER = """\
/*
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 *
 * Automatically generated code.  See nacl_syscall_handlers_gen.py
 *
 * NaCl Server Runtime Service Call abstractions
 */

#include "native_client/src/trusted/service_runtime/nacl_syscall_common.h"
#include "native_client/src/trusted/service_runtime/sys_exception.h"
#include "native_client/src/trusted/service_runtime/sys_fdio.h"
#include "native_client/src/trusted/service_runtime/sys_filename.h"
#include "native_client/src/trusted/service_runtime/sys_imc.h"
#include "native_client/src/trusted/service_runtime/sys_list_mappings.h"
#include "native_client/src/trusted/service_runtime/sys_memory.h"
#include "native_client/src/trusted/service_runtime/sys_parallel_io.h"

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
static INLINE void AssertSameType_%(name)s(void) {
  /* This assignment will give an error if the argument types don't match. */
  int32_t (*dummy)(%(arg_type_list)s) = %(name)s;
  /* 'dummy' is not actually a parameter but this suppresses the warning. */
  UNREFERENCED_PARAMETER(dummy);
}
"""

# Our syscall handling code, in nacl_syscall.S, always pushes the
# syscall arguments onto the untrusted user stack.  The syscall
# arguments get snapshotted from there into a per-syscall structure,
# to eliminate time-of-check vs time-of-use issues -- we always only
# refer to syscall arguments from this snapshot, rather than from the
# untrusted memory locations.


SYSCALL_LIST = [
    ('NACL_sys_null', 'NaClSysNull', []),
    ('NACL_sys_nameservice', 'NaClSysNameService', ['uint32_t desc_in_out']),
    ('NACL_sys_dup', 'NaClSysDup', ['int oldfd']),
    ('NACL_sys_dup2', 'NaClSysDup2', ['int oldfd', 'int newfd']),
    ('NACL_sys_open', 'NaClSysOpen',
     ['uint32_t path', 'int flags', 'int mode']),
    ('NACL_sys_close', 'NaClSysClose', ['int d']),
    ('NACL_sys_read', 'NaClSysRead',
     ['int d', 'uint32_t buf', 'uint32_t count']),
    ('NACL_sys_write', 'NaClSysWrite',
     ['int d', 'uint32_t buf', 'uint32_t count']),
    ('NACL_sys_lseek', 'NaClSysLseek',
     ['int d', 'uint32_t offp', 'int whence']),
    ('NACL_sys_fstat', 'NaClSysFstat',
     ['int d', 'uint32_t nasp']),
    ('NACL_sys_stat', 'NaClSysStat', ['uint32_t path', 'uint32_t nasp']),
    ('NACL_sys_getdents', 'NaClSysGetdents',
     ['int d', 'uint32_t buf', 'size_t count']),
    ('NACL_sys_isatty', 'NaClSysIsatty', ['int d']),
    ('NACL_sys_brk', 'NaClSysBrk', ['uintptr_t new_break']),
    ('NACL_sys_mmap', 'NaClSysMmap',
     ['void *start', 'size_t length', 'int prot',
      'int flags', 'int d', 'nacl_abi_off_t *offp']),
    ('NACL_sys_mprotect', 'NaClSysMprotect',
     ['uint32_t start', 'size_t length', 'int prot']),
    ('NACL_sys_list_mappings', 'NaClSysListMappings',
     ['uint32_t regions', 'uint32_t count']),
    ('NACL_sys_munmap', 'NaClSysMunmap', ['void *start', 'uint32_t length']),
    ('NACL_sys_exit', 'NaClSysExit', ['int status']),
    ('NACL_sys_getpid', 'NaClSysGetpid', []),
    ('NACL_sys_thread_exit', 'NaClSysThreadExit',
     ['uint32_t stack_flag_addr']),
    ('NACL_sys_gettimeofday', 'NaClSysGetTimeOfDay', ['uint32_t tv_addr']),
    ('NACL_sys_clock', 'NaClSysClock', []),
    ('NACL_sys_nanosleep', 'NaClSysNanosleep',
     ['struct nacl_abi_timespec *req', 'struct nacl_abi_timespec *rem']),
    ('NACL_sys_clock_getres', 'NaClSysClockGetRes',
     ['int clk_id', 'uint32_t tsp']),
    ('NACL_sys_clock_gettime', 'NaClSysClockGetTime',
     ['int clk_id', 'uint32_t tsp']),
    ('NACL_sys_mkdir', 'NaClSysMkdir', ['uint32_t path', 'int mode']),
    ('NACL_sys_rmdir', 'NaClSysRmdir', ['uint32_t path']),
    ('NACL_sys_chdir', 'NaClSysChdir', ['uint32_t path']),
    ('NACL_sys_getcwd', 'NaClSysGetcwd', ['uint32_t buffer', 'int len']),
    ('NACL_sys_unlink', 'NaClSysUnlink', ['uint32_t path']),
    ('NACL_sys_truncate', 'NaClSysTruncate',
     ['uint32_t path', 'uint32_t length_addr']),
    ('NACL_sys_lstat', 'NaClSysLstat', ['uint32_t path', 'uint32_t nasp']),
    ('NACL_sys_link', 'NaClSysLink',
     ['uint32_t oldname', 'uint32_t newname']),
    ('NACL_sys_rename', 'NaClSysRename',
     ['uint32_t oldname', 'uint32_t newname']),
    ('NACL_sys_symlink', 'NaClSysSymlink',
     ['uint32_t oldname', 'uint32_t newname']),
    ('NACL_sys_chmod', 'NaClSysChmod',
     ['uint32_t path', 'nacl_abi_mode_t mode']),
    ('NACL_sys_access', 'NaClSysAccess', ['uint32_t path', 'int amode']),
    ('NACL_sys_readlink', 'NaClSysReadlink',
     ['uint32_t path', 'uint32_t buffer', 'uint32_t buffer_size']),
    ('NACL_sys_utimes', 'NaClSysUtimes',
     ['uint32_t filename', 'uint32_t times']),
    ('NACL_sys_pread', 'NaClSysPRead',
     ['int32_t d', 'uint32_t usr_addr', 'uint32_t buffer_bytes',
      'uint32_t offset_addr']),
    ('NACL_sys_pwrite', 'NaClSysPWrite',
     ['int32_t d', 'uint32_t usr_addr', 'uint32_t buffer_bytes',
      'uint32_t offset_addr']),
    ('NACL_sys_imc_makeboundsock', 'NaClSysImcMakeBoundSock',
     ['uint32_t descs_addr']),
    ('NACL_sys_imc_accept', 'NaClSysImcAccept', ['int d']),
    ('NACL_sys_imc_connect', 'NaClSysImcConnect', ['int d']),
    ('NACL_sys_imc_sendmsg', 'NaClSysImcSendmsg',
     ['int d', 'uint32_t nanimhp', 'int flags']),
    ('NACL_sys_imc_recvmsg', 'NaClSysImcRecvmsg',
     ['int d', 'uint32_t nanimhp', 'int flags']),
    ('NACL_sys_imc_mem_obj_create', 'NaClSysImcMemObjCreate',
     ['size_t size']),
    ('NACL_sys_tls_init', 'NaClSysTlsInit', ['uint32_t thread_ptr']),
    ('NACL_sys_thread_create', 'NaClSysThreadCreate',
     ['uint32_t prog_ctr', 'uint32_t stack_ptr',
      'uint32_t thread_ptr', 'uint32_t second_thread_ptr']),
    ('NACL_sys_tls_get', 'NaClSysTlsGet', []),
    ('NACL_sys_thread_nice', 'NaClSysThreadNice', ['const int nice']),
    ('NACL_sys_mutex_create', 'NaClSysMutexCreate', []),
    ('NACL_sys_mutex_lock', 'NaClSysMutexLock',
     ['int32_t mutex_handle']),
    ('NACL_sys_mutex_unlock', 'NaClSysMutexUnlock',
     ['int32_t mutex_handle']),
    ('NACL_sys_mutex_trylock', 'NaClSysMutexTrylock',
     ['int32_t mutex_handle']),
    ('NACL_sys_cond_create', 'NaClSysCondCreate', []),
    ('NACL_sys_cond_wait', 'NaClSysCondWait',
     ['int32_t cond_handle', 'int32_t mutex_handle']),
    ('NACL_sys_cond_signal', 'NaClSysCondSignal',
     ['int32_t cond_handle']),
    ('NACL_sys_cond_broadcast', 'NaClSysCondBroadcast',
     ['int32_t cond_handle']),
    ('NACL_sys_cond_timed_wait_abs', 'NaClSysCondTimedWaitAbs',
     ['int32_t cond_handle', 'int32_t mutex_handle', 'uint32_t ts_addr']),
    ('NACL_sys_imc_socketpair', 'NaClSysImcSocketPair',
     ['uint32_t descs_out']),
    ('NACL_sys_sem_create', 'NaClSysSemCreate', ['int32_t init_value']),
    ('NACL_sys_sem_wait', 'NaClSysSemWait', ['int32_t sem_handle']),
    ('NACL_sys_sem_post', 'NaClSysSemPost', ['int32_t sem_handle']),
    ('NACL_sys_sem_get_value', 'NaClSysSemGetValue',
     ['int32_t sem_handle']),
    ('NACL_sys_sched_yield', 'NaClSysSchedYield', []),
    ('NACL_sys_sysconf', 'NaClSysSysconf',
     ['int32_t name', 'uint32_t result_addr']),
    ('NACL_sys_dyncode_create', 'NaClSysDyncodeCreate',
     ['uint32_t dest', 'uint32_t src', 'uint32_t size']),
    ('NACL_sys_dyncode_modify', 'NaClSysDyncodeModify',
     ['uint32_t dest', 'uint32_t src', 'uint32_t size']),
    ('NACL_sys_dyncode_delete', 'NaClSysDyncodeDelete',
     ['uint32_t dest', 'uint32_t size']),
    ('NACL_sys_second_tls_set', 'NaClSysSecondTlsSet',
     ['uint32_t new_value']),
    ('NACL_sys_second_tls_get', 'NaClSysSecondTlsGet', []),
    ('NACL_sys_exception_handler', 'NaClSysExceptionHandler',
     ['uint32_t handler_addr', 'uint32_t old_handler']),
    ('NACL_sys_exception_stack', 'NaClSysExceptionStack',
     ['uint32_t stack_addr', 'uint32_t stack_size']),
    ('NACL_sys_exception_clear_flag', 'NaClSysExceptionClearFlag', []),
    ('NACL_sys_test_infoleak', 'NaClSysTestInfoLeak', []),
    ('NACL_sys_test_crash', 'NaClSysTestCrash', ['int crash_type']),
    ('NACL_sys_futex_wait_abs', 'NaClSysFutexWaitAbs',
     ['uint32_t addr', 'uint32_t value', 'uint32_t abstime_ptr']),
    ('NACL_sys_futex_wake', 'NaClSysFutexWake',
     ['uint32_t addr', 'uint32_t nwake']),
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
  for arg in alist:
    t = ExtractType(arg)
    extra_cast = ''
    # avoid cast to pointer from integer of different size
    if TypeIsPointer(t):
      extra_cast = '(uintptr_t)'
    extractedargs += ['(' + t + ') ' + extra_cast
                      + ' p.' + ExtractVariable(arg)]

  return ', ' + ', '.join(extractedargs)


def MemoryArgStruct(architecture, name, alist):
  if not alist:
    return '  NaClCopyDropLock(natp->nap);'

  margs = ['    uint32_t %s' % ExtractVariable(arg)
           for arg in alist]
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
    # These inlines are no-ops that should be optimized away.
    # Emit the call just so that they are not reported as unused.
    assign.append("  AssertSameType_%s();" % func_name)
  print >>ostr, TABLE_INITIALIZER % "\n".join(assign)


def PrintImplSkel(architecture, protos, ostr):
  print >>ostr, AUTOGEN_HEADER
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

  # Check naming consistency.
  for syscall_number, func_name, alist in SYSCALL_LIST:
    assert syscall_number.startswith('NACL_sys_'), syscall_number
    name1 = syscall_number[len('NACL_sys_'):].replace('_', '')
    assert func_name.startswith('NaClSys'), func_name
    name2 = func_name[len('NaClSys'):].lower()
    assert name1 == name2, '%r != %r' % (name1, name2)

  data = input_src.read()
  protos = SYSCALL_LIST
  print >>output_dst, data
  PrintImplSkel(arch, protos, output_dst)
  PrintSyscallTableInitializer(protos, output_dst)

  return 0


if __name__ == '__main__':
  sys.exit(main(sys.argv))
