/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include "native_client/src/trusted/service_runtime/sys_filename.h"

#include <string.h>

#include "native_client/src/shared/platform/nacl_host_desc.h"
#include "native_client/src/shared/platform/nacl_host_dir.h"
#include "native_client/src/trusted/desc/nacl_desc_dir.h"
#include "native_client/src/trusted/desc/nacl_desc_io.h"
#include "native_client/src/trusted/service_runtime/include/sys/errno.h"
#include "native_client/src/trusted/service_runtime/include/sys/fcntl.h"
#include "native_client/src/trusted/service_runtime/include/sys/stat.h"
#include "native_client/src/trusted/service_runtime/nacl_app_thread.h"
#include "native_client/src/trusted/service_runtime/nacl_copy.h"
#include "native_client/src/trusted/service_runtime/nacl_syscall_common.h"
#include "native_client/src/trusted/service_runtime/sel_ldr.h"


/*
 * NaClOpenAclCheck: Is the NaCl app authorized to open this file?  The
 * return value is syscall return convention, so 0 is success and
 * small negative numbers are negated errno values.
 */
int32_t NaClOpenAclCheck(struct NaClApp *nap,
                         char const     *path,
                         int            flags,
                         int            mode) {
  /*
   * TODO(bsy): provide some minimal authorization check, based on
   * whether a debug flag is set; eventually provide a data-driven
   * authorization configuration mechanism, perhaps persisted via
   * gears.  need GUI for user configuration, as well as designing an
   * appropriate language (with sufficient expressiveness), however.
   */
  NaClLog(1, "NaClOpenAclCheck(0x%08"NACL_PRIxPTR", %s, 0%o, 0%o)\n",
          (uintptr_t) nap, path, flags, mode);
  if (3 < NaClLogGetVerbosity()) {
    NaClLog(0, "O_ACCMODE: 0%o\n", flags & NACL_ABI_O_ACCMODE);
    NaClLog(0, "O_RDONLY = %d\n", NACL_ABI_O_RDONLY);
    NaClLog(0, "O_WRONLY = %d\n", NACL_ABI_O_WRONLY);
    NaClLog(0, "O_RDWR   = %d\n", NACL_ABI_O_RDWR);
#define FLOG(VAR, BIT) do {\
      NaClLog(1, "%s: %s\n", #BIT, (VAR & BIT) ? "yes" : "no");\
    } while (0)
    FLOG(flags, NACL_ABI_O_CREAT);
    FLOG(flags, NACL_ABI_O_TRUNC);
    FLOG(flags, NACL_ABI_O_APPEND);
#undef FLOG
  }
  if (NaClAclBypassChecks) {
    return 0;
  }
  return -NACL_ABI_EACCES;
}

/*
 * NaClStatAclCheck: Is the NaCl app authorized to stat this pathname?  The
 * return value is syscall return convention, so 0 is success and
 * small negative numbers are negated errno values.
 *
 * This is primarily for debug use.  File access should be through
 * SRPC-based file servers.
 */
int32_t NaClStatAclCheck(struct NaClApp *nap,
                         char const     *path) {
  NaClLog(2,
          "NaClStatAclCheck(0x%08"NACL_PRIxPTR", %s)\n", (uintptr_t) nap, path);
  if (NaClAclBypassChecks) {
    return 0;
  }
  return -NACL_ABI_EACCES;
}

static uint32_t CopyPathFromUser(struct NaClApp *nap,
                                 char           *dest,
                                 size_t         num_bytes,
                                 uintptr_t      src) {
  /*
   * NaClCopyInFromUserZStr may (try to) get bytes that is outside the
   * app's address space and generate a fault.
   */
  if (!NaClCopyInFromUserZStr(nap, dest, num_bytes, src)) {
    if (dest[0] == '\0') {
      NaClLog(LOG_ERROR, "NaClSys: invalid address for pathname\n");
      return -NACL_ABI_EFAULT;
    }

    NaClLog(LOG_ERROR, "NaClSys: pathname string too long\n");
    return -NACL_ABI_ENAMETOOLONG;
  }

  return 0;
}

int32_t NaClSysOpen(struct NaClAppThread  *natp,
                    char                  *pathname,
                    int                   flags,
                    int                   mode) {
  struct NaClApp       *nap = natp->nap;
  uint32_t             retval = -NACL_ABI_EINVAL;
  char                 path[NACL_CONFIG_PATH_MAX];
  nacl_host_stat_t     stbuf;
  int                  allowed_flags;

  NaClLog(3, "NaClSysOpen(0x%08"NACL_PRIxPTR", "
          "0x%08"NACL_PRIxPTR", 0x%x, 0x%x)\n",
          (uintptr_t) natp, (uintptr_t) pathname, flags, mode);

  retval = CopyPathFromUser(nap, path, sizeof path, (uintptr_t) pathname);
  if (0 != retval)
    goto cleanup;

  allowed_flags = (NACL_ABI_O_ACCMODE | NACL_ABI_O_CREAT
                   | NACL_ABI_O_TRUNC | NACL_ABI_O_APPEND);
  if (0 != (flags & ~allowed_flags)) {
    NaClLog(LOG_WARNING, "Invalid open flags 0%o, ignoring extraneous bits\n",
            flags);
    flags &= allowed_flags;
  }
  if (0 != (mode & ~0600)) {
    NaClLog(1, "IGNORING Invalid access mode bits 0%o\n", mode);
    mode &= 0600;
  }

  retval = NaClOpenAclCheck(nap, path, flags, mode);
  if (0 != retval) {
    NaClLog(3, "Open ACL check rejected \"%s\".\n", path);
    goto cleanup;
  }

  /*
   * Perform a stat to determine whether the file is a directory.
   *
   * NB: it is okay for the stat to fail, since the request may be to
   * create a new file.
   *
   * There is a race conditions here: between the stat and the
   * open-as-a-file and open-as-a-dir, the type of the object that the
   * path refers to can change.
   */
  retval = NaClHostDescStat(path, &stbuf);

  /* Windows does not have S_ISDIR(m) macro */
  if (0 == retval && S_IFDIR == (S_IFDIR & stbuf.st_mode)) {
    struct NaClHostDir  *hd;

    hd = malloc(sizeof *hd);
    if (NULL == hd) {
      retval = -NACL_ABI_ENOMEM;
      goto cleanup;
    }
    retval = NaClHostDirOpen(hd, path);
    NaClLog(1, "NaClHostDirOpen(0x%08"NACL_PRIxPTR", %s) returned %d\n",
            (uintptr_t) hd, path, retval);
    if (0 == retval) {
      retval = NaClSetAvail(nap,
                            ((struct NaClDesc *) NaClDescDirDescMake(hd)));
      NaClLog(1, "Entered directory into open file table at %d\n",
              retval);
    }
  } else {
    struct NaClHostDesc  *hd;

    hd = malloc(sizeof *hd);
    if (NULL == hd) {
      retval = -NACL_ABI_ENOMEM;
      goto cleanup;
    }
    retval = NaClHostDescOpen(hd, path, flags, mode);
    NaClLog(1,
            "NaClHostDescOpen(0x%08"NACL_PRIxPTR", %s, 0%o, 0%o) returned %d\n",
            (uintptr_t) hd, path, flags, mode, retval);
    if (0 == retval) {
      struct NaClDesc *desc = (struct NaClDesc *) NaClDescIoDescMake(hd);
      if ((flags & NACL_ABI_O_ACCMODE) == NACL_ABI_O_RDONLY) {
        /*
         * Let any read-only open be used for PROT_EXEC mmap
         * calls.  Under -a, the user informally warrants that
         * files' code segments won't be changed after open.
         */
        NaClDescSetFlags(desc,
                         NaClDescGetFlags(desc) | NACL_DESC_FLAGS_MMAP_EXEC_OK);
      }
      retval = NaClSetAvail(nap, desc);
      NaClLog(1, "Entered into open file table at %d\n", retval);
    }
  }
cleanup:
  return retval;
}

int32_t NaClSysStat(struct NaClAppThread  *natp,
                    const char            *pathname,
                    struct nacl_abi_stat  *buf) {
  struct NaClApp      *nap = natp->nap;
  int32_t             retval = -NACL_ABI_EINVAL;
  char                path[NACL_CONFIG_PATH_MAX];
  nacl_host_stat_t    stbuf;

  NaClLog(3,
          ("Entered NaClSysStat(0x%08"NACL_PRIxPTR", 0x%08"NACL_PRIxPTR","
           " 0x%08"NACL_PRIxPTR")\n"),
          (uintptr_t) natp, (uintptr_t) pathname, (uintptr_t) buf);

  retval = CopyPathFromUser(nap, path, sizeof path, (uintptr_t) pathname);
  if (0 != retval)
    goto cleanup;

  retval = NaClStatAclCheck(nap, path);
  if (0 != retval)
    goto cleanup;

  /*
   * Perform a host stat.
   */
  retval = NaClHostDescStat(path, &stbuf);
  if (0 == retval) {
    struct nacl_abi_stat abi_stbuf;

    retval = NaClAbiStatHostDescStatXlateCtor(&abi_stbuf,
                                              &stbuf);
    if (!NaClCopyOutToUser(nap, (uintptr_t) buf,
                           &abi_stbuf, sizeof abi_stbuf)) {
      retval = -NACL_ABI_EFAULT;
    }
  }
cleanup:
  return retval;
}

int32_t NaClSysMkdir(struct NaClAppThread *natp,
                     uint32_t             pathname,
                     int                  mode) {
  struct NaClApp *nap = natp->nap;
  char           path[NACL_CONFIG_PATH_MAX];
  int32_t        retval = -NACL_ABI_EINVAL;

  if (!NaClAclBypassChecks) {
    retval = -NACL_ABI_EACCES;
    goto cleanup;
  }

  retval = CopyPathFromUser(nap, path, sizeof path, pathname);
  if (0 != retval)
    goto cleanup;

  retval = NaClHostDescMkdir(path, mode);
cleanup:
  return retval;
}

int32_t NaClSysRmdir(struct NaClAppThread *natp,
                     uint32_t             pathname) {
  struct NaClApp *nap = natp->nap;
  char           path[NACL_CONFIG_PATH_MAX];
  int32_t        retval = -NACL_ABI_EINVAL;

  if (!NaClAclBypassChecks) {
    retval = -NACL_ABI_EACCES;
    goto cleanup;
  }

  retval = CopyPathFromUser(nap, path, sizeof path, pathname);
  if (0 != retval)
    goto cleanup;

  retval = NaClHostDescRmdir(path);
cleanup:
  return retval;
}

int32_t NaClSysChdir(struct NaClAppThread *natp,
                     uint32_t             pathname) {
  struct NaClApp *nap = natp->nap;
  char           path[NACL_CONFIG_PATH_MAX];
  int32_t        retval = -NACL_ABI_EINVAL;

  if (!NaClAclBypassChecks) {
    retval = -NACL_ABI_EACCES;
    goto cleanup;
  }

  retval = CopyPathFromUser(nap, path, sizeof path, pathname);
  if (0 != retval)
    goto cleanup;

  retval = NaClHostDescChdir(path);
cleanup:
  return retval;
}

int32_t NaClSysGetcwd(struct NaClAppThread *natp,
                      uint32_t             buffer,
                      int                  len) {
  struct NaClApp *nap = natp->nap;
  int32_t        retval = -NACL_ABI_EINVAL;
  char           path[NACL_CONFIG_PATH_MAX];

  if (!NaClAclBypassChecks) {
    retval = -NACL_ABI_EACCES;
    goto cleanup;
  }

  if (len >= NACL_CONFIG_PATH_MAX)
    len = NACL_CONFIG_PATH_MAX - 1;

  retval = NaClHostDescGetcwd(path, len);
  if (retval != 0)
    goto cleanup;

  if (!NaClCopyOutToUser(nap, buffer, &path, strlen(path) + 1))
    retval = -NACL_ABI_EFAULT;

cleanup:
  return retval;
}

int32_t NaClSysUnlink(struct NaClAppThread *natp,
                      uint32_t             pathname) {
  struct NaClApp *nap = natp->nap;
  char           path[NACL_CONFIG_PATH_MAX];
  int32_t        retval = -NACL_ABI_EINVAL;

  if (!NaClAclBypassChecks) {
    retval = -NACL_ABI_EACCES;
    goto cleanup;
  }

  retval = CopyPathFromUser(nap, path, sizeof path, pathname);
  if (0 != retval)
    goto cleanup;

  retval = NaClHostDescUnlink(path);
cleanup:
  return retval;
}
