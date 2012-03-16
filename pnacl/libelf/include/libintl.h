/*
 * Copyright (c) 2012 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */
#ifndef __DUMMY_LIBINTL_H
#define __DUMMY_LIBINTL_H

/* Use a dummy libintl.h file to disable NLS. This works around an added
 * dependency on libintl when linking LLVM (or other consumers of libelf),
 * esp. on cygwin. This means that libelf error messages will not be
 * translated to the current domain.
 */

#define gettext(s)  (s)
#define dgettext(_dom, _id) (_id)

#endif
