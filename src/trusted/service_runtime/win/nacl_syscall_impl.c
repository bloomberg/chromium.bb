/*
 * Copyright 2008  The Native Client Authors.  All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include "native_client/src/include/portability.h"

#include <windows.h>
#include <io.h>
#include <process.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <sys/timeb.h>

#include <time.h>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/platform/nacl_time.h"
#include "native_client/src/shared/platform/win/xlate_system_error.h"

#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_conn_cap.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"
#include "native_client/src/trusted/desc/nacl_desc_imc_bound_desc.h"
#include "native_client/src/trusted/desc/nacl_desc_imc_shm.h"
#include "native_client/src/trusted/desc/nacl_desc_io.h"

#include "native_client/src/trusted/service_runtime/internal_errno.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/nacl_closure.h"
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_handlers.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_common.h"
#include "native_client/src/trusted/service_runtime/nacl_memory_object.h"
#include "native_client/src/trusted/service_runtime/sel_util.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"
#include "native_client/src/trusted/service_runtime/sel_memory.h"

#include "native_client/src/trusted/service_runtime/include/bits/mman.h"
#include "native_client/src/trusted/service_runtime/include/bits/nacl_syscalls.h"
#include "native_client/src/trusted/service_runtime/include/machine/_types.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/service_runtime/include/sys/nacl_imc_api.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"
#include "native_client/src/trusted/service_runtime/include/sys/time.h"
#include "native_client/src/trusted/service_runtime/include/sys/unistd.h"

#include "native_client/src/trusted/service_runtime/win/nacl_syscall_inl.h"

/*
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 *
 * The implementation of the system call abstractions will probably
 * change.  In particularly, blocking I/O calls must be interruptible
 * in order to implement the address-space move mechanism for mmap
 * error recovery, and the it seems that the only way that this would
 * be feasible is to do the following: instead of using the POSIX
 * abstraction layer, do the I/O using Windows file handles opened for
 * asynchronous operations.  When a potentially blocking system call
 * (e.g., read or write) is performed, use overlapped I/O via
 * ReadFile/WriteFile to initiate the I/O operation in a non-blocking
 * manner, and use a separate event object, so that the thread can,
 * after initiating the I/O, perform WaitForMultipleObjects on both
 * I/O completion (event in the OVERLAPPED structure) and on
 * mmap-generated interrupts.  The event can be signalled via SetEvent
 * by any other thread that wish to perform a safe mmap operation.
 *
 * When the safe mmap is to occur, all other application threads are
 * stopped (beware, of course, of the race condition where two threads
 * try to do mmap), and the remaining running thread performs
 * VirtualFree and MapViewOfFileEx.  If a thread (from some injected
 * DLL) puts some memory in the hole created by VirtualFree before the
 * MapViewOfFileEx runs, then we have to move the entire address space
 * to avoid allowing the untrusted NaCl app from touching this
 * innocent thread's memory.
 *
 * What this implies is that a mechanism must exist in order for the
 * mmapping thread to stop all other application threads, and this is
 * why the blocking syscalls must be interruptible.  When interrupted,
 * the thread that initiated the I/O must perform CancelIo and check,
 * via GetOverlappedResult, to see how much have completed, etc, then
 * put itself into a restartable state -- we might simply return
 * NACL_ABI_EINTR if no work has been dnoe and require libc to restart
 * the syscall in the SysV style, though it should be possible to just
 * restart the syscall in the BSD style -- and to signal the mmapping
 * thread that it is ready.
 *
 * WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING WARNING
 */

struct NaClSyscallTableEntry nacl_syscall[NACL_MAX_SYSCALLS] = {{0}};

#if defined(HAVE_SDL)
# include <SDL.h>
#endif /* HAVE_SDL */

static int32_t NotImplementedDecoder(struct NaClAppThread *natp) {
  UNREFERENCED_PARAMETER(natp);
  return -NACL_ABI_ENOSYS;
}

static void NaClAddSyscall(int num, int32_t (*fn)(struct NaClAppThread *)) {
  if (nacl_syscall[num].handler != &NotImplementedDecoder) {
    NaClLog(LOG_FATAL, "Duplicate syscall number %d\n", num);
  }
  nacl_syscall[num].handler = fn;
}

/* ====================================================================== */

int32_t NaClSysNull(struct NaClAppThread *natp) {
  UNREFERENCED_PARAMETER(natp);
  return 0;
}

int32_t NaClSysOpen(struct NaClAppThread  *natp,
                    char                  *pathname,
                    int                   flags,
                    int                   mode) {
  return NaClCommonSysOpen(natp, pathname, flags, mode);
}

int32_t NaClSysClose(struct NaClAppThread *natp,
                     int                  d) {
  return NaClCommonSysClose(natp, d);
}

int32_t NaClSysRead(struct NaClAppThread  *natp,
                    int                   d,
                    void                  *buf,
                    size_t                count) {
  return NaClCommonSysRead(natp, d, buf, count);
}

int32_t NaClSysWrite(struct NaClAppThread *natp,
                     int                  d,
                     void                 *buf,
                     size_t               count) {
  return NaClCommonSysWrite(natp, d, buf, count);
}

int32_t NaClSysLseek(struct NaClAppThread *natp,
                     int                  d,
                     nacl_abi_off_t       offset,
                     int                  whence) {
  return NaClCommonSysLseek(natp, d, (nacl_off64_t) offset, whence);
}

int32_t NaClSysIoctl(struct NaClAppThread *natp,
                     int                  d,
                     int                  request,
                     void                 *arg) {
  return NaClCommonSysIoctl(natp, d, request, arg);
}

int32_t NaClSysFstat(struct NaClAppThread *natp,
                     int                  d,
                     struct nacl_abi_stat *nasp) {
  return NaClCommonSysFstat(natp, d, nasp);
}

int32_t NaClSysStat(struct NaClAppThread *natp,
                    const char           *path,
                    struct nacl_abi_stat *nasp) {
  return NaClCommonSysStat(natp, path, nasp);
}

int32_t NaClSysGetdents(struct NaClAppThread  *natp,
                        int                   d,
                        void                  *buf,
                        size_t                count) {
  return NaClCommonSysGetdents(natp, d, buf, count);
}

int32_t NaClSysSysbrk(struct NaClAppThread *natp,
                      void *new_break) {
  return NaClSetBreak(natp, (uintptr_t) new_break);
}

int32_t NaClSysMmap(struct NaClAppThread  *natp,
                    void                  *start,
                    size_t                length,
                    int                   prot,
                    int                   flags,
                    int                   d,
                    nacl_abi_off_t        offset) {
  return NaClCommonSysMmap(natp, start, length, prot, flags, d, offset);
}

int32_t NaClSysMunmap(struct NaClAppThread  *natp,
                      void                  *start,
                      size_t                length) {
  int32_t   retval = -NACL_ABI_EINVAL;
  uintptr_t sysaddr;
  uintptr_t addr;
  uintptr_t endaddr;
  int       holding_app_lock = 0;
  size_t    alloc_rounded_length;

  NaClLog(3, "NaClSysMunmap(0x%08x, 0x%08x, 0x%x)\n",
          natp, start, length);

  NaClSysCommonThreadSyscallEnter(natp);

  if (!NaClIsAllocPageMultiple((uintptr_t) start)) {
    NaClLog(4, "start addr not allocation multiple\n");
    retval = -NACL_ABI_EINVAL;
    goto cleanup;
  }
  if (0 == length) {
    /*
     * linux mmap of zero length yields a failure, but windows code
     * would just iterate through and do nothing, so does not yield a
     * failure.
     */
    retval = -NACL_ABI_EINVAL;
    goto cleanup;
  }
  alloc_rounded_length = NaClRoundAllocPage(length);
  if (alloc_rounded_length != length) {
    length = alloc_rounded_length;
    NaClLog(LOG_WARNING,
            "munmap: rounded length to 0x%x\n", length);
  }
  sysaddr = NaClUserToSysAddrRange(natp->nap, (uintptr_t) start, length);
  if (kNaClBadAddress == sysaddr) {
    retval = -NACL_ABI_EFAULT;
    goto cleanup;
  }

  NaClXMutexLock(&natp->nap->mu);
  holding_app_lock = 1;

  /*
   * User should be unable to unmap any executable pages.  We check here.
   */
  if (NaClSysCommonAddrRangeContainsExecutablePages_mu(natp->nap,
                                                       (uintptr_t) start,
                                                       length)) {
    NaClLog(2, "NaClSysMunmap: region contains executable pages\n");
    retval = -NACL_ABI_EINVAL;
    goto cleanup;
  }

  endaddr = sysaddr + length;
  for (addr = sysaddr; addr < endaddr; addr += NACL_MAP_PAGESIZE) {
    struct NaClVmmapEntry const *entry;

    entry = NaClVmmapFindPage(&natp->nap->mem_map,
                              NaClSysToUser(natp->nap, addr)
                              >> NACL_PAGESHIFT);
    if (NULL == entry) {
      NaClLog(LOG_FATAL,
              "NaClSysMunmap: could not find VM map entry for addr 0x%08x\n",
              addr);
    }
    NaClLog(3,
            ("NaClSysMunmap: addr 0x%08x, nmop 0x%08x\n"),
            addr, entry->nmop);
    if (NULL == entry->nmop) {
      /* anonymous memory; we just decommit it and thus make it inaccessible */
      if (!VirtualFree((void *) addr,
                       NACL_MAP_PAGESIZE,
                       MEM_DECOMMIT)) {
        int error = GetLastError();
        NaClLog(LOG_FATAL,
                ("NaClSysMunmap: Could not VirtualFree MEM_DECOMMIT"
                 " addr 0x%08x, error %d (0x%x)\n"),
                addr, error, error);
      }
    } else {
      /*
       * This should invoke a "safe" version of unmap that fills the
       * memory hole as quickly as possible, and may return
       * -NACL_ABI_E_MOVE_ADDRESS_SPACE.  The "safe" version just
       * minimizes the size of the timing hole for any racers, plus
       * the size of the memory window is only 64KB, rather than
       * whatever size the user is unmapping.
       */
      retval = (*entry->nmop->ndp->vtbl->Unmap)(entry->nmop->ndp,
                                                natp->effp,
                                                (void*) addr,
                                                NACL_MAP_PAGESIZE);
      if (0 != retval) {
        NaClLog(LOG_FATAL,
                ("NaClSysMunmap: Could not unmap via ndp->Unmap 0x%08x"
                 " and cannot handle address space move\n"),
                addr);
      }
    }
    NaClVmmapUpdate(&natp->nap->mem_map,
                    (NaClSysToUser(natp->nap, (uintptr_t) addr)
                     >> NACL_PAGESHIFT),
                    NACL_PAGES_PER_MAP,
                    0,  /* prot */
                    (struct NaClMemObj *) NULL,
                    1);  /* delete */
  }
  retval = 0;
cleanup:
  if (holding_app_lock) {
    NaClXMutexUnlock(&natp->nap->mu);
  }
  NaClSysCommonThreadSyscallLeave(natp);
  return retval;
}

int32_t NaClSysExit(struct NaClAppThread *natp,
                    int                  status) {
  return NaClCommonSysExit(natp, status);
}

int32_t NaClSysGetpid(struct NaClAppThread  *natp) {
  return _getpid();  /* TODO(bsy): obfuscate? */
}

int32_t NaClSysThread_Exit(struct NaClAppThread  *natp, int32_t *stack_flag) {
  return NaClCommonSysThreadExit(natp, stack_flag);
}

int32_t NaClSysGetTimeOfDay(struct NaClAppThread      *natp,
                            struct nacl_abi_timeval   *tv,
                            struct nacl_abi_timezone  *tz) {
  int32_t                 retval;
  uintptr_t               sysaddr;
  UNREFERENCED_PARAMETER(tz);

  NaClSysCommonThreadSyscallEnter(natp);

  sysaddr = NaClUserToSysAddrRange(natp->nap, (uintptr_t) tv, sizeof tv);

  /*
   * tz is not supported in linux, nor is it supported by glibc, since
   * tzset(3) and the zoneinfo file should be used instead.
   *
   * TODO(bsy) Do we make the zoneinfo directory available to
   * applications?
   */

  if (kNaClBadAddress == sysaddr) {
    retval = -NACL_ABI_EFAULT;
    goto cleanup;
  }

  retval = NaClGetTimeOfDay((struct nacl_abi_timeval *) sysaddr);
cleanup:
  NaClSysCommonThreadSyscallLeave(natp);
  return retval;
}

/*
 * TODO(bsy): REMOVE THIS AND PROVIDE GETRUSAGE.  this is normally not
 * a syscall; instead, it is a library routine on top of getrusage,
 * with appropriate clock tick translation.
 */
/* ARGSUSED */
int32_t NaClSysClock(struct NaClAppThread *natp) {
  int32_t retval;

  UNREFERENCED_PARAMETER(natp);
  NaClSysCommonThreadSyscallEnter(natp);
  retval = clock();
  NaClSysCommonThreadSyscallLeave(natp);
  return retval;
}

int32_t NaClSysNanosleep(struct NaClAppThread     *natp,
                         struct nacl_abi_timespec *req,
                         struct nacl_abi_timespec *rem) {
  uintptr_t                 sys_req;
  int                       retval = -NACL_ABI_EINVAL;
  struct nacl_abi_timespec  host_req;

  UNREFERENCED_PARAMETER(rem);

  NaClLog(4, "NaClSysNanosleep(%08x)\n", (uintptr_t) req);

  NaClSysCommonThreadSyscallEnter(natp);

  sys_req = NaClUserToSysAddrRange(natp->nap, (uintptr_t) req, sizeof *req);
  if (kNaClBadAddress == sys_req) {
    retval = -NACL_ABI_EFAULT;
    goto cleanup;
  }

  /* copy once */
  host_req = *(struct nacl_abi_timespec *) sys_req;
  /*
   * We assume that we do not need to normalize the time request values.
   *
   * If bogus values can cause the underlying OS to get into trouble,
   * then we need more checking here.
   */

  NaClLog(4, "NaClSysNanosleep(time = %d.%09ld S)\n",
          host_req.tv_sec, host_req.tv_nsec);

  retval = NaClNanosleep(&host_req, NULL);

cleanup:
  NaClSysCommonThreadSyscallLeave(natp);
  return retval;
}

#if defined(HAVE_SDL)

int32_t NaClSysMultimedia_Init(struct NaClAppThread *natp,
                               int                  subsys) {
  /* tail call, should compile to a jump */
  return NaClCommonSysMultimedia_Init(natp, subsys);
}

int32_t NaClSysMultimedia_Shutdown(struct NaClAppThread *natp) {
  return NaClCommonSysMultimedia_Shutdown(natp);
}

int32_t NaClSysVideo_Init(struct NaClAppThread *natp,
                          int                  width,
                          int                  height) {
  /* tail call, should compile to a jump */
  return NaClCommonSysVideo_Init(natp, width, height);
}

int32_t NaClSysVideo_Shutdown(struct NaClAppThread *natp) {
  return NaClCommonSysVideo_Shutdown(natp);
}

int32_t NaClSysVideo_Update(struct NaClAppThread *natp,
                            unsigned char        *data) {
  /* tail call, should compile to a jump */
  return NaClCommonSysVideo_Update(natp, data);
}

int32_t NaClSysVideo_Poll_Event(struct NaClAppThread *natp,
                                union NaClMultimediaEvent *event) {
  return NaClCommonSysVideo_Poll_Event(natp, event);
}

int32_t NaClSysAudio_Init(struct NaClAppThread *natp,
                          int                  format,
                          int                  desired_samples,
                          int                  *obtained_samples) {
  return NaClCommonSysAudio_Init(natp, format,
                                 desired_samples, obtained_samples);
}


int32_t NaClSysAudio_Stream(struct NaClAppThread *natp,
                            const void           *data,
                            size_t               *size) {
  return NaClSliceSysAudio_Stream(natp, data, size);
}


int32_t NaClSysAudio_Shutdown(struct NaClAppThread *natp) {
  return NaClCommonSysAudio_Shutdown(natp);
}


#endif /* HAVE_SDL */

int32_t NaClSysSrpc_Get_Fd(struct NaClAppThread *natp) {
  extern int NaClSrpcFileDescriptor;
  UNREFERENCED_PARAMETER(natp);

  return NaClSrpcFileDescriptor;
}

int32_t NaClSysImc_MakeBoundSock(struct NaClAppThread *natp,
                                 int                  *sap) {
  return NaClCommonSysImc_MakeBoundSock(natp, sap);
}

int32_t NaClSysImc_Accept(struct NaClAppThread  *natp,
                          int                   d) {
  return NaClCommonSysImc_Accept(natp, d);
}

int32_t NaClSysImc_Connect(struct NaClAppThread *natp,
                           int                  d) {
  return NaClCommonSysImc_Connect(natp, d);
}

int32_t NaClSysImc_Sendmsg(struct NaClAppThread *natp,
                           int                  d,
                           struct NaClImcMsgHdr *nimhp,
                           int                  flags) {
  return NaClCommonSysImc_Sendmsg(natp, d, nimhp, flags);
}

int32_t NaClSysImc_Recvmsg(struct NaClAppThread *natp,
                           int                  d,
                           struct NaClImcMsgHdr *nimhp,
                           int                  flags) {
  return NaClCommonSysImc_Recvmsg(natp, d, nimhp, flags);
}

int32_t NaClSysImc_Mem_Obj_Create(struct NaClAppThread  *natp,
                                  size_t                size) {
  return NaClCommonSysImc_Mem_Obj_Create(natp, size);
}

int32_t NaClSysTls_Init(struct NaClAppThread  *natp,
                        void                  *tdb,
                        size_t                size) {
  return NaClCommonSysTls_Init(natp, tdb, size);
}

int32_t NaClSysThread_Create(struct NaClAppThread *natp,
                             void                 *prog_ctr,
                             void                 *stack_ptr,
                             void                 *tdb,
                             size_t               tdb_size) {
  return NaClCommonSysThread_Create(natp, prog_ctr, stack_ptr, tdb, tdb_size);
}

int32_t NaClSysThread_Nice(struct NaClAppThread *natp, const int nice) {
  return NaClCommonSysThread_Nice(natp, nice);
}

int32_t NaClSysMutex_Create(struct NaClAppThread *natp) {
  return NaClCommonSysMutex_Create(natp);
}

int32_t NaClSysMutex_Lock(struct NaClAppThread *natp,
                          int32_t              mutex_handle) {
  return NaClCommonSysMutex_Lock(natp, mutex_handle);
}

int32_t NaClSysMutex_Unlock(struct NaClAppThread *natp,
                            int32_t              mutex_handle) {
  return NaClCommonSysMutex_Unlock(natp, mutex_handle);
}

int32_t NaClSysMutex_Trylock(struct NaClAppThread *natp,
                             int32_t              mutex_handle) {
  return NaClCommonSysMutex_Trylock(natp, mutex_handle);
}


/* condition variable */

int32_t NaClSysCond_Create(struct NaClAppThread *natp) {
  return NaClCommonSysCond_Create(natp);
}

int32_t NaClSysCond_Wait(struct NaClAppThread *natp,
                         int32_t              cond_handle,
                         int32_t              mutex_handle) {
  return NaClCommonSysCond_Wait(natp, cond_handle, mutex_handle);
}

int32_t NaClSysCond_Signal(struct NaClAppThread *natp,
                           int32_t              cond_handle) {
  return NaClCommonSysCond_Signal(natp, cond_handle);
}

int32_t NaClSysCond_Broadcast(struct NaClAppThread *natp,
                              int32_t              cond_handle) {
  return NaClCommonSysCond_Broadcast(natp, cond_handle);
}

int32_t NaClSysCond_Timed_Wait_Abs(struct NaClAppThread     *natp,
                                   int32_t                  cond_handle,
                                   int32_t                  mutex_handle,
                                   struct nacl_abi_timespec *ts) {
  return NaClCommonSysCond_Timed_Wait_Abs(natp,
                                          cond_handle,
                                          mutex_handle,
                                          ts);
}

int32_t NaClSysImc_SocketPair(struct NaClAppThread  *natp,
                              int32_t               *d_out) {
  return NaClCommonSysImc_SocketPair(natp, d_out);
}
int32_t NaClSysSem_Create(struct NaClAppThread *natp,
                          int32_t              init_value) {
  return NaClCommonSysSem_Create(natp, init_value);
}

int32_t NaClSysSem_Wait(struct NaClAppThread *natp,
                        int32_t              sem_handle) {
  return NaClCommonSysSem_Wait(natp, sem_handle);
}

int32_t NaClSysSem_Post(struct NaClAppThread *natp,
                        int32_t              sem_handle) {
  return NaClCommonSysSem_Post(natp, sem_handle);
}

int32_t NaClSysSem_Get_Value(struct NaClAppThread *natp,
                             int32_t              sem_handle) {
  return NaClCommonSysSem_Get_Value(natp, sem_handle);
}

int32_t NaClSysSched_Yield(struct NaClAppThread *natp) {
  SwitchToThread();
  return 0;
}

int32_t NaClSysSysconf(struct NaClAppThread *natp,
                       int32_t              name,
                       int32_t              *result) {
  int32_t         retval = -NACL_ABI_EINVAL;
  static int32_t  number_of_workers = 0;
  uintptr_t       sysaddr;

  NaClSysCommonThreadSyscallEnter(natp);

  sysaddr = NaClUserToSysAddrRange(natp->nap,
                                   (uintptr_t) result,
                                   sizeof(*result));
  if (kNaClBadAddress == sysaddr) {
    retval = -NACL_ABI_EINVAL;
    goto cleanup;
  }

  switch (name) {
    case NACL_ABI__SC_NPROCESSORS_ONLN: {
      if (0 == number_of_workers) {
        SYSTEM_INFO si;
        GetSystemInfo(&si);
        number_of_workers = (int32_t)si.dwNumberOfProcessors;
      }
      *(int32_t*)sysaddr = number_of_workers;
      break;
    }
    default:
      retval = -NACL_ABI_EINVAL;
      goto cleanup;
  }
  retval = 0;
cleanup:
  NaClSysCommonThreadSyscallLeave(natp);
  return retval;
}
