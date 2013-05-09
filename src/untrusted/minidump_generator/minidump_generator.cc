/*
 * Copyright (c) 2013 The Native Client Authors. All rights reserved.
 * Use of this source code is governed by a BSD-style license that can be
 * found in the LICENSE file.
 */

#include <assert.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>

#include <algorithm>

#include "breakpad/src/google_breakpad/common/minidump_format.h"
#include "native_client/src/include/elf_constants.h"
#include "native_client/src/include/nacl/nacl_exception.h"
#include "native_client/src/include/nacl/nacl_minidump.h"
#include "native_client/src/untrusted/irt/irt.h"


extern char __executable_start[];  // Start of code segment
extern char __etext[];  // End of code segment

// @IGNORE_LINES_FOR_CODE_HYGIENE[1]
#if defined(__GLIBC__)
// Variable defined by ld.so, used as a workaround for
// https://code.google.com/p/nativeclient/issues/detail?id=3431.
extern void *__libc_stack_end;
#endif

class MinidumpFileWriter;

// Restrict how much of the stack we dump to reduce upload size and to
// avoid dynamic allocation.
static const size_t kLimitStackDumpSize = 512 * 1024;

static const size_t kLimitNonStackSize = 64 * 1024;

// The crash reporter is expected to be used in a situation where the
// current process is damaged or out of memory, so it avoids dynamic
// memory allocation and allocates a fixed-size buffer of the
// following size at startup.
static const size_t kMinidumpBufferSize =
    kLimitStackDumpSize + kLimitNonStackSize;

static const char *g_module_name = "main.nexe";
static MDGUID g_module_build_id;
static nacl_minidump_callback_t g_callback_func;
static MinidumpFileWriter *g_minidump_writer;
static int g_handling_exception = 0;


class MinidumpFileWriter {
  char *buf_;
  uint32_t buf_size_;
  uint32_t offset_;

 public:
  MinidumpFileWriter() : buf_(NULL), buf_size_(0), offset_(0) {
    void *mapping = mmap(NULL, kMinidumpBufferSize, PROT_READ | PROT_WRITE,
                         MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (mapping == MAP_FAILED) {
      fprintf(stderr, "minidump: Failed to preallocate memory\n");
      return;
    }
    buf_ = (char *) mapping;
    buf_size_ = kMinidumpBufferSize;
  }

  bool AllocateSpace(size_t size, char **ptr, uint32_t *position) {
    if (offset_ + size >= buf_size_)
      return false;
    *position = offset_;
    *ptr = buf_ + offset_;
    offset_ += size;
    memset(*ptr, 0, size);
    return true;
  }

  char *data() { return buf_; }
  size_t size() { return offset_; }
};

// TypedMDRVA represents a minidump object chunk.  This interface is
// based on the TypedMDRVA class in the Breakpad codebase.  Breakpad's
// implementation writes directly to a file, whereas this
// implementation constructs the minidump file in memory.
template<typename MDType>
class TypedMDRVA {
  MinidumpFileWriter *writer_;
  char *ptr_;
  uint32_t position_;
  size_t size_;

 public:
  explicit TypedMDRVA(MinidumpFileWriter *writer) :
      writer_(writer),
      ptr_(NULL),
      position_(0),
      size_(0) {}

  // Allocates space for MDType.
  bool Allocate() {
    return AllocateArray(1);
  }

  // Allocates an array of |count| elements of MDType.
  bool AllocateArray(size_t count) {
    size_ = sizeof(MDType) * count;
    return writer_->AllocateSpace(size_, &ptr_, &position_);
  }

  // Allocates an array of |count| elements of |size| after object of MDType.
  bool AllocateObjectAndArray(size_t count, size_t length) {
    size_ = sizeof(MDType) + count * length;
    return writer_->AllocateSpace(size_, &ptr_, &position_);
  }

  // Copy |item| to |index|.  Must have been allocated using AllocateArray().
  void CopyIndexAfterObject(unsigned int index, void *src, size_t length) {
    size_t offset = sizeof(MDType) + index * length;
    assert(offset + length <= size_);
    memcpy(ptr_ + offset, src, length);
  }

  MDType *get() { return (MDType *) ptr_; }

  uint32_t position() { return position_; }

  MDLocationDescriptor location() {
    MDLocationDescriptor location = { size_, position_ };
    return location;
  }
};


static void ConvertRegisters(MinidumpFileWriter *minidump_writer,
                             struct NaClExceptionContext *context,
                             MDRawThread *thread) {
  NaClExceptionPortableContext *pcontext =
      nacl_exception_context_get_portable(context);
#define COPY_REG(REG) regs.get()->REG = src_regs->REG
  switch (context->arch) {
    case EM_386: {
      struct NaClUserRegisterStateX8632 *src_regs =
          (struct NaClUserRegisterStateX8632 *) &context->regs;
      TypedMDRVA<MDRawContextX86> regs(minidump_writer);
      if (!regs.Allocate())
        return;
      thread->thread_context = regs.location();
      // TODO(mseaborn): Report x87/SSE registers too.
      regs.get()->context_flags =
        MD_CONTEXT_X86_CONTROL | MD_CONTEXT_X86_INTEGER;
      COPY_REG(edi);
      COPY_REG(esi);
      COPY_REG(ebx);
      COPY_REG(edx);
      COPY_REG(ecx);
      COPY_REG(eax);
      COPY_REG(ebp);
      regs.get()->eip = src_regs->prog_ctr;
      regs.get()->eflags = src_regs->flags;
      regs.get()->esp = src_regs->stack_ptr;
      break;
    }
    case EM_X86_64: {
      struct NaClUserRegisterStateX8664 *src_regs =
          (struct NaClUserRegisterStateX8664 *) &context->regs;
      TypedMDRVA<MDRawContextAMD64> regs(minidump_writer);
      if (!regs.Allocate())
        return;
      thread->thread_context = regs.location();
      // TODO(mseaborn): Report x87/SSE registers too.
      regs.get()->context_flags =
        MD_CONTEXT_AMD64_CONTROL | MD_CONTEXT_AMD64_INTEGER;
      regs.get()->eflags = src_regs->flags;
      COPY_REG(rax);
      COPY_REG(rcx);
      COPY_REG(rdx);
      COPY_REG(rbx);
      regs.get()->rsp = pcontext->stack_ptr;
      regs.get()->rbp = pcontext->frame_ptr;
      COPY_REG(rsi);
      COPY_REG(rdi);
      COPY_REG(r8);
      COPY_REG(r9);
      COPY_REG(r10);
      COPY_REG(r11);
      COPY_REG(r12);
      COPY_REG(r13);
      COPY_REG(r14);
      COPY_REG(r15);
      regs.get()->rip = pcontext->prog_ctr;
      break;
    }
    case EM_ARM: {
      struct NaClUserRegisterStateARM *src_regs =
          (struct NaClUserRegisterStateARM *) &context->regs;
      TypedMDRVA<MDRawContextARM> regs(minidump_writer);
      if (!regs.Allocate())
        return;
      thread->thread_context = regs.location();
      for (int regnum = 0; regnum < 16; regnum++) {
        regs.get()->iregs[regnum] = ((uint32_t *) &src_regs->r0)[regnum];
      }
      regs.get()->cpsr = src_regs->cpsr;
      break;
    }
    default: {
      // Architecture not recognized.  Dump the register state anyway.
      // Maybe we should do this on all architectures, and Breakpad
      // should be adapted to read NaCl's portable container format
      // for register state so that the client code is
      // architecture-neutral.
      TypedMDRVA<uint8_t> regs(minidump_writer);
      if (!regs.AllocateArray(context->size))
        return;
      thread->thread_context = regs.location();
      memcpy(regs.get(), &context, context->size);
      break;
    }
  }
#undef COPY_REG
}

static MDMemoryDescriptor SnapshotMemory(MinidumpFileWriter *minidump_writer,
                                         uintptr_t start, size_t size) {
  TypedMDRVA<uint8_t> mem_copy(minidump_writer);
  MDMemoryDescriptor desc = {0};
  if (mem_copy.AllocateArray(size)) {
    memcpy(mem_copy.get(), (void *) start, size);

    desc.start_of_memory_range = start;
    desc.memory = mem_copy.location();
  }
  return desc;
}

static bool GetStackEnd(void **stack_end) {
  // @IGNORE_LINES_FOR_CODE_HYGIENE[1]
#if defined(__GLIBC__)
  void *stack_base;
  size_t stack_size;
  pthread_attr_t attr;
  if (pthread_getattr_np(pthread_self(), &attr) == 0 &&
      pthread_attr_getstack(&attr, &stack_base, &stack_size) == 0) {
    *stack_end = (void *) ((char *) stack_base + stack_size);
    pthread_attr_destroy(&attr);
    return true;
  }
  // pthread_getattr_np() currently fails on the initial thread.  As a
  // workaround, if we reach here, assume we are on the initial thread
  // and get the initial thread's stack end as recorded by glibc.
  // See https://code.google.com/p/nativeclient/issues/detail?id=3431
  *stack_end = __libc_stack_end;
  return true;
#else
  return pthread_get_stack_end_np(pthread_self(), stack_end) == 0;
#endif
}

static void WriteThreadList(MinidumpFileWriter *minidump_writer,
                            MDRawDirectory *dirent,
                            struct NaClExceptionContext *context) {
  // This records only the thread that crashed.
  // TODO(mseaborn): Record other threads too.  This will require NaCl
  // to provide an interface for suspending threads.
  TypedMDRVA<uint32_t> list(minidump_writer);
  int num_threads = 1;
  if (!list.AllocateObjectAndArray(num_threads, sizeof(MDRawThread)))
    return;
  *list.get() = num_threads;

  MDRawThread thread = {0};
  ConvertRegisters(minidump_writer, context, &thread);

  // Record the stack contents.
  NaClExceptionPortableContext *pcontext =
      nacl_exception_context_get_portable(context);
  uintptr_t stack_start = pcontext->stack_ptr;
  if (context->arch == EM_X86_64) {
    // Include the x86-64 red zone too to capture local variables.
    stack_start -= 128;
  }
  void *stack_end;
  if (GetStackEnd(&stack_end) && stack_start <= (uintptr_t) stack_end) {
    size_t stack_size = (uintptr_t) stack_end - stack_start;
    stack_size = std::min(stack_size, kLimitStackDumpSize);
    thread.stack = SnapshotMemory(minidump_writer, stack_start, stack_size);
  }

  list.CopyIndexAfterObject(0, &thread, sizeof(thread));

  dirent->stream_type = MD_THREAD_LIST_STREAM;
  dirent->location = list.location();
}

static int MinidumpArchFromElfMachine(int e_machine) {
  switch (e_machine) {
    case EM_386:     return MD_CPU_ARCHITECTURE_X86;
    case EM_X86_64:  return MD_CPU_ARCHITECTURE_AMD64;
    case EM_ARM:     return MD_CPU_ARCHITECTURE_ARM;
    case EM_MIPS:    return MD_CPU_ARCHITECTURE_MIPS;
    default:         return MD_CPU_ARCHITECTURE_UNKNOWN;
  }
}

static void WriteSystemInfo(MinidumpFileWriter *minidump_writer,
                            MDRawDirectory *dirent,
                            struct NaClExceptionContext *context) {
  TypedMDRVA<MDRawSystemInfo> sysinfo(minidump_writer);
  if (!sysinfo.Allocate())
    return;
  sysinfo.get()->processor_architecture =
      MinidumpArchFromElfMachine(context->arch);
  sysinfo.get()->platform_id = MD_OS_NACL;
  dirent->stream_type = MD_SYSTEM_INFO_STREAM;
  dirent->location = sysinfo.location();
}

static uint32_t WriteString(MinidumpFileWriter *minidump_writer,
                            const char *string) {
  int string_length = strlen(string);
  TypedMDRVA<uint32_t> obj(minidump_writer);
  if (!obj.AllocateObjectAndArray(string_length + 1, sizeof(uint16_t)))
    return 0;
  *obj.get() = string_length * sizeof(uint16_t);
  for (int i = 0; i < string_length + 1; ++i) {
    ((MDString *) obj.get())->buffer[i] = string[i];
  }
  return obj.position();
}

static void WriteModuleList(MinidumpFileWriter *minidump_writer,
                            MDRawDirectory *dirent) {
  // This assumes static linking and reports a single module.
  // TODO(mseaborn): Extend this to handle dynamic linking and use
  // dl_iterate_phdr() to report loaded libraries.
  // TODO(mseaborn): Report the IRT's build ID here too, once the IRT
  // provides an interface for querying it.
  TypedMDRVA<uint32_t> module_list(minidump_writer);
  int module_count = 1;
  if (!module_list.AllocateObjectAndArray(module_count, MD_MODULE_SIZE))
    return;
  *module_list.get() = module_count;

  TypedMDRVA<MDCVInfoPDB70> cv(minidump_writer);
  int name_length = strlen(g_module_name);
  if (!cv.AllocateObjectAndArray(name_length + 1, sizeof(char)))
    return;
  cv.get()->cv_signature = MD_CVINFOPDB70_SIGNATURE;
  cv.get()->signature = g_module_build_id;
  memcpy(cv.get()->pdb_file_name, g_module_name, name_length + 1);

  MDRawModule module = {0};
  module.base_of_image = (uintptr_t) &__executable_start;
  module.size_of_image = (uintptr_t) &__etext - (uintptr_t) &__executable_start;
  module.module_name_rva = WriteString(minidump_writer, g_module_name);
  module.cv_record = cv.location();
  module_list.CopyIndexAfterObject(0, &module, MD_MODULE_SIZE);

  dirent->stream_type = MD_MODULE_LIST_STREAM;
  dirent->location = module_list.location();
}

static void WriteMinidump(MinidumpFileWriter *minidump_writer,
                          struct NaClExceptionContext *context) {
  const int kNumWriters = 3;
  TypedMDRVA<MDRawHeader> header(minidump_writer);
  TypedMDRVA<MDRawDirectory> dir(minidump_writer);
  if (!header.Allocate())
    return;
  if (!dir.AllocateArray(kNumWriters))
    return;
  header.get()->signature = MD_HEADER_SIGNATURE;
  header.get()->version = MD_HEADER_VERSION;
  header.get()->time_date_stamp = time(NULL);
  header.get()->stream_count = kNumWriters;
  header.get()->stream_directory_rva = dir.position();

  int dir_index = 0;
  WriteThreadList(minidump_writer, &dir.get()[dir_index++], context);
  WriteSystemInfo(minidump_writer, &dir.get()[dir_index++], context);
  WriteModuleList(minidump_writer, &dir.get()[dir_index++]);
  assert(dir_index == kNumWriters);
}

static void CrashHandler(struct NaClExceptionContext *context) {
  static const char msg[] = "minidump: Caught crash\n";
  write(2, msg, sizeof(msg) - 1);

  // Prevent re-entering the crash handler if two crashes occur
  // concurrently.  We preallocate storage that cannot be used
  // concurrently.  We avoid using a pthread mutex here in case
  // libpthread's data structures are corrupted.
  if (__sync_lock_test_and_set(&g_handling_exception, 1)) {
    // Wait forever here so that the first crashing thread can report
    // the crash and exit.
    for (;;)
      sleep(9999);
  }

  MinidumpFileWriter *minidump_writer = g_minidump_writer;
  WriteMinidump(minidump_writer, context);

  if (g_callback_func)
    g_callback_func(minidump_writer->data(), minidump_writer->size());

  // Flush streams to aid debugging, although since the process might
  // be in a corrupted state this might crash.
  fflush(NULL);

  _exit(127);
}

void nacl_minidump_register_crash_handler(void) {
  struct nacl_irt_exception_handling irt_exception_handling;
  if (nacl_interface_query(NACL_IRT_EXCEPTION_HANDLING_v0_1,
                           &irt_exception_handling,
                           sizeof(irt_exception_handling))
      != sizeof(irt_exception_handling)) {
    fprintf(stderr, "minidump: Exception handling IRT interface not present\n");
    return;
  }
  if (irt_exception_handling.exception_handler(CrashHandler, NULL) != 0) {
    fprintf(stderr, "minidump: Failed to register an exception handler\n");
    return;
  }
  g_minidump_writer = new MinidumpFileWriter();
}

void nacl_minidump_set_callback(nacl_minidump_callback_t callback) {
  g_callback_func = callback;
}

void nacl_minidump_set_module_name(const char *module_name) {
  g_module_name = module_name;
}

void nacl_minidump_set_module_build_id(
    const uint8_t data[NACL_MINIDUMP_BUILD_ID_SIZE]) {
  assert(sizeof(g_module_build_id) == NACL_MINIDUMP_BUILD_ID_SIZE);
  memcpy(&g_module_build_id, data, NACL_MINIDUMP_BUILD_ID_SIZE);
}
