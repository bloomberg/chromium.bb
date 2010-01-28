/*
 * Copyright 2009 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */


#include "native_client/src/trusted/platform_qualify/nacl_os_qualify.h"

/*
 * Dummy: always true or not applicable
 */
int NaClOsIsSupported() {
  return 1;
}


/*
 * Dummy: always true or not applicable
 */
int NaClOsIs64BitWindows() {
  return 1;
}

/*
 * Dummy: always true or not applicable
 */
int NaClOsRestoresLdt() {
  return 1;
}
