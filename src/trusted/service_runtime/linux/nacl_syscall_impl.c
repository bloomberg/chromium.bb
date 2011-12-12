/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */


#include <time.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <errno.h>
#include <sched.h>
#include <sys/mman.h>
#include <string.h>
#include <unistd.h>

#include "native_client/src/include/portability.h"
#include "native_client/src/include/nacl_platform.h"
#include "native_client/src/include/nacl_macros.h"

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/shared/platform/nacl_sync_checked.h"
#include "native_client/src/shared/platform/nacl_time.h"

#include "native_client/src/trusted/desc/nacl_desc_base.h"
#include "native_client/src/trusted/desc/nacl_desc_conn_cap.h"
#include "native_client/src/trusted/desc/nacl_desc_imc.h"
#include "native_client/src/trusted/desc/nacl_desc_imc_bound_desc.h"
#include "native_client/src/trusted/desc/nacl_desc_imc_shm.h"
#include "native_client/src/trusted/desc/nacl_desc_io.h"

#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_config.h"
#include "native_client/src/trusted/service_runtime/nacl_globals.h"
#include "native_client/src/trusted/service_runtime/nacl_memory_object.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_handlers.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_common.h"
#include "native_client/src/trusted/service_runtime/nacl_text.h"
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

#include "native_client/src/trusted/service_runtime/linux/nacl_syscall_inl.h"

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

int32_t NaClSysNameService(struct NaClAppThread *natp,
                           int                  *desc_in_out) {
  return NaClCommonSysNameService(natp, desc_in_out);
}

int32_t NaClSysDup(struct NaClAppThread *natp,
                   int                  oldfd) {
  return NaClCommonSysDup(natp, oldfd);
}

int32_t NaClSysDup2(struct NaClAppThread  *natp,
                    int                   oldfd,
                    int                   newfd) {
  return NaClCommonSysDup2(natp, oldfd, newfd);
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

/* Warning: sizeof(nacl_abi_off_t)!=sizeof(off_t) on OSX */
int32_t NaClSysLseek(struct NaClAppThread *natp,
                     int                  d,
                     nacl_abi_off_t       *offp,
                     int                  whence) {
  return NaClCommonSysLseek(natp, d, offp, whence);
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

int32_t NaClSysSysbrk(struct NaClAppThread  *natp,
                      void                  *new_break) {
  return NaClSetBreak(natp, (uintptr_t) new_break);
}

int32_t NaClSysMmap(struct NaClAppThread  *natp,
                    void                  *start,
                    size_t                length,
                    int                   prot,
                    int                   flags,
                    int                   d,
                    nacl_abi_off_t        *offp) {
  return NaClCommonSysMmap(natp, start, length, prot, flags, d, offp);
}

int32_t NaClSysMunmap(struct NaClAppThread  *natp,
                      void                  *start,
                      size_t                length) {
  int32_t   retval = -NACL_ABI_EINVAL;
  uintptr_t sysaddr;
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
     * linux mmap of zero length yields a failure, but osx does not, leading
     * to a NaClVmmapUpdate of zero pages, which should not occur.
     */
    retval = -NACL_ABI_EINVAL;
    goto cleanup;
  }
  alloc_rounded_length = NaClRoundAllocPage(length);
  if (alloc_rounded_length != length) {
    length = alloc_rounded_length;
    NaClLog(LOG_WARNING,
            "munmap: rounded length to 0x%"NACL_PRIxS"\n", length);
  }
  sysaddr = NaClUserToSysAddrRange(natp->nap, (uintptr_t) start, length);
  if (kNaClBadAddress == sysaddr) {
    NaClLog(4, "region not user addresses\n");
    retval = -NACL_ABI_EFAULT;
    goto cleanup;
  }

  NaClXMutexLock(&natp->nap->mu);

  while (0 != natp->nap->threads_launching) {
    NaClXCondVarWait(&natp->nap->cv, &natp->nap->mu);
  }
  natp->nap->vm_hole_may_exist = 1;
  /* This call is not needed on Unix, but is included for consistency. */
  NaClUntrustedThreadsSuspend(natp->nap);

  holding_app_lock = 1;
  /*
   * NB: windows (or generic) version would use Munmap virtual
   * function from the backing NaClDesc object obtained by iterating
   * through the address map for the region, and those Munmap virtual
   * functions may return -NACL_ABI_E_MOVE_ADDRESS_SPACE.
   *
   * We should hold the application lock while doing this iteration
   * and unmapping, so that the address space is consistent for other
   * threads.
   */

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

  /*
   * Overwrite current mapping with inaccessible, anonymous
   * zero-filled pages, which should be copy-on-write and thus
   * relatively cheap.  Do not open up an address space hole.
   */
  NaClLog(4,
          ("NaClSysMunmap: mmap(0x%08"NACL_PRIxPTR", 0x%"NACL_PRIxS","
           " 0x%x, 0x%x, -1, 0)\n"),
          sysaddr, length, PROT_NONE,
          MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED);
  if (MAP_FAILED == mmap((void *) sysaddr,
                         length,
                         PROT_NONE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED,
                         -1,
                         (off_t) 0)) {
    NaClLog(4, "mmap to put in anonymous memory failed, errno = %d\n", errno);
    retval = -NaClXlateErrno(errno);
    goto cleanup;
  }
  NaClVmmapUpdate(&natp->nap->mem_map,
                  (NaClSysToUser(natp->nap, (uintptr_t) sysaddr)
                   >> NACL_PAGESHIFT),
                  length >> NACL_PAGESHIFT,
                  0,  /* prot */
                  (struct NaClMemObj *) NULL,
                  1);  /* Delete mapping */
  retval = 0;
cleanup:
  if (holding_app_lock) {
    natp->nap->vm_hole_may_exist = 0;
    NaClXCondVarBroadcast(&natp->nap->cv);
    NaClXMutexUnlock(&natp->nap->mu);

    /* This call is not needed on Unix, but is included for consistency. */
    NaClUntrustedThreadsResume(natp->nap);
  }
  NaClSysCommonThreadSyscallLeave(natp);
  return retval;
}

int32_t NaClSysExit(struct NaClAppThread *natp,
                    int                  status) {
  return NaClCommonSysExit(natp, status);
}

int32_t NaClSysGetpid(struct NaClAppThread *natp) {
  int32_t pid;

  NaClSysCommonThreadSyscallEnter(natp);

  pid = getpid();  /* TODO(bsy): obfuscate? */
  NaClLog(4, "NaClSysGetpid: returning %d\n", pid);

  NaClSysCommonThreadSyscallLeave(natp);
  return pid;
}

int32_t NaClSysThread_Exit(struct NaClAppThread  *natp,
                           int32_t               *stack_flag) {
  return NaClCommonSysThreadExit(natp, stack_flag);
}

int32_t NaClSysGetTimeOfDay(struct NaClAppThread      *natp,
                            struct nacl_abi_timeval   *tv,
                            struct nacl_abi_timezone  *tz) {
  uintptr_t               sysaddr;
  int                     retval;
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
    /*
     * To make it harder to distinguish Linux platforms from Windows,
     * coarsen the time to the same level we get on Windows -
     * milliseconds, unless in "debug" mode.
     */
    if (!NaClHighResolutionTimerEnabled()) {
      now.nacl_abi_tv_usec = (now.nacl_abi_tv_usec / 1000) * 1000;
    }
    CHECK(now.nacl_abi_tv_usec >= 0);
    CHECK(now.nacl_abi_tv_usec < NACL_MICROS_PER_UNIT);
    *(struct nacl_abi_timeval *) sysaddr = now;
  }
cleanup:
  NaClSysCommonThreadSyscallLeave(natp);
  return retval;
}

/*
 * TODO(bsy): REMOVE THIS AND PROVIDE GETRUSAGE.  This is normally
 * not a syscall; instead, it is a library routine on top of
 * getrusage, with appropriate clock tick translation.
 */
int32_t NaClSysClock(struct NaClAppThread *natp) {
  int32_t retval;

  NaClLog(3,
          ("Entered NaClSysClock(%08"NACL_PRIxPTR")\n"),
          (uintptr_t) natp);

  NaClSysCommonThreadSyscallEnter(natp);
  retval = clock();
  NaClSysCommonThreadSyscallLeave(natp);
  return retval;
}

int32_t NaClSysNanosleep(struct NaClAppThread     *natp,
                         struct nacl_abi_timespec *req,
                         struct nacl_abi_timespec *rem) {
  uintptr_t                 sys_req;
  uintptr_t                 sys_rem;
  struct nacl_abi_timespec  t_sleep;
  struct nacl_abi_timespec  t_rem;
  struct nacl_abi_timespec  *remptr;
  int                       retval = -NACL_ABI_EINVAL;

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
  if (NULL == rem) {
    sys_rem = 0;
  } else {
    sys_rem = NaClUserToSysAddrRange(natp->nap, (uintptr_t) rem, sizeof *rem);
    if (kNaClBadAddress == sys_rem) {
      retval = -NACL_ABI_EFAULT;
      goto cleanup;
    }
  }
  /*
   * post-condition: if sys_rem is non-NULL, it's safe to write to
   * (modulo thread races) and the user code wants the remaining time
   * written there.
   */

  NaClLog(4, " copying timespec from %08"NACL_PRIxPTR"x\n", sys_req);
  /* copy once */
  t_sleep = *(struct nacl_abi_timespec *) sys_req;

  remptr = (0 == sys_rem) ? NULL : &t_rem;
  /* NULL != remptr \equiv NULL != rem */

  /*
   * We assume that we do not need to normalize the time request values.
   *
   * If bogus values can cause the underlying OS to get into trouble,
   * then we need more checking here.
   */
  NaClLog(4, "NaClSysNanosleep(time = %"NACL_PRId64".%09"NACL_PRId64" S)\n",
          (int64_t) t_sleep.tv_sec, (int64_t) t_sleep.tv_nsec);
  retval = NaClNanosleep(&t_sleep, remptr);
  NaClLog(4, "NaClNanosleep returned %d\n", retval);

  if (-EINTR == retval && NULL != rem) {
    /* definitely different types, and shape may actually differ too. */
    rem = (struct nacl_abi_timespec *) sys_rem;
    rem->tv_sec = remptr->tv_sec;
    rem->tv_nsec = remptr->tv_nsec;
  }

cleanup:
  NaClLog(4, "nanosleep done.\n");
  NaClSysCommonThreadSyscallLeave(natp);
  return retval;
}

int32_t NaClSysImc_MakeBoundSock(struct NaClAppThread *natp,
                                 int32_t              *sap) {
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

int32_t NaClSysImc_Sendmsg(struct NaClAppThread         *natp,
                           int                          d,
                           struct NaClAbiNaClImcMsgHdr  *nanimhp,
                           int                          flags) {
  return NaClCommonSysImc_Sendmsg(natp, d, nanimhp, flags);
}

int32_t NaClSysImc_Recvmsg(struct NaClAppThread         *natp,
                           int                          d,
                           struct NaClAbiNaClImcMsgHdr  *nimhp,
                           int                          flags) {
  return NaClCommonSysImc_Recvmsg(natp, d, nimhp, flags);
}

int32_t NaClSysImc_Mem_Obj_Create(struct NaClAppThread  *natp,
                                  nacl_abi_size_t       size) {
  return NaClCommonSysImc_Mem_Obj_Create(natp, size);
}

int32_t NaClSysTls_Init(struct NaClAppThread  *natp,
                        void                  *thread_ptr) {
  return NaClCommonSysTls_Init(natp, thread_ptr);
}

int32_t NaClSysThread_Create(struct NaClAppThread *natp,
                             void                 *prog_ctr,
                             void                 *stack_ptr,
                             void                 *thread_ptr,
                             void                 *second_thread_ptr) {
  return NaClCommonSysThread_Create(natp, prog_ctr, stack_ptr, thread_ptr,
                                    second_thread_ptr);
}

int32_t NaClSysTls_Get(struct NaClAppThread *natp) {
  return NaClCommonSysTlsGet(natp);
}

int32_t NaClSysThread_Nice(struct NaClAppThread *natp, const int nice) {
  return NaClCommonSysThread_Nice(natp, nice);
}

/* mutex */

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
  UNREFERENCED_PARAMETER(natp);
  sched_yield();
  return 0;
}

int32_t NaClSysSysconf(struct NaClAppThread *natp,
                       int32_t name,
                       int32_t *result) {
  int32_t         retval = -NACL_ABI_EINVAL;
  static int32_t  number_of_workers = -1;
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
#ifdef _SC_NPROCESSORS_ONLN
    case NACL_ABI__SC_NPROCESSORS_ONLN: {
      if (-1 == number_of_workers) {
        number_of_workers = sysconf(_SC_NPROCESSORS_ONLN);
      }
      if (-1 == number_of_workers) {
        /* failed to get the number of processors */
        retval = -NACL_ABI_EINVAL;
        goto cleanup;
      }
      *(int32_t*)sysaddr = number_of_workers;
      break;
    }
#endif
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
 * Note that this is duplicated in win/nacl_syscall_impl.c but is
 * too trivial to share.
 * TODO(mseaborn): We could reduce the duplication if
 * nacl_syscall_handlers_gen2.py did not scrape the OS-specific files.
 */
int32_t NaClSysSecond_Tls_Set(struct NaClAppThread *natp,
                              uint32_t             new_value) {
  natp->tls2 = new_value;
  return 0;
}

int32_t NaClSysSecond_Tls_Get(struct NaClAppThread *natp) {
  return natp->tls2;
}

int32_t NaClSysException_Handler(struct NaClAppThread *natp,
                                 uint32_t             handler_addr,
                                 uint32_t             old_handler) {
  uintptr_t safe_old_handler;
  if (!NaClIsValidJumpTarget(natp->nap, handler_addr)) {
    return -NACL_ABI_EFAULT;
  }
  safe_old_handler = NaClUserToSysAddrNullOkay(natp->nap, old_handler);
  if (kNaClBadAddress == safe_old_handler) {
    return -NACL_ABI_EINVAL;
  }
  NaClXMutexLock(&natp->nap->exception_mu);
  if (old_handler) {
    *(uint32_t*)safe_old_handler = natp->nap->exception_handler;
  }
  natp->nap->exception_handler = handler_addr;
  NaClXMutexUnlock(&natp->nap->exception_mu);
  return 0;
}

int32_t NaClSysException_Stack(struct NaClAppThread *natp,
                               uint32_t             stack_addr,
                               uint32_t             stack_size) {
  if (kNaClBadAddress == NaClUserToSysAddrNullOkay(natp->nap,
                                                   stack_addr + stack_size)) {
    return -NACL_ABI_EINVAL;
  }
  natp->user.exception_stack = stack_addr + stack_size;
  return 0;
}

int32_t NaClSysException_Clear_Flag(struct NaClAppThread *natp) {
  natp->user.exception_flag = 0;
  return 0;
}

int32_t NaClSysTest_InfoLeak(struct NaClAppThread *natp) {
  return NaClCommonSysTest_InfoLeak(natp);
}
