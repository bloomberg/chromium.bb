/* Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file. */

#ifndef LIBRARIES_NACL_IO_INCLUDE_SYS_MOUNT_H
#define LIBRARIES_NACL_IO_INCLUDE_SYS_MOUNT_H

#include <sys/cdefs.h>

__BEGIN_DECLS

int mount(const char* source,
          const char* target,
          const char* filesystemtype,
          unsigned long mountflags,
          const void* data);

int umount(const char* target);

__END_DECLS

#endif  /* LIBRARIES_NACL_IO_INCLUDE_SYS_MOUNT_H */
