/*
 * Copyright (c) 2011 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

const char* hello();

int fortytwo();

/*
 * The two functions below or proxies for the functions above that live
 * in different libraries
 */

const char* hello2();

int fortytwo2();
