/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

/*
 * This is a dummy linker script used as crt1.o when linking with libgcc_eh.
 * It should not be used for creating stable bitcode pexes, which are not
 * supposed to export the symbol for libgcc_eh.
 *
 * The actual startup code is just the _start function defined in a library.
 * We provide this file for two purposes:
 * 1. To keep with the traditional linking sequence that puts crt1.o first.
 * 2. To generate references to the main and exit symbols like the real
 *    startup code would, so that they will be brought in from libraries
 *    before -lc is encountered in the link.  Otherwise a main defined in
 *    a library wouldn't be referenced until after that library had already
 *    been examined, and _start's call would get an undefined reference.
 */

EXTERN ( main exit _exit )

/*
 * We're linking with libgcc_eh, so these symbols have to be exported from
 * the C library contained in the pexe, since libgcc_eh needs them.
 */
EXTERN ( malloc free strlen abort )
