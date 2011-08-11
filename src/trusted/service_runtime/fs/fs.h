/*
 * Copyright 2008 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

/*
 * NaCl Service Runtime Virtual Filesystem Support Interface.
 *
 * NB: this code has not been integrated with the rest of NaCl, and is
 * likely to change.
 *
 * We *really* need the low-level IMC module (../../src/shared/imc)
 * to provide a robust non-blocking / asynchronous interface.  Current
 * NACL_DONT_WAIT flag is completely bogus, since on some platforms
 * (e.g, OSX and Windows) the flag is ignored.
 *
 * For now, we burn a thread per server-based filesystem.  If that
 * thread has problems, we can time out in the upper-layer's
 * condition wait, leaving the thread stuck.
 *
 * Much of the design is patterned after NFSv2, except using an IMC
 * transport instead of UDP.
 */

#ifndef NATIVE_CLIENT_SERVICE_RUNTIME_FS_FS_H_
#define NATIVE_CLIENT_SERVICE_RUNTIME_FS_FS_H_

#include "native_client/src/include/nacl_base.h"

#include "native_client/src/trusted/service_runtime/include/machine/_types.h"

#include "native_client/src/trusted/desc/nacl_desc_base.h"

EXTERN_C_BEGIN

#define NACL_MAXDATA      (64 << 10)
#define NACL_MAXPATHLEN   1024
#define NACL_MAXNAMLEN    255
#define NACL_COOKIESIZE   4
#define NACL_FHSIZE       32
/*
 * 256 bits, so 2 AES blocks if encryption is desired.  also a random
 * value is good enough for some use cases.  we assume a secure
 * channel in any case, so only unspoofability by the client is needed
 * -- so if encryption is used, semantically secure encryption is
 * required.  since clients are service runtime implementations, and
 * NaCl modules cannot directly access this channel, spoofing is not a
 * threat.  (if we allow NaCl modules to get the IMC socket and invoke
 * the nacl_mount operation itself, then this becomes an issue.)
 */

struct NaClFileSystem;

/*
 * An inode within the context of a NaClFileSystem.  Essentialy
 * contains the (implemented) fields of nacl_abi_stat.  This is
 * essentially what is read from/written to filesystems, though when
 * using SRPC sans XDR or protobufs, the structure members are
 * manually serialized/deserialized.
 */
struct NaClFileAttr {
  nacl_abi_dev_t        dev;    /* fsid */
  nacl_abi_ino_t        ino;    /* fileid; a per filesystem value */
  nacl_abi_mode_t       mode;
  nacl_abi_uid_t        uid;    /* origin ID?  presistent mapping to URI? */
  nacl_abi_gid_t        gid;    /* ??? */
  nacl_abi_nlink_t      nlink;  /* on-disk ref count */
  nacl_abi_off_t        size;
  nacl_abi_time_t       atime;  /* access time */
  nacl_abi_time_t       mtime;  /* modification time */
  nacl_abi_time_t       ctime;  /* inode change time */
  /*
   * no need to ensure padding / structure size is the same on all
   * platforms, since we won't be running over a real network.
   */
};

/*
 * some fs/inodes tuples are mount points, so whenever we see an inode
 * number in a directory read operation, we need to check the
 * per-filesystem list of mount points.  when a filesystem is
 * unmounted, there should be no mount points within that file system.
 */

/*
 * Runtime / in-memory representation of an inode.  The filesystem in
 * which the inode is contained is determined by using the dev member
 * of the inode.
 */
struct NaClMemFileAttr {
  struct NaClFileAttr     fa;
  struct NaClDescConnCap  *cc;  /* if an IMC-based device */

  /*
   * NB: instead of mounting a filesystem, we can associate an existing
   * connection capability with an inode and make that appear to be a
   * filesystem object.  this is just a special kind of filesystem
   * object that contains a single entry.  this allows (some) servers
   * to advertise their availability, using the filesystem as a name
   * server.  the client is responsible for figuring out what the
   * service is via service discovery, and of course the client should
   * not necessarily trust the server.
   */
};

/*
 * First, a NaClFileSystem base class.  We will have the in-memory
 * filesystem for the root, with files made available to the NaCl
 * module from the browser plugin (via the secure command channel), as
 * well as external file servers that provide filesystem services via
 * SRPC.  We must not trust such servers even though they normally would
 * nominally be trusted processes -- all input should be validated.
 */

struct NaClFileSystemVtbl;  /* fwd */
struct Container;  /* fwd */

struct NaClFileSystem {
  struct NaClFileSystemVtbl *vtbl;
  struct NaClMutex          mu;
  uint32_t                  ref_count;
  uint64_t                  open_files_count;
  /*
   * device number is assigned by NaCl, do not depend on it having a
   * particular value.  in particular, no major/minor device number
   * encoding should be assumed.  (NB: do not make available to user
   * apps for now.)
   */
  nacl_abi_dev_t            dev;
  /* where is filesystem mounted? mountpt is NULL for the root filesystem */
  struct NaClMemInode       *mountpt;
  /*
   * What inodes are mount points?
   */
  struct Container          *mountpt_inos;
};

void NaClFileSystemIncRef(struct NaClFileSystem *fsp);
void NaClFileSystemDecRef(struct NaClFileSystem *fsp);

struct NaClFileSystemVtbl {
  void              (*Dtor)(struct NaClFileSystem *fsp);

  void              (*IncRef)(struct NaClFileSystem *fsp);
  /* used when mounting */
  void              (*DecRef)(struct NaClFileSystem *fsp);
  /* used when unmounting */

  /*
   * On-disk ref count, deletion/gc, esp if file server has multiple
   * clients.  These are used, for example, for open(..., O_CREAT) and
   * unlink syscalls.  We inc refcount before modifying the containing
   * directory, and dec the refcount after modifying the containing
   * directory -- assuming no re-ordering of filesystem operations,
   * this ensures that if an object's refcount is inconsistent, it
   * would just result in an inode (and associated data blocks) which
   * is inaccessible, rather than being put into a free list while it
   * is still accessible.
   */
  int               (*RefIno)(struct NaClFileSystem  *fsp,
                              nacl_abi_ino_t         inum);
  int               (*UnrefIno)(struct NaClFileSystem *fsp,
                                nacl_abi_ino_t        inum);

  /*
   * In-memory refcount.  Can prevent freeing of data blocks
   * associated with an inode, even though the on-disk ref count is
   * zero.  These are used, for example, for open and close syscalls,
   * so that open followed by unlink would not gc the inode and
   * associated data blocks.  If the inode has been unlinked between
   * the directory lookup and the unlink, DynRefIno may return
   * -NACL_ABI_EACCES.
   */
  int               (*DynRefIno)(struct NaClFileSystem  *fsp,
                                 nacl_abi_ino_t         inum);
  int               (*DynUnrefIno)(struct NaClFileSystem  *fsp,
                                   nacl_abi_ino_t         inum);

  /* meta data */
  int               (*ReadIno)(struct NaClFileSystem  *fsp,
                               nacl_abi_ino_t         inum,
                               struct NaClFileAttr    *nfap);
  int               (*WriteIno)(struct NaClFileSystem *fsp,
                                nacl_abi_ino_t        inum,
                                struct NaClFileSystem *nfap);

  /*
   * data, pread/pwrite-like interface.  there's no notion of current
   * file pointer/seek, since offset is explicit
   */
  nacl_abi_ssize_t  (*Read)(struct NaClFileSystem *fsp,
                            nacl_abi_ino_t        inum,
                            void                  *buf,
                            nacl_abi_size_t       count,
                            nacl_abi_off_t        offset);
  nacl_abi_ssize_t  (*Write)(struct NaClFileSystem  *fsp,
                             nacl_abi_ino_t         inum,
                             void const             *buf,
                             nacl_abi_size_t        count,
                             nacl_abi_off_t         offset);
  /* mmap?, mount? */
};

EXTERN_C_END

#endif
