// Copyright (c) 2011 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stdio.h>
#include <nacl/ppruntime.h>

int main(int argc, char* argv[]) {
  printf("Hello from before the PpapiPluginMain routine starts.\n");
  return PpapiPluginMain();
}
