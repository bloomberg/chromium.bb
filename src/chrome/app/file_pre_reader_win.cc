// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/app/file_pre_reader_win.h"

#include <windows.h>

#include <memoryapi.h>  // NOLINT(build/include_order)

#include "base/files/file.h"
#include "base/files/memory_mapped_file.h"

void PreReadFile(const base::FilePath& file_path) {
  // On Win8 and higher use ::PrefetchVirtualMemory(). This is better than
  // a simple data file read, more from a RAM perspective than CPU. This is
  // because reading the file as data results in double mapping to
  // Image/executable pages for all pages of code executed. On Win7 just do a
  // simple file read as data.

  // ::PrefetchVirtualMemory() isn't available on Win7.
  HMODULE kernel32_library = GetModuleHandleA("kernel32.dll");

  auto prefetch_virtual_memory =
      reinterpret_cast<decltype(&::PrefetchVirtualMemory)>(
          GetProcAddress(kernel32_library, "PrefetchVirtualMemory"));

  if (!prefetch_virtual_memory) {
    // On Win7 read in the file as data since the OS doesn't have
    // the support for better options.

    constexpr DWORD kStepSize = 1024 * 1024;

    base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                   base::File::FLAG_SEQUENTIAL_SCAN);
    if (!file.IsValid())
      return;

    char* buffer = reinterpret_cast<char*>(
        ::VirtualAlloc(nullptr, kStepSize, MEM_COMMIT, PAGE_READWRITE));
    if (!buffer)
      return;

    while (file.ReadAtCurrentPos(buffer, kStepSize) > 0) {
    }

    ::VirtualFree(buffer, 0, MEM_RELEASE);
  } else {
    // NB: Creating the file mapping before the ::LoadLibrary() of the file is
    // more efficient memory wise, but we must be sure no other threads try to
    // loadlibrary the file while we are doing the mapping and prefetching or
    // the process will get a private copy of the DLL via COW.

    base::MemoryMappedFile mapped_file;
    if (mapped_file.Initialize(file_path,
                               base::MemoryMappedFile::READ_CODE_IMAGE)) {
      _WIN32_MEMORY_RANGE_ENTRY address_range = {mapped_file.data(),
                                                 mapped_file.length()};

      // NB: PrefetchVirtualMemory requires the file to be opened with
      // only read access or it will fail.

      (*prefetch_virtual_memory)(GetCurrentProcess(), 1, &address_range, 0);
    }
  }
}
