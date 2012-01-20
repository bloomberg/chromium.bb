/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
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

#include "native_client/src/include/nacl_macros.h"

#include "native_client/src/shared/platform/nacl_check.h"
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
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_handlers.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_common.h"
#include "native_client/src/trusted/service_runtime/nacl_text.h"
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

int32_t NaClSysMunmap(struct NaClAppThread  *natp,
                      void                  *start,
                      size_t                length) {
  int32_t   retval = -NACL_ABI_EINVAL;
  uintptr_t sysaddr;
  uintptr_t addr;
  uintptr_t endaddr;
  int       holding_app_lock = 0;
  size_t    alloc_rounded_length;

  NaClLog(3, "Entered NaClSysMunmap(0x%08"NACL_PRIxPTR", "
          "0x%08"NACL_PRIxPTR", 0x%"NACL_PRIxS")\n",
          (uintptr_t) natp, (uintptr_t) start, length);

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

  while (0 != natp->nap->threads_launching) {
    NaClXCondVarWait(&natp->nap->cv, &natp->nap->mu);
  }
  natp->nap->vm_hole_may_exist = 1;
  NaClUntrustedThreadsSuspend(natp->nap);

  holding_app_lock = 1;

  /*
   * User should be unable to unmap any executable pages.  We check here.
   */
  if (NaClSysCommonAddrRangeContainsExecutablePages_mu(natp->nap,
                                                       (uintptr_t) start,
                                                       length)) {
    NaClLog(4, "NaClSysMunmap: region contains executable pages\n");
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
      retval = (*((struct NaClDescVtbl const *) entry->nmop->ndp->base.vtbl)->
                Unmap)(entry->nmop->ndp,
                       natp->nap->effp,
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
    natp->nap->vm_hole_may_exist = 0;
    NaClXCondVarBroadcast(&natp->nap->cv);
    NaClXMutexUnlock(&natp->nap->mu);

    NaClUntrustedThreadsResume(natp->nap);
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
  struct nacl_abi_timeval now;

  UNREFERENCED_PARAMETER(tz);

  NaClLog(3,
          ("Entered NaClSysGetTimeOfDay(%08"NACL_PRIxPTR
           ", 0x%08"NACL_PRIxPTR", 0x%08"NACL_PRIxPTR")\n"),
          (uintptr_t) natp, (uintptr_t) tv, (uintptr_t) tz);
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

  retval = NaClGetTimeOfDay(&now);
  if (0 == retval) {
    CHECK(now.nacl_abi_tv_usec >= 0);
    CHECK(now.nacl_abi_tv_usec < NACL_MICROS_PER_UNIT);
    *(struct nacl_abi_timeval volatile *) sysaddr = now;
  }
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

  NaClLog(3,
          ("Entered NaClSysClock(%08"NACL_PRIxPTR")\n"),
          (uintptr_t) natp);

  NaClSysCommonThreadSyscallEnter(natp);
  retval = 1000 * clock();
  /*
   * Windows CLOCKS_PER_SEC is 1000, but XSI requires it to be
   * 1000000L and that's the ABI that we are sticking with.
   *
   * NB: 1000 \cdot n \bmod 2^{32} when n is a 32-bit counter is fine
   * -- user code has to deal with \pmod{2^{32}} wraparound anyway,
   * and time differences will work out fine:
   *
   * \begin{align*}
   * (1000 \cdot \Delta n) \bmod 2^{32}
   *  &\equiv ((1000 \bmod 2^{32}) \cdot (\Delta n \bmod 2^{32}) \bmod 2^{32}\\
   *  &\equiv (1000 \cdot (\Delta n \bmod 2^{32})) \bmod 2^{32}.
   * \end{align*}
   *
   * so when $\Delta n$ is small, the time difference is going to be a
   * small multiple of $1000$, regardless of wraparound.
   */
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

  NaClLog(3,
          ("Entered NaClSysNanosleep(0x%08"NACL_PRIxPTR
           ", 0x%08"NACL_PRIxPTR", 0x%08"NACL_PRIxPTR"x)\n"),
          (uintptr_t) natp, (uintptr_t) req, (uintptr_t) rem);

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

  NaClLog(3,
          ("Entered NaClSysSysconf(%08"NACL_PRIxPTR
           "x, %d, 0x%08"NACL_PRIxPTR")\n"),
          (uintptr_t) natp, name, (uintptr_t) result);

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
    case NACL_ABI__SC_SENDMSG_MAX_SIZE: {
      /* TODO(sehr,bsy): this value needs to be determined at run time. */
      const int32_t kImcSendMsgMaxSize = 1 << 16;
      *(int32_t*)sysaddr = kImcSendMsgMaxSize;
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

int32_t NaClSysDyncode_Create(struct NaClAppThread *natp,
                            uint32_t             dest,
                            uint32_t             src,
                            uint32_t             size) {
  return NaClTextSysDyncode_Create(natp, dest, src, size);
}

int32_t NaClSysDyncode_Modify(struct NaClAppThread *natp,
                            uint32_t             dest,
                            uint32_t             src,
                            uint32_t             size) {
  return NaClTextSysDyncode_Modify(natp, dest, src, size);
}

int32_t NaClSysDyncode_Delete(struct NaClAppThread *natp,
                            uint32_t             dest,
                            uint32_t             size) {
  return NaClTextSysDyncode_Delete(natp, dest, size);
}

/*
 * Note that this is duplicated in linux/nacl_syscall_impl.c but is
 * too trivial to share.
 * TODO(mseaborn): We could reduce the duplication if
 * nacl_syscall_handlers_gen.py did not scrape the OS-specific files.
 */
int32_t NaClSysSecond_Tls_Set(struct NaClAppThread *natp,
                              uint32_t             new_value) {
  natp->tls2 = new_value;
  return 0;
}

int32_t NaClSysSecond_Tls_Get(struct NaClAppThread *natp) {
  return natp->tls2;
}
