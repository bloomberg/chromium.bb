/*
 * Copyright 2014 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <pthread.h>
#define main main_chameneos
/* override stacksize -- chameneos.c sets it too small for nacl glibc */
#define pthread_attr_setstacksize(a,b) pthread_attr_setstacksize((a),1024*1024)
#include "native_client/src/third_party/computer_language_benchmarks_game/chameneos.c"
#undef main
