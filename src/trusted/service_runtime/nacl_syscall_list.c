/*
 * Copyright (c) 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/nacl_syscall_common.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_handlers.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_register.h"
#include "native_client/src/trusted/service_runtime/nacl_text.h"
#include "native_client/src/trusted/service_runtime/sys_exception.h"
#include "native_client/src/trusted/service_runtime/sys_fdio.h"
#include "native_client/src/trusted/service_runtime/sys_filename.h"
#include "native_client/src/trusted/service_runtime/sys_imc.h"
#include "native_client/src/trusted/service_runtime/sys_list_mappings.h"
#include "native_client/src/trusted/service_runtime/sys_memory.h"
#include "native_client/src/trusted/service_runtime/sys_parallel_io.h"
#include "native_client/src/trusted/service_runtime/sys_random.h"
#include "native_client/src/trusted/service_runtime/include/bits/nacl_syscalls.h"

/*
 * The following declarations define wrapper functions which read in
 * syscall arguments and call the syscall implementation functions listed
 * here.
 */
NACL_DEFINE_SYSCALL_0(NaClSysNull)
NACL_DEFINE_SYSCALL_1(NaClSysDup)
NACL_DEFINE_SYSCALL_2(NaClSysDup2)
NACL_DEFINE_SYSCALL_3(NaClSysOpen)
NACL_DEFINE_SYSCALL_1(NaClSysClose)
NACL_DEFINE_SYSCALL_3(NaClSysRead)
NACL_DEFINE_SYSCALL_3(NaClSysWrite)
NACL_DEFINE_SYSCALL_3(NaClSysLseek)
NACL_DEFINE_SYSCALL_2(NaClSysFstat)
NACL_DEFINE_SYSCALL_2(NaClSysStat)
NACL_DEFINE_SYSCALL_3(NaClSysGetdents)
NACL_DEFINE_SYSCALL_1(NaClSysIsatty)
NACL_DEFINE_SYSCALL_1(NaClSysBrk)
NACL_DEFINE_SYSCALL_6(NaClSysMmap)
NACL_DEFINE_SYSCALL_3(NaClSysMprotect)
NACL_DEFINE_SYSCALL_2(NaClSysListMappings)
NACL_DEFINE_SYSCALL_2(NaClSysMunmap)
NACL_DEFINE_SYSCALL_1(NaClSysExit)
NACL_DEFINE_SYSCALL_0(NaClSysGetpid)
NACL_DEFINE_SYSCALL_1(NaClSysThreadExit)
NACL_DEFINE_SYSCALL_1(NaClSysGetTimeOfDay)
NACL_DEFINE_SYSCALL_0(NaClSysClock)
NACL_DEFINE_SYSCALL_2(NaClSysNanosleep)
NACL_DEFINE_SYSCALL_2(NaClSysClockGetRes)
NACL_DEFINE_SYSCALL_2(NaClSysClockGetTime)
NACL_DEFINE_SYSCALL_2(NaClSysMkdir)
NACL_DEFINE_SYSCALL_1(NaClSysRmdir)
NACL_DEFINE_SYSCALL_1(NaClSysChdir)
NACL_DEFINE_SYSCALL_2(NaClSysGetcwd)
NACL_DEFINE_SYSCALL_1(NaClSysUnlink)
NACL_DEFINE_SYSCALL_2(NaClSysTruncate)
NACL_DEFINE_SYSCALL_2(NaClSysLstat)
NACL_DEFINE_SYSCALL_2(NaClSysLink)
NACL_DEFINE_SYSCALL_2(NaClSysRename)
NACL_DEFINE_SYSCALL_2(NaClSysSymlink)
NACL_DEFINE_SYSCALL_2(NaClSysChmod)
NACL_DEFINE_SYSCALL_2(NaClSysAccess)
NACL_DEFINE_SYSCALL_3(NaClSysReadlink)
NACL_DEFINE_SYSCALL_2(NaClSysUtimes)
NACL_DEFINE_SYSCALL_4(NaClSysPRead)
NACL_DEFINE_SYSCALL_4(NaClSysPWrite)
NACL_DEFINE_SYSCALL_1(NaClSysImcMakeBoundSock)
NACL_DEFINE_SYSCALL_1(NaClSysImcAccept)
NACL_DEFINE_SYSCALL_1(NaClSysImcConnect)
NACL_DEFINE_SYSCALL_3(NaClSysImcSendmsg)
NACL_DEFINE_SYSCALL_3(NaClSysImcRecvmsg)
NACL_DEFINE_SYSCALL_1(NaClSysImcMemObjCreate)
NACL_DEFINE_SYSCALL_1(NaClSysTlsInit)
NACL_DEFINE_SYSCALL_4(NaClSysThreadCreate)
NACL_DEFINE_SYSCALL_0(NaClSysTlsGet)
NACL_DEFINE_SYSCALL_1(NaClSysThreadNice)
NACL_DEFINE_SYSCALL_0(NaClSysMutexCreate)
NACL_DEFINE_SYSCALL_1(NaClSysMutexLock)
NACL_DEFINE_SYSCALL_1(NaClSysMutexUnlock)
NACL_DEFINE_SYSCALL_1(NaClSysMutexTrylock)
NACL_DEFINE_SYSCALL_0(NaClSysCondCreate)
NACL_DEFINE_SYSCALL_2(NaClSysCondWait)
NACL_DEFINE_SYSCALL_1(NaClSysCondSignal)
NACL_DEFINE_SYSCALL_1(NaClSysCondBroadcast)
NACL_DEFINE_SYSCALL_3(NaClSysCondTimedWaitAbs)
NACL_DEFINE_SYSCALL_1(NaClSysImcSocketPair)
NACL_DEFINE_SYSCALL_1(NaClSysSemCreate)
NACL_DEFINE_SYSCALL_1(NaClSysSemWait)
NACL_DEFINE_SYSCALL_1(NaClSysSemPost)
NACL_DEFINE_SYSCALL_1(NaClSysSemGetValue)
NACL_DEFINE_SYSCALL_0(NaClSysSchedYield)
NACL_DEFINE_SYSCALL_2(NaClSysSysconf)
NACL_DEFINE_SYSCALL_3(NaClSysDyncodeCreate)
NACL_DEFINE_SYSCALL_3(NaClSysDyncodeModify)
NACL_DEFINE_SYSCALL_2(NaClSysDyncodeDelete)
NACL_DEFINE_SYSCALL_1(NaClSysSecondTlsSet)
NACL_DEFINE_SYSCALL_0(NaClSysSecondTlsGet)
NACL_DEFINE_SYSCALL_2(NaClSysExceptionHandler)
NACL_DEFINE_SYSCALL_2(NaClSysExceptionStack)
NACL_DEFINE_SYSCALL_0(NaClSysExceptionClearFlag)
NACL_DEFINE_SYSCALL_0(NaClSysTestInfoLeak)
NACL_DEFINE_SYSCALL_1(NaClSysTestCrash)
NACL_DEFINE_SYSCALL_3(NaClSysFutexWaitAbs)
NACL_DEFINE_SYSCALL_2(NaClSysFutexWake)
NACL_DEFINE_SYSCALL_2(NaClSysGetRandomBytes)

/*
 * This fills in the global table of syscall handlers (nacl_syscall[]),
 * which maps syscall numbers to syscall implementation functions via
 * wrapper functions which read in the syscall arguments.
 */
static void RegisterSyscalls(void) {
  NACL_REGISTER_SYSCALL(NaClSysNull, NACL_sys_null);
  NACL_REGISTER_SYSCALL(NaClSysDup, NACL_sys_dup);
  NACL_REGISTER_SYSCALL(NaClSysDup2, NACL_sys_dup2);
  NACL_REGISTER_SYSCALL(NaClSysOpen, NACL_sys_open);
  NACL_REGISTER_SYSCALL(NaClSysClose, NACL_sys_close);
  NACL_REGISTER_SYSCALL(NaClSysRead, NACL_sys_read);
  NACL_REGISTER_SYSCALL(NaClSysWrite, NACL_sys_write);
  NACL_REGISTER_SYSCALL(NaClSysLseek, NACL_sys_lseek);
  NACL_REGISTER_SYSCALL(NaClSysFstat, NACL_sys_fstat);
  NACL_REGISTER_SYSCALL(NaClSysStat, NACL_sys_stat);
  NACL_REGISTER_SYSCALL(NaClSysGetdents, NACL_sys_getdents);
  NACL_REGISTER_SYSCALL(NaClSysIsatty, NACL_sys_isatty);
  NACL_REGISTER_SYSCALL(NaClSysBrk, NACL_sys_brk);
  NACL_REGISTER_SYSCALL(NaClSysMmap, NACL_sys_mmap);
  NACL_REGISTER_SYSCALL(NaClSysMprotect, NACL_sys_mprotect);
  NACL_REGISTER_SYSCALL(NaClSysListMappings, NACL_sys_list_mappings);
  NACL_REGISTER_SYSCALL(NaClSysMunmap, NACL_sys_munmap);
  NACL_REGISTER_SYSCALL(NaClSysExit, NACL_sys_exit);
  NACL_REGISTER_SYSCALL(NaClSysGetpid, NACL_sys_getpid);
  NACL_REGISTER_SYSCALL(NaClSysThreadExit, NACL_sys_thread_exit);
  NACL_REGISTER_SYSCALL(NaClSysGetTimeOfDay, NACL_sys_gettimeofday);
  NACL_REGISTER_SYSCALL(NaClSysClock, NACL_sys_clock);
  NACL_REGISTER_SYSCALL(NaClSysNanosleep, NACL_sys_nanosleep);
  NACL_REGISTER_SYSCALL(NaClSysClockGetRes, NACL_sys_clock_getres);
  NACL_REGISTER_SYSCALL(NaClSysClockGetTime, NACL_sys_clock_gettime);
  NACL_REGISTER_SYSCALL(NaClSysMkdir, NACL_sys_mkdir);
  NACL_REGISTER_SYSCALL(NaClSysRmdir, NACL_sys_rmdir);
  NACL_REGISTER_SYSCALL(NaClSysChdir, NACL_sys_chdir);
  NACL_REGISTER_SYSCALL(NaClSysGetcwd, NACL_sys_getcwd);
  NACL_REGISTER_SYSCALL(NaClSysUnlink, NACL_sys_unlink);
  NACL_REGISTER_SYSCALL(NaClSysTruncate, NACL_sys_truncate);
  NACL_REGISTER_SYSCALL(NaClSysLstat, NACL_sys_lstat);
  NACL_REGISTER_SYSCALL(NaClSysLink, NACL_sys_link);
  NACL_REGISTER_SYSCALL(NaClSysRename, NACL_sys_rename);
  NACL_REGISTER_SYSCALL(NaClSysSymlink, NACL_sys_symlink);
  NACL_REGISTER_SYSCALL(NaClSysChmod, NACL_sys_chmod);
  NACL_REGISTER_SYSCALL(NaClSysAccess, NACL_sys_access);
  NACL_REGISTER_SYSCALL(NaClSysReadlink, NACL_sys_readlink);
  NACL_REGISTER_SYSCALL(NaClSysUtimes, NACL_sys_utimes);
  NACL_REGISTER_SYSCALL(NaClSysPRead, NACL_sys_pread);
  NACL_REGISTER_SYSCALL(NaClSysPWrite, NACL_sys_pwrite);
  NACL_REGISTER_SYSCALL(NaClSysImcMakeBoundSock, NACL_sys_imc_makeboundsock);
  NACL_REGISTER_SYSCALL(NaClSysImcAccept, NACL_sys_imc_accept);
  NACL_REGISTER_SYSCALL(NaClSysImcConnect, NACL_sys_imc_connect);
  NACL_REGISTER_SYSCALL(NaClSysImcSendmsg, NACL_sys_imc_sendmsg);
  NACL_REGISTER_SYSCALL(NaClSysImcRecvmsg, NACL_sys_imc_recvmsg);
  NACL_REGISTER_SYSCALL(NaClSysImcMemObjCreate, NACL_sys_imc_mem_obj_create);
  NACL_REGISTER_SYSCALL(NaClSysTlsInit, NACL_sys_tls_init);
  NACL_REGISTER_SYSCALL(NaClSysThreadCreate, NACL_sys_thread_create);
  NACL_REGISTER_SYSCALL(NaClSysTlsGet, NACL_sys_tls_get);
  NACL_REGISTER_SYSCALL(NaClSysThreadNice, NACL_sys_thread_nice);
  NACL_REGISTER_SYSCALL(NaClSysMutexCreate, NACL_sys_mutex_create);
  NACL_REGISTER_SYSCALL(NaClSysMutexLock, NACL_sys_mutex_lock);
  NACL_REGISTER_SYSCALL(NaClSysMutexUnlock, NACL_sys_mutex_unlock);
  NACL_REGISTER_SYSCALL(NaClSysMutexTrylock, NACL_sys_mutex_trylock);
  NACL_REGISTER_SYSCALL(NaClSysCondCreate, NACL_sys_cond_create);
  NACL_REGISTER_SYSCALL(NaClSysCondWait, NACL_sys_cond_wait);
  NACL_REGISTER_SYSCALL(NaClSysCondSignal, NACL_sys_cond_signal);
  NACL_REGISTER_SYSCALL(NaClSysCondBroadcast, NACL_sys_cond_broadcast);
  NACL_REGISTER_SYSCALL(NaClSysCondTimedWaitAbs, NACL_sys_cond_timed_wait_abs);
  NACL_REGISTER_SYSCALL(NaClSysImcSocketPair, NACL_sys_imc_socketpair);
  NACL_REGISTER_SYSCALL(NaClSysSemCreate, NACL_sys_sem_create);
  NACL_REGISTER_SYSCALL(NaClSysSemWait, NACL_sys_sem_wait);
  NACL_REGISTER_SYSCALL(NaClSysSemPost, NACL_sys_sem_post);
  NACL_REGISTER_SYSCALL(NaClSysSemGetValue, NACL_sys_sem_get_value);
  NACL_REGISTER_SYSCALL(NaClSysSchedYield, NACL_sys_sched_yield);
  NACL_REGISTER_SYSCALL(NaClSysSysconf, NACL_sys_sysconf);
  NACL_REGISTER_SYSCALL(NaClSysDyncodeCreate, NACL_sys_dyncode_create);
  NACL_REGISTER_SYSCALL(NaClSysDyncodeModify, NACL_sys_dyncode_modify);
  NACL_REGISTER_SYSCALL(NaClSysDyncodeDelete, NACL_sys_dyncode_delete);
  NACL_REGISTER_SYSCALL(NaClSysSecondTlsSet, NACL_sys_second_tls_set);
  NACL_REGISTER_SYSCALL(NaClSysSecondTlsGet, NACL_sys_second_tls_get);
  NACL_REGISTER_SYSCALL(NaClSysExceptionHandler, NACL_sys_exception_handler);
  NACL_REGISTER_SYSCALL(NaClSysExceptionStack, NACL_sys_exception_stack);
  NACL_REGISTER_SYSCALL(NaClSysExceptionClearFlag,
                        NACL_sys_exception_clear_flag);
  NACL_REGISTER_SYSCALL(NaClSysTestInfoLeak, NACL_sys_test_infoleak);
  NACL_REGISTER_SYSCALL(NaClSysTestCrash, NACL_sys_test_crash);
  NACL_REGISTER_SYSCALL(NaClSysFutexWaitAbs, NACL_sys_futex_wait_abs);
  NACL_REGISTER_SYSCALL(NaClSysFutexWake, NACL_sys_futex_wake);
  NACL_REGISTER_SYSCALL(NaClSysGetRandomBytes, NACL_sys_get_random_bytes);
}

static void SyscallTableInitEmpty(void) {
  int i;
  for (i = 0; i < NACL_MAX_SYSCALLS; ++i) {
    nacl_syscall[i].handler = &NaClSysNotImplementedDecoder;
  }
}

void NaClSyscallTableInit(void) {
  SyscallTableInitEmpty();
  RegisterSyscalls();
}
