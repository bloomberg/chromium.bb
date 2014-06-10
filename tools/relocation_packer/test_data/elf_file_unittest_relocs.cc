// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Test data for packing/unpacking.  When compiled, creates a run of
// relative relocations.
//
// #!/bin/bash
//
// # Compile as an arm shared library.
// /usr/bin/arm-linux-gnueabi-g++ -shared -o /tmp/testdata.so \
//   elf_file_unittest_relocs.cc
//
// # Add a new null .android.rel.dyn section, needed for packing.
// echo 'NULL' >/tmp/small
// /usr/bin/arm-linux-gnueabi-objcopy \
//   --add-section .android.rel.dyn=/tmp/small /tmp/testdata.so
//
// # Create packed and unpacked reference files.
// packer="../../../out_android/Debug/relocation_packer"
// cp /tmp/testdata.so elf_file_unittest_relocs_packed.so
// $packer elf_file_unittest_relocs_packed.so
// cp elf_file_unittest_relocs_packed.so elf_file_unittest_relocs.so
// $packer -u elf_file_unittest_relocs.so
//
// # Clean up.
// rm /tmp/testdata.so /tmp/small

const int i = 0;

const void* pointer_0 = &i;
const void* pointer_1 = &i;
const void* pointer_2 = &i;
const void* pointer_3 = &i;
const void* pointer_4 = &i;
const void* pointer_5 = &i;
const void* pointer_6 = &i;
const void* pointer_7 = &i;
const void* pointer_8 = &i;
const void* pointer_9 = &i;
