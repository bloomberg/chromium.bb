/*
 * Copyright 2010 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can
 * be found in the LICENSE file.
 */

int main() {
  /* Run forever.  This is useful for testing that the plugin can shut
     down this process OK.  If this process is left behind by the
     plugin, it will be more obvious if it is consuming CPU than if it
     is sleeping! */
  while (1) { }

  return 0;
}
