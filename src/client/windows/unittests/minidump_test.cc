// Copyright (c) 2010, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <windows.h>
#include <objbase.h>
#include <dbghelp.h>

#include "gtest/gtest.h"

namespace {

// Convenience to get to the PEB pointer in a TEB.
struct FakeTEB {
  char dummy[0x30];
  void* peb;
};

// Minidump with stacks, PEB, TEB, and unloaded module list.
const MINIDUMP_TYPE kSmallDumpType = static_cast<MINIDUMP_TYPE>(
    MiniDumpWithProcessThreadData |  // Get PEB and TEB.
    MiniDumpWithUnloadedModules);  // Get unloaded modules when available.

// Minidump with all of the above, plus memory referenced from stack.
const MINIDUMP_TYPE kLargerDumpType = static_cast<MINIDUMP_TYPE>(
    MiniDumpWithProcessThreadData |  // Get PEB and TEB.
    MiniDumpWithUnloadedModules |  // Get unloaded modules when available.
    MiniDumpWithIndirectlyReferencedMemory);  // Get memory referenced by stack.

// Large dump with all process memory.
const MINIDUMP_TYPE kFullDumpType = static_cast<MINIDUMP_TYPE>(
    MiniDumpWithFullMemory |  // Full memory from process.
    MiniDumpWithProcessThreadData |  // Get PEB and TEB.
    MiniDumpWithHandleData |  // Get all handle information.
    MiniDumpWithUnloadedModules);  // Get unloaded modules when available.

class MinidumpTest: public testing::Test {
 public:
  MinidumpTest() : dump_file_view_(NULL), dump_file_handle_(NULL),
      dump_file_mapping_(NULL) {
  }

  virtual void SetUp() {
    // Make sure URLMon isn't loaded into our process.
    ASSERT_EQ(NULL, ::GetModuleHandle(L"urlmon.dll"));

    // Then load and unload it to ensure we have something to
    // stock the unloaded module list with.
    HMODULE urlmon = ::LoadLibrary(L"urlmon.dll");
    ASSERT_TRUE(urlmon != NULL);
    ASSERT_TRUE(::FreeLibrary(urlmon));

    wchar_t temp_dir_path[ MAX_PATH ] = {0};
    wchar_t dump_file_name[ MAX_PATH ] = {0};
    ::GetTempPath(MAX_PATH, temp_dir_path);
    ::GetTempFileName(temp_dir_path, L"tst", 0, dump_file_name);
    dump_file_ = dump_file_name;
    dump_file_handle_ = ::CreateFile(dump_file_.c_str(),
                                     GENERIC_WRITE | GENERIC_READ,
                                     0,
                                     NULL,
                                     OPEN_EXISTING,
                                     0,
                                     NULL);
    ASSERT_TRUE(dump_file_handle_ != NULL);
  }

  virtual void TearDown() {
    if (dump_file_view_ != NULL) {
      EXPECT_TRUE(::UnmapViewOfFile(dump_file_view_));
      ::CloseHandle(dump_file_mapping_);
      dump_file_mapping_ = NULL;
    }

    ::CloseHandle(dump_file_handle_);
    dump_file_handle_ = NULL;

    EXPECT_TRUE(::DeleteFile(dump_file_.c_str()));
  }

  void EnsureDumpMapped() {
    ASSERT_TRUE(dump_file_handle_ != NULL);
    if (dump_file_view_ == NULL) {
      ASSERT_TRUE(dump_file_mapping_ == NULL);

      dump_file_mapping_ = ::CreateFileMapping(dump_file_handle_,
                                               NULL,
                                               PAGE_READONLY,
                                               0,
                                               0,
                                               NULL);
      ASSERT_TRUE(dump_file_mapping_ != NULL);

      dump_file_view_ = ::MapViewOfFile(dump_file_mapping_,
                                        FILE_MAP_READ,
                                        0,
                                        0,
                                        0);
      ASSERT_TRUE(dump_file_view_ != NULL);
    }
  }

  bool WriteDump(ULONG flags) {
    // Fake exception is access violation on write to this.
    EXCEPTION_RECORD ex_record = {
        STATUS_ACCESS_VIOLATION,  // ExceptionCode
        0,  // ExceptionFlags
        NULL,  // ExceptionRecord;
        reinterpret_cast<void*>(0xCAFEBABE),  // ExceptionAddress;
        2,  // NumberParameters;
        { EXCEPTION_WRITE_FAULT, reinterpret_cast<ULONG_PTR>(this) }
    };
    CONTEXT ctx_record = {};
    EXCEPTION_POINTERS ex_ptrs = {
      &ex_record,
      &ctx_record,
    };
    MINIDUMP_EXCEPTION_INFORMATION ex_info = {
        ::GetCurrentThreadId(),
        &ex_ptrs,
        FALSE,
    };

    // Capture our register context.
    ::RtlCaptureContext(&ctx_record);

    // And write a dump
    BOOL result = ::MiniDumpWriteDump(::GetCurrentProcess(),
                                      ::GetCurrentProcessId(),
                                      dump_file_handle_,
                                      static_cast<MINIDUMP_TYPE>(flags),
                                      &ex_info,
                                      NULL,
                                      NULL);
    return result == TRUE;
  }

  bool DumpHasStream(ULONG stream_number) {
    EnsureDumpMapped();

    MINIDUMP_DIRECTORY* directory = NULL;
    void* stream = NULL;
    ULONG stream_size = 0;
    BOOL ret = ::MiniDumpReadDumpStream(dump_file_view_,
                                        stream_number,
                                        &directory,
                                        &stream,
                                        &stream_size);

    return ret != FALSE && stream != NULL && stream_size > 0;
  }

  template <class StreamType>
  size_t GetStream(ULONG stream_number, StreamType** stream) {
    EnsureDumpMapped();
    MINIDUMP_DIRECTORY* directory = NULL;
    ULONG memory_list_size = 0;
    BOOL ret = ::MiniDumpReadDumpStream(dump_file_view_,
                                        stream_number,
                                        &directory,
                                        reinterpret_cast<void**>(stream),
                                        &memory_list_size);

    return ret ? memory_list_size : 0;
  }

  bool DumpHasTebs() {
    MINIDUMP_THREAD_LIST* thread_list = NULL;
    size_t thread_list_size = GetStream(ThreadListStream, &thread_list);

    if (thread_list_size > 0 && thread_list != NULL) {
      for (ULONG i = 0; i < thread_list->NumberOfThreads; ++i) {
        if (!DumpHasMemory(thread_list->Threads[i].Teb))
          return false;
      }

      return true;
    }

    // No thread list, no TEB info.
    return false;
  }

  bool DumpHasPeb() {
    MINIDUMP_THREAD_LIST* thread_list = NULL;
    size_t thread_list_size = GetStream(ThreadListStream, &thread_list);

    if (thread_list_size > 0 && thread_list != NULL &&
        thread_list->NumberOfThreads > 0) {
      FakeTEB* teb = NULL;
      if (!DumpHasMemory(thread_list->Threads[0].Teb, &teb))
        return false;

      return DumpHasMemory(teb->peb);
    }

    return false;
  }

  bool DumpHasMemory(ULONG64 address) {
    return DumpHasMemory<BYTE>(address, NULL);
  }

  bool DumpHasMemory(const void* address) {
    return DumpHasMemory<BYTE>(address, NULL);
  }

  template <class StructureType>
  bool DumpHasMemory(ULONG64 address, StructureType** structure = NULL) {
    // We can't cope with 64 bit addresses for now.
    if (address > 0xFFFFFFFFUL)
      return false;

    return DumpHasMemory(reinterpret_cast<void*>(address), structure);
  }

  template <class StructureType>
  bool DumpHasMemory(const void* addr_in, StructureType** structure = NULL) {
    uintptr_t address = reinterpret_cast<uintptr_t>(addr_in);
    MINIDUMP_MEMORY_LIST* memory_list = NULL;
    size_t memory_list_size = GetStream(MemoryListStream, &memory_list);
    if (memory_list_size > 0 && memory_list != NULL) {
      for (ULONG i = 0; i < memory_list->NumberOfMemoryRanges; ++i) {
        MINIDUMP_MEMORY_DESCRIPTOR& descr = memory_list->MemoryRanges[i];
        const uintptr_t range_start =
            static_cast<uintptr_t>(descr.StartOfMemoryRange);
        uintptr_t range_end = range_start + descr.Memory.DataSize;

        if (address >= range_start &&
            address + sizeof(StructureType) < range_end) {
          // The start address falls in the range, and the end address is
          // in bounds, return a pointer to the structure if requested.
          if (structure != NULL)
            *structure = reinterpret_cast<StructureType*>(
                RVA_TO_ADDR(dump_file_view_, descr.Memory.Rva));

          return true;
        }
      }
    }

    // We didn't find the range in a MINIDUMP_MEMORY_LIST, so maybe this
    // is a full dump using MINIDUMP_MEMORY64_LIST with all the memory at the
    // end of the dump file.
    MINIDUMP_MEMORY64_LIST* memory64_list = NULL;
    memory_list_size = GetStream(Memory64ListStream, &memory64_list);
    if (memory_list_size > 0 && memory64_list != NULL) {
      // Keep track of where the current descriptor maps to.
      RVA64 curr_rva = memory64_list->BaseRva;
      for (ULONG i = 0; i < memory64_list->NumberOfMemoryRanges; ++i) {
        MINIDUMP_MEMORY_DESCRIPTOR64& descr = memory64_list->MemoryRanges[i];
        uintptr_t range_start =
            static_cast<uintptr_t>(descr.StartOfMemoryRange);
        uintptr_t range_end = range_start + static_cast<size_t>(descr.DataSize);

        if (address >= range_start &&
            address + sizeof(StructureType) < range_end) {
          // The start address falls in the range, and the end address is
          // in bounds, return a pointer to the structure if requested.
          if (structure != NULL)
            *structure = reinterpret_cast<StructureType*>(
                RVA_TO_ADDR(dump_file_view_, curr_rva));

          return true;
        }

        // Advance the current RVA.
        curr_rva += descr.DataSize;
      }
    }



    return false;
  }

 protected:
  HANDLE dump_file_handle_;
  HANDLE dump_file_mapping_;
  void* dump_file_view_;

  std::wstring dump_file_;
};

// We need to be able to get file information from Windows
bool HasFileInfo(const std::wstring& file_path) {
  DWORD dummy;
  const wchar_t* path = file_path.c_str();
  DWORD length = ::GetFileVersionInfoSize(path, &dummy);
  if (length == 0)
    return NULL;

  void* data = calloc(length, 1);
  if (!data)
    return false;

  if (!::GetFileVersionInfo(path, dummy, length, data)) {
    free(data);
    return false;
  }

  void* translate = NULL;
  UINT page_count;
  BOOL query_result = VerQueryValue(
      data,
      L"\\VarFileInfo\\Translation",
      static_cast<void**>(&translate),
      &page_count);

  free(data);
  if (query_result && translate) {
    return true;
  } else {
    return false;
  }
}

TEST_F(MinidumpTest, Version) {
  API_VERSION* version = ::ImagehlpApiVersion();

  HMODULE dbg_help = ::GetModuleHandle(L"dbghelp.dll");
  ASSERT_TRUE(dbg_help != NULL);

  wchar_t dbg_help_file[1024] = {};
  ASSERT_TRUE(::GetModuleFileName(dbg_help,
                                  dbg_help_file,
                                  sizeof(dbg_help_file) /
                                      sizeof(*dbg_help_file)));
  ASSERT_TRUE(HasFileInfo(std::wstring(dbg_help_file)) != NULL);

//  LOG(INFO) << "DbgHelp.dll version: " << file_info->file_version();
}

TEST_F(MinidumpTest, Normal) {
  EXPECT_TRUE(WriteDump(MiniDumpNormal));

  // We expect threads, modules and some memory.
  EXPECT_TRUE(DumpHasStream(ThreadListStream));
  EXPECT_TRUE(DumpHasStream(ModuleListStream));
  EXPECT_TRUE(DumpHasStream(MemoryListStream));
  EXPECT_TRUE(DumpHasStream(ExceptionStream));
  EXPECT_TRUE(DumpHasStream(SystemInfoStream));
  EXPECT_TRUE(DumpHasStream(MiscInfoStream));

  EXPECT_FALSE(DumpHasStream(ThreadExListStream));
  EXPECT_FALSE(DumpHasStream(Memory64ListStream));
  EXPECT_FALSE(DumpHasStream(CommentStreamA));
  EXPECT_FALSE(DumpHasStream(CommentStreamW));
  EXPECT_FALSE(DumpHasStream(HandleDataStream));
  EXPECT_FALSE(DumpHasStream(FunctionTableStream));
  EXPECT_FALSE(DumpHasStream(UnloadedModuleListStream));
  EXPECT_FALSE(DumpHasStream(MemoryInfoListStream));
  EXPECT_FALSE(DumpHasStream(ThreadInfoListStream));
  EXPECT_FALSE(DumpHasStream(HandleOperationListStream));
  EXPECT_FALSE(DumpHasStream(TokenStream));

  // We expect no PEB nor TEBs in this dump.
  EXPECT_FALSE(DumpHasTebs());
  EXPECT_FALSE(DumpHasPeb());

  // We expect no off-stack memory in this dump.
  EXPECT_FALSE(DumpHasMemory(this));
}

TEST_F(MinidumpTest, SmallDump) {
  ASSERT_TRUE(WriteDump(kSmallDumpType));

  EXPECT_TRUE(DumpHasStream(ThreadListStream));
  EXPECT_TRUE(DumpHasStream(ModuleListStream));
  EXPECT_TRUE(DumpHasStream(MemoryListStream));
  EXPECT_TRUE(DumpHasStream(ExceptionStream));
  EXPECT_TRUE(DumpHasStream(SystemInfoStream));
  EXPECT_TRUE(DumpHasStream(UnloadedModuleListStream));
  EXPECT_TRUE(DumpHasStream(MiscInfoStream));

  // We expect PEB and TEBs in this dump.
  EXPECT_TRUE(DumpHasTebs());
  EXPECT_TRUE(DumpHasPeb());

  EXPECT_FALSE(DumpHasStream(ThreadExListStream));
  EXPECT_FALSE(DumpHasStream(Memory64ListStream));
  EXPECT_FALSE(DumpHasStream(CommentStreamA));
  EXPECT_FALSE(DumpHasStream(CommentStreamW));
  EXPECT_FALSE(DumpHasStream(HandleDataStream));
  EXPECT_FALSE(DumpHasStream(FunctionTableStream));
  EXPECT_FALSE(DumpHasStream(MemoryInfoListStream));
  EXPECT_FALSE(DumpHasStream(ThreadInfoListStream));
  EXPECT_FALSE(DumpHasStream(HandleOperationListStream));
  EXPECT_FALSE(DumpHasStream(TokenStream));

  // We expect no off-stack memory in this dump.
  EXPECT_FALSE(DumpHasMemory(this));
}

TEST_F(MinidumpTest, LargerDump) {
  ASSERT_TRUE(WriteDump(kLargerDumpType));

  // The dump should have all of these streams.
  EXPECT_TRUE(DumpHasStream(ThreadListStream));
  EXPECT_TRUE(DumpHasStream(ModuleListStream));
  EXPECT_TRUE(DumpHasStream(MemoryListStream));
  EXPECT_TRUE(DumpHasStream(ExceptionStream));
  EXPECT_TRUE(DumpHasStream(SystemInfoStream));
  EXPECT_TRUE(DumpHasStream(UnloadedModuleListStream));
  EXPECT_TRUE(DumpHasStream(MiscInfoStream));

  // We expect memory referenced by stack in this dump.
  EXPECT_TRUE(DumpHasMemory(this));

  // We expect PEB and TEBs in this dump.
  EXPECT_TRUE(DumpHasTebs());
  EXPECT_TRUE(DumpHasPeb());

  EXPECT_FALSE(DumpHasStream(ThreadExListStream));
  EXPECT_FALSE(DumpHasStream(Memory64ListStream));
  EXPECT_FALSE(DumpHasStream(CommentStreamA));
  EXPECT_FALSE(DumpHasStream(CommentStreamW));
  EXPECT_FALSE(DumpHasStream(HandleDataStream));
  EXPECT_FALSE(DumpHasStream(FunctionTableStream));
  EXPECT_FALSE(DumpHasStream(MemoryInfoListStream));
  EXPECT_FALSE(DumpHasStream(ThreadInfoListStream));
  EXPECT_FALSE(DumpHasStream(HandleOperationListStream));
  EXPECT_FALSE(DumpHasStream(TokenStream));
}

TEST_F(MinidumpTest, FullDump) {
  ASSERT_TRUE(WriteDump(kFullDumpType));

  // The dump should have all of these streams.
  EXPECT_TRUE(DumpHasStream(ThreadListStream));
  EXPECT_TRUE(DumpHasStream(ModuleListStream));
  EXPECT_TRUE(DumpHasStream(Memory64ListStream));
  EXPECT_TRUE(DumpHasStream(ExceptionStream));
  EXPECT_TRUE(DumpHasStream(SystemInfoStream));
  EXPECT_TRUE(DumpHasStream(UnloadedModuleListStream));
  EXPECT_TRUE(DumpHasStream(MiscInfoStream));
  EXPECT_TRUE(DumpHasStream(HandleDataStream));

  // We expect memory referenced by stack in this dump.
  EXPECT_TRUE(DumpHasMemory(this));

  // We expect PEB and TEBs in this dump.
  EXPECT_TRUE(DumpHasTebs());
  EXPECT_TRUE(DumpHasPeb());

  EXPECT_FALSE(DumpHasStream(ThreadExListStream));
  EXPECT_FALSE(DumpHasStream(MemoryListStream));
  EXPECT_FALSE(DumpHasStream(CommentStreamA));
  EXPECT_FALSE(DumpHasStream(CommentStreamW));
  EXPECT_FALSE(DumpHasStream(FunctionTableStream));
  EXPECT_FALSE(DumpHasStream(MemoryInfoListStream));
  EXPECT_FALSE(DumpHasStream(ThreadInfoListStream));
  EXPECT_FALSE(DumpHasStream(HandleOperationListStream));
  EXPECT_FALSE(DumpHasStream(TokenStream));
}

}  // namespace
