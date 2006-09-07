// Copyright (C) 2006 Google Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

// minidump.cc: A minidump reader.
//
// See minidump.h for documentation.
//
// Author: Mark Mentovai


#include <stdio.h>
#include <time.h>
#include <unistd.h>
#ifdef _WIN32
#include <io.h>
typedef SSIZE_T ssize_t;
#define read _read
#define lseek _lseek
#endif // _WIN32

#include <map>
#include <memory>
#include <vector>

#include "processor/minidump.h"


namespace google_airbag {


using std::auto_ptr;
using std::vector;


//
// Swapping routines
//
// Inlining these doesn't increase code size significantly, and it saves
// a whole lot of unnecessary jumping back and forth.
//


// Swapping an 8-bit quantity is a no-op.  This function is only provided
// to account for certain templatized operations that require swapping for
// wider types but handle u_int8_t too
// (MinidumpMemoryRegion::GetMemoryAtAddressInternal).
static inline void Swap(u_int8_t* value) {
}


// Optimization: don't need to AND the furthest right shift, because we're
// shifting an unsigned quantity.  The standard requires zero-filling in this
// case.  If the quantities were signed, a bitmask whould be needed for this
// right shift to avoid an arithmetic shift (which retains the sign bit).
// The furthest left shift never needs to be ANDed bitmask.


static inline void Swap(u_int16_t* value) {
  *value = (*value >> 8) |
           (*value << 8);
}


static inline void Swap(u_int32_t* value) {
  *value =  (*value >> 24) |
           ((*value >> 8)  & 0x0000ff00) |
           ((*value << 8)  & 0x00ff0000) |
            (*value << 24);
}


static inline void Swap(u_int64_t* value) {
  *value =  (*value >> 56) |
           ((*value >> 40) & 0x000000000000ff00LL) |
           ((*value >> 24) & 0x0000000000ff0000LL) |
           ((*value >> 8)  & 0x00000000ff000000LL) |
           ((*value << 8)  & 0x000000ff00000000LL) |
           ((*value << 24) & 0x0000ff0000000000LL) |
           ((*value << 40) & 0x00ff000000000000LL) |
            (*value << 56);
}


static inline void Swap(MDLocationDescriptor* location_descriptor) {
  Swap(&location_descriptor->data_size);
  Swap(&location_descriptor->rva);
}


static inline void Swap(MDMemoryDescriptor* memory_descriptor) {
  Swap(&memory_descriptor->start_of_memory_range);
  Swap(&memory_descriptor->memory);
}


static inline void Swap(MDGUID* guid) {
  Swap(&guid->data1);
  Swap(&guid->data2);
  Swap(&guid->data3);
  // Don't swap guid->data4[] because it contains 8-bit quantities.
}


//
// Character conversion routines
//


// Standard wide-character conversion routines depend on the system's own
// idea of what width a wide character should be: some use 16 bits, and
// some use 32 bits.  For the purposes of a minidump, wide strings are
// always represented with 16-bit UTF-16 chracters.  iconv isn't available
// everywhere, and its interface varies where it is available.  iconv also
// deals purely with char* pointers, so in addition to considering the swap
// parameter, a converter that uses iconv would also need to take the host
// CPU's endianness into consideration.  It doesn't seems worth the trouble
// of making it a dependency when we don't care about anything but UTF-16.
static string* UTF16ToUTF8(const vector<u_int16_t>& in,
                           bool                     swap) {
  auto_ptr<string> out(new string());

  // Set the string's initial capacity to the number of UTF-16 characters,
  // because the UTF-8 representation will always be at least this long.
  // If the UTF-8 representation is longer, the string will grow dynamically.
  out->reserve(in.size());

  for (vector<u_int16_t>::const_iterator iterator = in.begin();
       iterator != in.end();
       ++iterator) {
    // Get a 16-bit value from the input
    u_int16_t in_word = *iterator;
    if (swap)
      Swap(&in_word);

    // Convert the input value (in_word) into a Unicode code point (unichar).
    u_int32_t unichar;
    if (in_word >= 0xdc00 && in_word <= 0xdcff) {
      // Low surrogate not following high surrogate, fail.
      return NULL;
    } else if (in_word >= 0xd800 && in_word <= 0xdbff) {
      // High surrogate.
      unichar = (in_word - 0xd7c0) << 10;
      if (++iterator == in.end()) {
        // End of input
        return NULL;
      }
      in_word = *iterator;
      if (in_word < 0xdc00 || in_word > 0xdcff) {
        // Expected low surrogate, found something else
        return NULL;
      }
      unichar |= in_word & 0x03ff;
    } else {
      // The ordinary case, a single non-surrogate Unicode character encoded
      // as a single 16-bit value.
      unichar = in_word;
    }

    // Convert the Unicode code point (unichar) into its UTF-8 representation,
    // appending it to the out string.
    if (unichar < 0x80) {
      (*out) += unichar;
    } else if (unichar < 0x800) {
      (*out) += 0xc0 | (unichar >> 6);
      (*out) += 0x80 | (unichar & 0x3f);
    } else if (unichar < 0x10000) {
      (*out) += 0xe0 | (unichar >> 12);
      (*out) += 0x80 | ((unichar >> 6) & 0x3f);
      (*out) += 0x80 | (unichar & 0x3f);
    } else if (unichar < 0x200000) {
      (*out) += 0xf0 | (unichar >> 18);
      (*out) += 0x80 | ((unichar >> 12) & 0x3f);
      (*out) += 0x80 | ((unichar >> 6) & 0x3f);
      (*out) += 0x80 | (unichar & 0x3f);
    } else {
      // Some (high) value that's not (presently) defined in UTF-8
      return NULL;
    }
  }

  return out.release();
}


//
// MinidumpObject
//


MinidumpObject::MinidumpObject(Minidump* minidump)
    : minidump_(minidump),
      valid_(false) {
}


//
// MinidumpStream
//


MinidumpStream::MinidumpStream(Minidump* minidump)
    : MinidumpObject(minidump) {
}


//
// MinidumpContext
//


MinidumpContext::MinidumpContext(Minidump* minidump)
    : MinidumpStream(minidump),
      context_() {
}


bool MinidumpContext::Read(u_int32_t expected_size) {
  valid_ = false;

  if (expected_size != sizeof(context_))
    return false;

  if (!minidump_->ReadBytes(&context_, sizeof(context_)))
    return false;

  if (minidump_->swap()) {
    Swap(&context_.context_flags);
    Swap(&context_.dr0);
    Swap(&context_.dr1);
    Swap(&context_.dr2);
    Swap(&context_.dr3);
    Swap(&context_.dr6);
    Swap(&context_.dr7);
    Swap(&context_.float_save.control_word);
    Swap(&context_.float_save.status_word);
    Swap(&context_.float_save.tag_word);
    Swap(&context_.float_save.error_offset);
    Swap(&context_.float_save.error_selector);
    Swap(&context_.float_save.data_offset);
    Swap(&context_.float_save.data_selector);
    // context_.float_save.register_area[] contains 8-bit quantities and does
    // not need to be swapped.
    Swap(&context_.float_save.cr0_npx_state);
    Swap(&context_.gs);
    Swap(&context_.fs);
    Swap(&context_.es);
    Swap(&context_.ds);
    Swap(&context_.edi);
    Swap(&context_.esi);
    Swap(&context_.ebx);
    Swap(&context_.edx);
    Swap(&context_.ecx);
    Swap(&context_.eax);
    Swap(&context_.ebp);
    Swap(&context_.eip);
    Swap(&context_.cs);
    Swap(&context_.eflags);
    Swap(&context_.esp);
    Swap(&context_.ss);
    // context_.extended_registers[] contains 8-bit quantities and does not
    // need to be swapped.
  }

  valid_ = true;
  return true;
}


void MinidumpContext::Print() {
  if (!valid_)
    return;

  printf("MDRawContextX86\n");
  printf("  context_flags                = 0x%x\n", context_.context_flags);
  printf("  dr0                          = 0x%x\n", context_.dr0);
  printf("  dr1                          = 0x%x\n", context_.dr1);
  printf("  dr2                          = 0x%x\n", context_.dr2);
  printf("  dr3                          = 0x%x\n", context_.dr3);
  printf("  dr6                          = 0x%x\n", context_.dr6);
  printf("  dr7                          = 0x%x\n", context_.dr7);
  printf("  float_save.control_word      = 0x%x\n",
         context_.float_save.control_word);
  printf("  float_save.status_word       = 0x%x\n",
         context_.float_save.status_word);
  printf("  float_save.tag_word          = 0x%x\n",
         context_.float_save.tag_word);
  printf("  float_save.error_offset      = 0x%x\n",
         context_.float_save.error_offset);
  printf("  float_save.error_selector    = 0x%x\n",
         context_.float_save.error_selector);
  printf("  float_save.data_offset       = 0x%x\n",
         context_.float_save.data_offset);
  printf("  float_save.data_selector     = 0x%x\n",
         context_.float_save.data_selector);
  printf("  float_save.register_area[%2d] = 0x",
         MD_FLOATINGSAVEAREA_SIZEOF_REGISTERAREA_X86);
  for (unsigned int register_index = 0;
       register_index < MD_FLOATINGSAVEAREA_SIZEOF_REGISTERAREA_X86;
       ++register_index) {
    printf("%02x", context_.float_save.register_area[register_index]);
  }
  printf("\n");
  printf("  float_save.cr0_npx_state     = 0x%x\n",
         context_.float_save.cr0_npx_state);
  printf("  gs                           = 0x%x\n", context_.gs);
  printf("  fs                           = 0x%x\n", context_.fs);
  printf("  es                           = 0x%x\n", context_.es);
  printf("  ds                           = 0x%x\n", context_.ds);
  printf("  edi                          = 0x%x\n", context_.edi);
  printf("  esi                          = 0x%x\n", context_.esi);
  printf("  ebx                          = 0x%x\n", context_.ebx);
  printf("  edx                          = 0x%x\n", context_.edx);
  printf("  ecx                          = 0x%x\n", context_.ecx);
  printf("  eax                          = 0x%x\n", context_.eax);
  printf("  ebp                          = 0x%x\n", context_.ebp);
  printf("  eip                          = 0x%x\n", context_.eip);
  printf("  cs                           = 0x%x\n", context_.cs);
  printf("  eflags                       = 0x%x\n", context_.eflags);
  printf("  esp                          = 0x%x\n", context_.esp);
  printf("  ss                           = 0x%x\n", context_.ss);
  printf("  extended_registers[%3d]      = 0x",
         MD_CONTEXT_SIZEOF_EXTENDED_REGISTERS_X86);
  for (unsigned int register_index = 0;
       register_index < MD_CONTEXT_SIZEOF_EXTENDED_REGISTERS_X86;
       ++register_index) {
    printf("%02x", context_.extended_registers[register_index]);
  }
  printf("\n\n");
}


//
// MinidumpMemoryRegion
//


MinidumpMemoryRegion::MinidumpMemoryRegion(Minidump* minidump)
    : MinidumpObject(minidump),
      descriptor_(NULL),
      memory_(NULL) {
}


MinidumpMemoryRegion::~MinidumpMemoryRegion() {
  delete memory_;
}


void MinidumpMemoryRegion::SetDescriptor(MDMemoryDescriptor* descriptor) {
  descriptor_ = descriptor;
  valid_ = descriptor &&
           (descriptor_->start_of_memory_range +
            descriptor_->memory.data_size) >
           descriptor_->start_of_memory_range;
}


const u_int8_t* MinidumpMemoryRegion::GetMemory() {
  if (!valid_)
    return NULL;

  if (!memory_) {
    if (!minidump_->SeekSet(descriptor_->memory.rva))
      return NULL;

    // TODO(mmentovai): verify rational size!
    auto_ptr<vector<u_int8_t> > memory(
        new vector<u_int8_t>(descriptor_->memory.data_size));

    if (!minidump_->ReadBytes(&(*memory)[0], descriptor_->memory.data_size)) 
      return NULL;

    memory_ = memory.release();
  }

  return &(*memory_)[0];
}


u_int64_t MinidumpMemoryRegion::GetBase() {
  return valid_ ? descriptor_->start_of_memory_range : (u_int64_t)-1;
}


u_int32_t MinidumpMemoryRegion::GetSize() {
  return valid_ ? descriptor_->memory.data_size : 0;
}


void MinidumpMemoryRegion::FreeMemory() {
  delete memory_;
  memory_ = NULL;
}


template<typename T>
bool MinidumpMemoryRegion::GetMemoryAtAddressInternal(u_int64_t address,
                                                      T*        value) {
  if (!valid_ || !value)
    return false;

  if (address < descriptor_->start_of_memory_range ||
      address + sizeof(T) > descriptor_->start_of_memory_range +
                            descriptor_->memory.data_size) {
    return false;
  }

  const u_int8_t* memory = GetMemory();
  if (!memory)
    return false;

  // If the CPU requires memory accesses to be aligned, this can crash.
  // x86 and ppc are able to cope, though.
  *value = *reinterpret_cast<const T*>(
      &memory[address - descriptor_->start_of_memory_range]);

  if (minidump_->swap())
    Swap(value);

  return true;
}


bool MinidumpMemoryRegion::GetMemoryAtAddress(u_int64_t  address,
                                              u_int8_t*  value) {
  return GetMemoryAtAddressInternal(address, value);
}


bool MinidumpMemoryRegion::GetMemoryAtAddress(u_int64_t  address,
                                              u_int16_t* value) {
  return GetMemoryAtAddressInternal(address, value);
}


bool MinidumpMemoryRegion::GetMemoryAtAddress(u_int64_t  address,
                                              u_int32_t* value) {
  return GetMemoryAtAddressInternal(address, value);
}


bool MinidumpMemoryRegion::GetMemoryAtAddress(u_int64_t  address,
                                              u_int64_t* value) {
  return GetMemoryAtAddressInternal(address, value);
}


void MinidumpMemoryRegion::Print() {
  if (!valid_)
    return;

  const u_int8_t* memory = GetMemory();
  if (memory) {
    printf("0x");
    for (unsigned int byte_index = 0;
         byte_index < descriptor_->memory.data_size;
         byte_index++) {
      printf("%02x", memory[byte_index]);
    }
    printf("\n");
  } else {
    printf("No memory\n");
  }
}


//
// MinidumpThread
//


MinidumpThread::MinidumpThread(Minidump* minidump)
    : MinidumpObject(minidump),
      thread_(),
      memory_(NULL),
      context_(NULL) {
}


MinidumpThread::~MinidumpThread() {
  delete memory_;
  delete context_;
}


bool MinidumpThread::Read() {
  // Invalidate cached data.
  delete memory_;
  memory_ = NULL;
  delete context_;
  context_ = NULL;

  valid_ = false;

  if (!minidump_->ReadBytes(&thread_, sizeof(thread_)))
    return false;

  if (minidump_->swap()) {
    Swap(&thread_.thread_id);
    Swap(&thread_.suspend_count);
    Swap(&thread_.priority_class);
    Swap(&thread_.priority);
    Swap(&thread_.teb);
    Swap(&thread_.stack);
    Swap(&thread_.thread_context);
  }

  // Check for base + size overflow or undersize.  A separate size==0
  // check is needed in case base == 0.
  u_int64_t high_address = thread_.stack.start_of_memory_range +
                           thread_.stack.memory.data_size - 1;
  if (thread_.stack.memory.data_size == 0 ||
      high_address < thread_.stack.start_of_memory_range)
    return false;

  memory_ = new MinidumpMemoryRegion(minidump_);
  memory_->SetDescriptor(&thread_.stack);

  valid_ = true;
  return true;
}


MinidumpMemoryRegion* MinidumpThread::GetMemory() {
  return !valid_ ? NULL : memory_;
}


MinidumpContext* MinidumpThread::GetContext() {
  if (!valid_)
    return NULL;

  if (!context_) {
    if (!minidump_->SeekSet(thread_.thread_context.rva))
      return NULL;

    auto_ptr<MinidumpContext> context(new MinidumpContext(minidump_));

    if (!context->Read(thread_.thread_context.data_size))
      return NULL;

    context_ = context.release();
  }

  return context_;
}


u_int32_t MinidumpThread::GetThreadID() {
  return valid_ ? thread_.thread_id : (u_int32_t)-1;
}


void MinidumpThread::Print() {
  if (!valid_)
    return;

  printf("MDRawThread\n");
  printf("  thread_id                   = 0x%x\n",   thread_.thread_id);
  printf("  suspend_count               = %d\n",     thread_.suspend_count);
  printf("  priority_class              = 0x%x\n",   thread_.priority_class);
  printf("  priority                    = 0x%x\n",   thread_.priority);
  printf("  teb                         = 0x%llx\n", thread_.teb);
  printf("  stack.start_of_memory_range = 0x%llx\n",
         thread_.stack.start_of_memory_range);
  printf("  stack.memory.data_size      = 0x%x\n",
         thread_.stack.memory.data_size);
  printf("  stack.memory.rva            = 0x%x\n",   thread_.stack.memory.rva);
  printf("  thread_context.data_size    = 0x%x\n",
         thread_.thread_context.data_size);
  printf("  thread_context.rva          = 0x%x\n",
         thread_.thread_context.rva);

  MinidumpContext* context = GetContext();
  if (context) {
    printf("\n");
    context->Print();
  } else {
    printf("  (no context)\n");
    printf("\n");
  }

  MinidumpMemoryRegion* memory = GetMemory();
  if (memory) {
    printf("Stack\n");
    memory->Print();
  } else {
    printf("No stack\n");
  }
  printf("\n");
}


//
// MinidumpThreadList
//


MinidumpThreadList::MinidumpThreadList(Minidump* minidump)
    : MinidumpStream(minidump),
      id_to_thread_map_(),
      threads_(NULL),
      thread_count_(0) {
}


MinidumpThreadList::~MinidumpThreadList() {
  delete threads_;
}


bool MinidumpThreadList::Read(u_int32_t expected_size) {
  // Invalidate cached data.
  id_to_thread_map_.clear();
  delete threads_;
  threads_ = NULL;
  thread_count_ = 0;

  valid_ = false;

  u_int32_t thread_count;
  if (expected_size < sizeof(thread_count))
    return false;
  if (!minidump_->ReadBytes(&thread_count, sizeof(thread_count)))
    return false;

  if (minidump_->swap())
    Swap(&thread_count);

  if (expected_size != sizeof(thread_count) +
                       thread_count * sizeof(MDRawThread)) {
    return false;
  }

  // TODO(mmentovai): verify rational size!
  auto_ptr<MinidumpThreads> threads(
      new MinidumpThreads(thread_count, MinidumpThread(minidump_)));

  for (unsigned int thread_index = 0;
       thread_index < thread_count;
       ++thread_index) {
    MinidumpThread* thread = &(*threads)[thread_index];

    // Assume that the file offset is correct after the last read.
    if (!thread->Read())
      return false;

    u_int32_t thread_id = thread->GetThreadID();
    if (GetThreadByID(thread_id)) {
      // Another thread with this ID is already in the list.  Data error.
      return false;
    }
    id_to_thread_map_[thread_id] = thread;
  }

  threads_ = threads.release();
  thread_count_ = thread_count;

  valid_ = true;
  return true;
}


MinidumpThread* MinidumpThreadList::GetThreadAtIndex(unsigned int index)
    const {
  if (!valid_ || index >= thread_count_)
    return NULL;

  return &(*threads_)[index];
}


MinidumpThread* MinidumpThreadList::GetThreadByID(u_int32_t thread_id) {
  // Don't check valid_.  Read calls this method before everything is
  // validated.  It is safe to not check valid_ here.
  return id_to_thread_map_[thread_id];
}


void MinidumpThreadList::Print() {
  if (!valid_)
    return;

  printf("MinidumpThreadList\n");
  printf("  thread_count = %d\n", thread_count_);
  printf("\n");

  for (unsigned int thread_index = 0;
       thread_index < thread_count_;
       ++thread_index) {
    printf("thread[%d]\n", thread_index);

    (*threads_)[thread_index].Print();
  }
}


//
// MinidumpModule
//


MinidumpModule::MinidumpModule(Minidump* minidump)
    : MinidumpObject(minidump),
      module_(),
      name_(NULL),
      cv_record_(NULL),
      misc_record_(NULL),
      debug_filename_(NULL) {
}


MinidumpModule::~MinidumpModule() {
  delete name_;
  delete cv_record_;
  delete misc_record_;
  delete debug_filename_;
}


bool MinidumpModule::Read() {
  // Invalidate cached data.
  delete name_;
  name_ = NULL;
  delete cv_record_;
  cv_record_ = NULL;
  delete misc_record_;
  misc_record_ = NULL;
  delete debug_filename_;
  debug_filename_ = NULL;

  valid_ = false;

  if (!minidump_->ReadBytes(&module_, MD_MODULE_SIZE))
    return false;

  if (minidump_->swap()) {
    Swap(&module_.base_of_image);
    Swap(&module_.size_of_image);
    Swap(&module_.checksum);
    Swap(&module_.time_date_stamp);
    Swap(&module_.module_name_rva);
    Swap(&module_.version_info.signature);
    Swap(&module_.version_info.struct_version);
    Swap(&module_.version_info.file_version_hi);
    Swap(&module_.version_info.file_version_lo);
    Swap(&module_.version_info.product_version_hi);
    Swap(&module_.version_info.product_version_lo);
    Swap(&module_.version_info.file_flags_mask);
    Swap(&module_.version_info.file_flags);
    Swap(&module_.version_info.file_os);
    Swap(&module_.version_info.file_type);
    Swap(&module_.version_info.file_subtype);
    Swap(&module_.version_info.file_date_hi);
    Swap(&module_.version_info.file_date_lo);
    Swap(&module_.cv_record);
    Swap(&module_.misc_record);
    // Don't swap reserved fields because their contents are unknown (as
    // are their proper widths).
  }

  // Check for base + size overflow or undersize.  A separate size==0
  // check is needed in case base == 0.
  u_int64_t high_address = module_.base_of_image + module_.size_of_image - 1;
  if (module_.size_of_image == 0 || high_address < module_.base_of_image)
    return false;

  valid_ = true;
  return true;
}


const string* MinidumpModule::GetName() {
  if (!valid_)
    return NULL;

  if (!name_)
    name_ = minidump_->ReadString(module_.module_name_rva);

  return name_;
}


const u_int8_t* MinidumpModule::GetCVRecord() {
  if (!valid_)
    return NULL;

  if (!cv_record_) {
    // Only check against the smallest possible structure size now - recheck
    // if necessary later if the actual structure is larger.
    if (sizeof(MDCVInfoPDB20) > module_.cv_record.data_size)
      return NULL;

    if (!minidump_->SeekSet(module_.cv_record.rva))
      return NULL;

    // TODO(mmentovai): verify rational size!

    // Allocating something that will be accessed as MDCVInfoPDB70 or
    // MDCVInfoPDB20 but is allocated as u_int8_t[] can cause alignment
    // problems.  x86 and ppc are able to cope, though.  This allocation
    // style is needed because the MDCVInfoPDB70 or MDCVInfoPDB20 are
    // variable-sized due to their pdb_file_name fields; these structures
    // are not sizeof(MDCVInfoPDB70) or sizeof(MDCVInfoPDB20) and treating
    // them as such would result in incomplete structures or overruns.
    auto_ptr<vector<u_int8_t> > cv_record(
        new vector<u_int8_t>(module_.cv_record.data_size));

    if (!minidump_->ReadBytes(&(*cv_record)[0], module_.cv_record.data_size))
      return NULL;

    MDCVInfoPDB70* cv_record_70 =
        reinterpret_cast<MDCVInfoPDB70*>(&(*cv_record)[0]);
    u_int32_t signature = cv_record_70->cv_signature;
    if (minidump_->swap())
      Swap(&signature);

    if (signature == MD_CVINFOPDB70_SIGNATURE) {
      // Now that the structure type is known, recheck the size.
      if (sizeof(MDCVInfoPDB70) > module_.cv_record.data_size)
        return NULL;

      if (minidump_->swap()) {
        Swap(&cv_record_70->cv_signature);
        Swap(&cv_record_70->signature);
        Swap(&cv_record_70->age);
        // Don't swap cv_record_70.pdb_file_name because it's an array of 8-bit
        // quanities.  (It's a path, is it UTF-8?)
      }
    } else if (signature == MD_CVINFOPDB20_SIGNATURE) {
      if (minidump_->swap()) {
        MDCVInfoPDB20* cv_record_20 =
         reinterpret_cast<MDCVInfoPDB20*>(&(*cv_record)[0]);
        Swap(&cv_record_20->cv_header.signature);
        Swap(&cv_record_20->cv_header.offset);
        Swap(&cv_record_20->signature);
        Swap(&cv_record_20->age);
        // Don't swap cv_record_20.pdb_file_name because it's an array of 8-bit
        // quantities.  (It's a path, is it UTF-8?)
      }
    } else {
      // Some unknown structure type.  We don't need to bail out here, but we
      // do instead of returning it, because this method guarantees properly
      // swapped data, and data in an unknown format can't possibly be swapped.
      return NULL;
    }

    // The last field of either structure is null-terminated 8-bit character
    // data.  Ensure that it's null-terminated.
    if ((*cv_record)[module_.cv_record.data_size - 1] != '\0')
      return NULL;

    // Store the vector type because that's how storage was allocated, but
    // return it casted to u_int8_t*.
    cv_record_ = cv_record.release();
  }

  return &(*cv_record_)[0];
}


const MDImageDebugMisc* MinidumpModule::GetMiscRecord() {
  if (!valid_)
    return NULL;

  if (!misc_record_) {
    if (sizeof(MDImageDebugMisc) > module_.misc_record.data_size)
      return NULL;

    if (!minidump_->SeekSet(module_.misc_record.rva))
      return NULL;

    // TODO(mmentovai): verify rational size!

    // Allocating something that will be accessed as MDImageDebugMisc but
    // is allocated as u_int8_t[] can cause alignment problems.  x86 and
    // ppc are able to cope, though.  This allocation style is needed
    // because the MDImageDebugMisc is variable-sized due to its data field;
    // this structure is not sizeof(MDImageDebugMisc) and treating it as such
    // would result in an incomplete structure or an overrun.
    auto_ptr<vector<u_int8_t> > misc_record_mem(
        new vector<u_int8_t>(module_.misc_record.data_size));
    MDImageDebugMisc* misc_record =
        reinterpret_cast<MDImageDebugMisc*>(&(*misc_record_mem)[0]);

    if (!minidump_->ReadBytes(misc_record, module_.misc_record.data_size))
      return NULL;

    if (minidump_->swap()) {
      Swap(&misc_record->data_type);
      Swap(&misc_record->length);
      // Don't swap misc_record.unicode because it's an 8-bit quantity.
      // Don't swap the reserved fields for the same reason, and because
      // they don't contain any valid data.
      if (misc_record->unicode) {
        // There is a potential alignment problem, but shouldn't be a problem
        // in practice due to the layout of MDImageDebugMisc.
        u_int16_t* data16 = reinterpret_cast<u_int16_t*>(&(misc_record->data));
        unsigned int dataBytes = module_.misc_record.data_size -
                                 sizeof(MDImageDebugMisc);
        unsigned int dataLength = dataBytes / 2;
        for (unsigned int characterIndex = 0;
             characterIndex < dataLength;
             ++characterIndex) {
          Swap(&data16[characterIndex]);
        }
      }
    }

    if (module_.misc_record.data_size != misc_record->length)
      return NULL;

    // Store the vector type because that's how storage was allocated, but
    // return it casted to MDImageDebugMisc*.
    misc_record_ = misc_record_mem.release();
  }

  return reinterpret_cast<MDImageDebugMisc*>(&(*misc_record_)[0]);
}


// This method will perform no allocation-size checking on its own; it relies
// on GetCVRecord() and GetMiscRecord() to have made the determination that
// the necessary structures aren't oversized.
const string* MinidumpModule::GetDebugFilename() {
  if (!valid_)
    return NULL;

  if (!debug_filename_) {
    // Prefer the CodeView record if present.
    const MDCVInfoPDB70* cv_record_70 =
        reinterpret_cast<const MDCVInfoPDB70*>(GetCVRecord());
    if (cv_record_70) {
      if (cv_record_70->cv_signature == MD_CVINFOPDB70_SIGNATURE) {
        // GetCVRecord guarantees pdb_file_name is null-terminated.
        debug_filename_ = new string(
            reinterpret_cast<const char*>(cv_record_70->pdb_file_name));

        return debug_filename_;
      } else if (cv_record_70->cv_signature == MD_CVINFOPDB20_SIGNATURE) {
        // It's actually a MDCVInfoPDB20 structure.
        const MDCVInfoPDB20* cv_record_20 =
            reinterpret_cast<const MDCVInfoPDB20*>(cv_record_70);

        // GetCVRecord guarantees pdb_file_name is null-terminated.
        debug_filename_ = new string(
            reinterpret_cast<const char*>(cv_record_20->pdb_file_name));

        return debug_filename_;
      }

      // If there's a CodeView record but it doesn't match either of those
      // signatures, try the miscellaneous record - but it's suspicious because
      // GetCVRecord shouldn't have returned a CodeView record that doesn't
      // match either signature.
    }

    // No usable CodeView record.  Try the miscellaneous debug record.
    const MDImageDebugMisc* misc_record = GetMiscRecord();
    if (!misc_record)
      return NULL;

    if (!misc_record->unicode) {
      // If it's not Unicode, just stuff it into the string.  It's unclear
      // if misc_record->data is 0-terminated, so use an explicit size.
      debug_filename_ = new string(
          reinterpret_cast<const char*>(misc_record->data),
          module_.misc_record.data_size - sizeof(MDImageDebugMisc));

      return debug_filename_;
    }

    // There's a misc_record but it encodes the debug filename in UTF-16.
    // (Actually, because miscellaneous records are so old, it's probably
    // UCS-2.)  Convert it to UTF-8 for congruity with the other strings that
    // this method (and all other methods in the Minidump family) return.

    unsigned int bytes =
        module_.misc_record.data_size - sizeof(MDImageDebugMisc);
    if (bytes % 2 != 0)
      return NULL;
    unsigned int utf16_words = bytes / 2;

    // UTF16ToUTF8 expects a vector<u_int16_t>, so create a temporary one and
    // copy the UTF-16 data into it.
    vector<u_int16_t> string_utf16(utf16_words);
    memcpy(&string_utf16[0], &misc_record->data, bytes);

    // GetMiscRecord already byte-swapped the data[] field if it contains
    // UTF-16, so pass false as the swap argument.
    debug_filename_ = UTF16ToUTF8(string_utf16, false);
  }

  return debug_filename_;
}


void MinidumpModule::Print() {
  if (!valid_)
    return;

  printf("MDRawModule\n");
  printf("  base_of_image                   = 0x%llx\n",
         module_.base_of_image);
  printf("  size_of_image                   = 0x%x\n",
         module_.size_of_image);
  printf("  checksum                        = 0x%x\n",
         module_.checksum);
  printf("  time_date_stamp                 = 0x%x\n",
         module_.time_date_stamp);
  printf("  module_name_rva                 = 0x%x\n",
         module_.module_name_rva);
  printf("  version_info.signature          = 0x%x\n",
         module_.version_info.signature);
  printf("  version_info.struct_version     = 0x%x\n",
         module_.version_info.struct_version);
  printf("  version_info.file_version       = 0x%x:0x%x\n",
         module_.version_info.file_version_hi,
         module_.version_info.file_version_lo);
  printf("  version_info.product_version    = 0x%x:0x%x\n",
         module_.version_info.product_version_hi,
         module_.version_info.product_version_lo);
  printf("  version_info.file_flags_mask    = 0x%x\n",
         module_.version_info.file_flags_mask);
  printf("  version_info.file_flags         = 0x%x\n",
         module_.version_info.file_flags);
  printf("  version_info.file_os            = 0x%x\n",
         module_.version_info.file_os);
  printf("  version_info.file_type          = 0x%x\n",
         module_.version_info.file_type);
  printf("  version_info.file_subtype       = 0x%x\n",
         module_.version_info.file_subtype);
  printf("  version_info.file_date          = 0x%x:0x%x\n",
         module_.version_info.file_date_hi,
         module_.version_info.file_date_lo);
  printf("  cv_record.data_size             = %d\n",
         module_.cv_record.data_size);
  printf("  cv_record.rva                   = 0x%x\n",
         module_.cv_record.rva);
  printf("  misc_record.data_size           = %d\n",
         module_.misc_record.data_size);
  printf("  misc_record.rva                 = 0x%x\n",
         module_.misc_record.rva);

  const char* module_name = GetName()->c_str();
  if (module_name)
    printf("  (module_name)                   = \"%s\"\n", module_name);
  else
    printf("  (module_name)                   = (null)\n");

  const MDCVInfoPDB70* cv_record =
      reinterpret_cast<const MDCVInfoPDB70*>(GetCVRecord());
  if (cv_record) {
    if (cv_record->cv_signature == MD_CVINFOPDB70_SIGNATURE) {
      printf("  (cv_record).cv_signature        = 0x%x\n",
             cv_record->cv_signature);
      printf("  (cv_record).signature           = %08x-%04x-%04x-%02x%02x-",
             cv_record->signature.data1,
             cv_record->signature.data2,
             cv_record->signature.data3,
             cv_record->signature.data4[0],
             cv_record->signature.data4[1]);
      for (unsigned int guidIndex = 2;
           guidIndex < 8;
           ++guidIndex) {
        printf("%02x", cv_record->signature.data4[guidIndex]);
      }
      printf("\n");
      printf("  (cv_record).age                 = %d\n",
             cv_record->age);
      printf("  (cv_record).pdb_file_name       = \"%s\"\n",
             cv_record->pdb_file_name);
    } else {
      const MDCVInfoPDB20* cv_record_20 =
       reinterpret_cast<const MDCVInfoPDB20*>(cv_record);
      printf("  (cv_record).cv_header.signature = 0x%x\n",
             cv_record_20->cv_header.signature);
      printf("  (cv_record).cv_header.offset    = 0x%x\n",
             cv_record_20->cv_header.offset);
      printf("  (cv_record).signature           = 0x%x\n",
             cv_record_20->signature);
      printf("  (cv_record).age                 = %d\n",
             cv_record_20->age);
      printf("  (cv_record).pdb_file_name       = \"%s\"\n",
             cv_record_20->pdb_file_name);
    }
  } else {
    printf("  (cv_record)                     = (null)\n");
  }

  const MDImageDebugMisc* misc_record = GetMiscRecord();
  if (misc_record) {
    printf("  (misc_record).data_type         = 0x%x\n",
           misc_record->data_type);
    printf("  (misc_record).length            = 0x%x\n",
           misc_record->length);
    printf("  (misc_record).unicode           = %d\n",
           misc_record->unicode);
    // Don't bother printing the UTF-16, we don't really even expect to ever
    // see this misc_record anyway.
    if (misc_record->unicode)
      printf("  (misc_record).data              = \"%s\"\n",
             misc_record->data);
    else
      printf("  (misc_record).data              = (UTF-16)\n");
  } else {
    printf("  (misc_record)                   = (null)\n");
  }

  const string* debug_filename = GetDebugFilename();
  if (debug_filename) {
    printf("  (debug_filename)                = \"%s\"\n",
           debug_filename->c_str());
  } else {
    printf("  (debug_filename)                = (null)\n");
  }
  printf("\n");
}


//
// MinidumpModuleList
//


MinidumpModuleList::MinidumpModuleList(Minidump* minidump)
    : MinidumpStream(minidump),
      range_map_(),
      modules_(NULL),
      module_count_(0) {
}


MinidumpModuleList::~MinidumpModuleList() {
  delete modules_;
}


bool MinidumpModuleList::Read(u_int32_t expected_size) {
  // Invalidate cached data.
  range_map_.Clear();
  delete modules_;
  modules_ = NULL;
  module_count_ = 0;

  valid_ = false;

  u_int32_t module_count;
  if (expected_size < sizeof(module_count))
    return false;
  if (!minidump_->ReadBytes(&module_count, sizeof(module_count)))
    return false;

  if (minidump_->swap())
    Swap(&module_count);

  if (expected_size != sizeof(module_count) +
                       module_count * MD_MODULE_SIZE) {
    return false;
  }

  // TODO(mmentovai): verify rational size!
  auto_ptr<MinidumpModules> modules(
      new MinidumpModules(module_count, MinidumpModule(minidump_)));

  for (unsigned int module_index = 0;
       module_index < module_count;
       ++module_index) {
    MinidumpModule* module = &(*modules)[module_index];

    // Assume that the file offset is correct after the last read.
    if (!module->Read())
      return false;

    u_int64_t base_address = module->base_address();
    u_int64_t module_size = module->size();
    if (base_address == (u_int64_t)-1)
      return false;

    if (!range_map_.StoreRange(base_address, module_size, module_index))
      return false;
  }

  modules_ = modules.release();
  module_count_ = module_count;

  valid_ = true;
  return true;
}


MinidumpModule* MinidumpModuleList::GetModuleAtIndex(unsigned int index)
    const {
  if (!valid_ || index >= module_count_)
    return NULL;

  return &(*modules_)[index];
}


MinidumpModule* MinidumpModuleList::GetModuleForAddress(u_int64_t address) {
  if (!valid_)
    return NULL;

  unsigned int module_index;
  if (!range_map_.RetrieveRange(address, &module_index))
    return NULL;

  return GetModuleAtIndex(module_index);
}


void MinidumpModuleList::Print() {
  if (!valid_)
    return;

  printf("MinidumpModuleList\n");
  printf("  module_count = %d\n", module_count_);
  printf("\n");

  for (unsigned int module_index = 0;
       module_index < module_count_;
       ++module_index) {
    printf("module[%d]\n", module_index);

    (*modules_)[module_index].Print();
  }
}


//
// MinidumpMemoryList
//


MinidumpMemoryList::MinidumpMemoryList(Minidump* minidump)
    : MinidumpStream(minidump),
      range_map_(),
      descriptors_(NULL),
      regions_(NULL),
      region_count_(0) {
}


MinidumpMemoryList::~MinidumpMemoryList() {
  delete descriptors_;
  delete regions_;
}


bool MinidumpMemoryList::Read(u_int32_t expected_size) {
  // Invalidate cached data.
  delete descriptors_;
  descriptors_ = NULL;
  delete regions_;
  regions_ = NULL;
  range_map_.Clear();
  region_count_ = 0;

  valid_ = false;

  u_int32_t region_count;
  if (expected_size < sizeof(region_count))
    return false;
  if (!minidump_->ReadBytes(&region_count, sizeof(region_count)))
    return false;

  if (minidump_->swap())
    Swap(&region_count);

  if (expected_size != sizeof(region_count) +
                       region_count * sizeof(MDMemoryDescriptor)) {
    return false;
  }

  // TODO(mmentovai): verify rational size!
  auto_ptr<MemoryDescriptors> descriptors(new MemoryDescriptors(region_count));

  // Read the entire array in one fell swoop, instead of reading one entry
  // at a time in the loop.
  if (!minidump_->ReadBytes(&(*descriptors)[0],
                            sizeof(MDMemoryDescriptor) * region_count)) {
    return false;
  }

  auto_ptr<MemoryRegions> regions(
      new MemoryRegions(region_count, MinidumpMemoryRegion(minidump_)));

  for (unsigned int region_index = 0;
       region_index < region_count;
       ++region_index) {
    MDMemoryDescriptor* descriptor = &(*descriptors)[region_index];

    if (minidump_->swap())
      Swap(&*descriptor);

    u_int64_t base_address = descriptor->start_of_memory_range;
    u_int32_t region_size = descriptor->memory.data_size;

    // Check for base + size overflow or undersize.  A separate size==0
    // check is needed in case base == 0.
    u_int64_t high_address = base_address + region_size - 1;
    if (region_size == 0 || high_address < base_address)
      return false;

    if (!range_map_.StoreRange(base_address, region_size, region_index))
      return false;

    (*regions)[region_index].SetDescriptor(descriptor);
  }

  region_count_ = region_count;
  descriptors_ = descriptors.release();
  regions_ = regions.release();

  valid_ = true;
  return true;
}


MinidumpMemoryRegion* MinidumpMemoryList::GetMemoryRegionAtIndex(
      unsigned int index) {
  if (!valid_ || index >= region_count_)
    return NULL;

  return &(*regions_)[index];
}


MinidumpMemoryRegion* MinidumpMemoryList::GetMemoryRegionForAddress(
    u_int64_t address) {
  if (!valid_)
    return NULL;

  unsigned int region_index;
  if (!range_map_.RetrieveRange(address, &region_index))
    return NULL;

  return GetMemoryRegionAtIndex(region_index);
}


void MinidumpMemoryList::Print() {
  if (!valid_)
    return;

  printf("MinidumpMemoryList\n");
  printf("  region_count = %d\n", region_count_);
  printf("\n");

  for (unsigned int region_index = 0;
       region_index < region_count_;
       ++region_index) {
    MDMemoryDescriptor* descriptor = &(*descriptors_)[region_index];
    printf("region[%d]\n", region_index);
    printf("MDMemoryDescriptor\n");
    printf("  start_of_memory_range = 0x%llx\n",
           descriptor->start_of_memory_range);
    printf("  memory.data_size      = 0x%x\n", descriptor->memory.data_size);
    printf("  memory.rva            = 0x%x\n", descriptor->memory.rva);
    MinidumpMemoryRegion* region = GetMemoryRegionAtIndex(region_index);
    if (region) {
      printf("Memory\n");
      region->Print();
    } else {
      printf("No memory\n");
    }
    printf("\n");
  }
}


//
// MinidumpException
//


MinidumpException::MinidumpException(Minidump* minidump)
    : MinidumpStream(minidump),
      exception_(),
      context_(NULL) {
}


MinidumpException::~MinidumpException() {
  delete context_;
}


bool MinidumpException::Read(u_int32_t expected_size) {
  // Invalidate cached data.
  delete context_;
  context_ = NULL;

  valid_ = false;

  if (expected_size != sizeof(exception_))
    return false;

  if (!minidump_->ReadBytes(&exception_, sizeof(exception_)))
    return false;

  if (minidump_->swap()) {
    Swap(&exception_.thread_id);
    // exception_.__align is for alignment only and does not need to be
    // swapped.
    Swap(&exception_.exception_record.exception_code);
    Swap(&exception_.exception_record.exception_flags);
    Swap(&exception_.exception_record.exception_record);
    Swap(&exception_.exception_record.exception_address);
    Swap(&exception_.exception_record.number_parameters);
    // exception_.exception_record.__align is for alignment only and does not
    // need to be swapped.
    for (unsigned int parameter_index = 0;
         parameter_index < MD_EXCEPTION_MAXIMUM_PARAMETERS;
         ++parameter_index) {
      Swap(&exception_.exception_record.exception_information[parameter_index]);
    }
    Swap(&exception_.thread_context);
  }

  valid_ = true;
  return true;
}


u_int32_t MinidumpException::GetThreadID() {
  return valid_ ? exception_.thread_id : 0;
}


MinidumpContext* MinidumpException::GetContext() {
  if (!valid_)
    return NULL;

  if (!context_) {
    if (!minidump_->SeekSet(exception_.thread_context.rva))
      return NULL;

    auto_ptr<MinidumpContext> context(new MinidumpContext(minidump_));

    if (!context->Read(exception_.thread_context.data_size))
      return NULL;

    context_ = context.release();
  }

  return context_;
}


void MinidumpException::Print() {
  if (!valid_)
    return;

  printf("MDException\n");
  printf("  thread_id                                  = 0x%x\n",
         exception_.thread_id);
  printf("  exception_record.exception_code            = 0x%x\n",
         exception_.exception_record.exception_code);
  printf("  exception_record.exception_flags           = 0x%x\n",
         exception_.exception_record.exception_flags);
  printf("  exception_record.exception_record          = 0x%llx\n",
         exception_.exception_record.exception_record);
  printf("  exception_record.exception_address         = 0x%llx\n",
         exception_.exception_record.exception_address);
  printf("  exception_record.number_parameters         = %d\n",
         exception_.exception_record.number_parameters);
  for (unsigned int parameterIndex = 0;
       parameterIndex < exception_.exception_record.number_parameters;
       ++parameterIndex) {
    printf("  exception_record.exception_information[%2d] = 0x%llx\n",
           parameterIndex,
           exception_.exception_record.exception_information[parameterIndex]);
  }
  printf("  thread_context.data_size                   = %d\n",
         exception_.thread_context.data_size);
  printf("  thread_context.rva                         = 0x%x\n",
         exception_.thread_context.rva);
  MinidumpContext* context = GetContext();
  if (context) {
    printf("\n");
    context->Print();
  } else {
    printf("  (no context)\n");
    printf("\n");
  }
}


//
// MinidumpSystemInfo
//


MinidumpSystemInfo::MinidumpSystemInfo(Minidump* minidump)
    : MinidumpStream(minidump),
      system_info_(),
      csd_version_(NULL) {
}


MinidumpSystemInfo::~MinidumpSystemInfo() {
  delete csd_version_;
}


bool MinidumpSystemInfo::Read(u_int32_t expected_size) {
  // Invalidate cached data.
  delete csd_version_;
  csd_version_ = NULL;

  valid_ = false;

  if (expected_size != sizeof(system_info_))
    return false;

  if (!minidump_->ReadBytes(&system_info_, sizeof(system_info_)))
    return false;

  if (minidump_->swap()) {
    Swap(&system_info_.processor_architecture);
    Swap(&system_info_.processor_level);
    Swap(&system_info_.processor_revision);
    // number_of_processors and product_type are 8-bit quantities and need no
    // swapping.
    Swap(&system_info_.major_version);
    Swap(&system_info_.minor_version);
    Swap(&system_info_.build_number);
    Swap(&system_info_.platform_id);
    Swap(&system_info_.csd_version_rva);
    Swap(&system_info_.suite_mask);
    Swap(&system_info_.reserved2);
    // TODO(mmentovai): This obviously only supports x86 for the time being.
    for (unsigned int i = 0; i < 3; ++i)
      Swap(&system_info_.cpu.x86_cpu_info.vendor_id[i]);
    Swap(&system_info_.cpu.x86_cpu_info.version_information);
    Swap(&system_info_.cpu.x86_cpu_info.feature_information);
    Swap(&system_info_.cpu.x86_cpu_info.amd_extended_cpu_features);
  }

  valid_ = true;
  return true;
}


const string* MinidumpSystemInfo::GetCSDVersion() {
  if (!valid_)
    return NULL;

  if (!csd_version_)
    csd_version_ = minidump_->ReadString(system_info_.csd_version_rva);

  return csd_version_;
}


void MinidumpSystemInfo::Print() {
  if (!valid_)
    return;

  printf("MDRawSystemInfo\n");
  printf("  processor_architecture                     = %d\n",
         system_info_.processor_architecture);
  printf("  processor_level                            = %d\n",
         system_info_.processor_level);
  printf("  number_of_processors                       = %d\n",
         system_info_.number_of_processors);
  printf("  product_type                               = %d\n",
         system_info_.product_type);
  printf("  major_version                              = %d\n",
         system_info_.major_version);
  printf("  minor_version                              = %d\n",
         system_info_.minor_version);
  printf("  build_number                               = %d\n",
         system_info_.build_number);
  printf("  platform_id                                = %d\n",
         system_info_.platform_id);
  printf("  csd_version_rva                            = 0x%x\n",
         system_info_.csd_version_rva);
  printf("  suite_mask                                 = 0x%x\n",
         system_info_.suite_mask);
  printf("  reserved2                                  = 0x%x\n",
         system_info_.reserved2);
  for (unsigned int i = 0; i < 3; ++i) {
    printf("  cpu.x86_cpu_info.vendor_id[%d]              = 0x%x\n",
           i, system_info_.cpu.x86_cpu_info.vendor_id[i]);
  }
  printf("  cpu.x86_cpu_info.version_information       = 0x%x\n",
         system_info_.cpu.x86_cpu_info.version_information);
  printf("  cpu.x86_cpu_info.feature_information       = 0x%x\n",
         system_info_.cpu.x86_cpu_info.feature_information);
  printf("  cpu.x86_cpu_info.amd_extended_cpu_features = 0x%x\n",
         system_info_.cpu.x86_cpu_info.amd_extended_cpu_features);
  const char* csd_version = GetCSDVersion()->c_str();
  if (csd_version)
    printf("  (csd_version)                              = \"%s\"\n",
           csd_version);
  else
    printf("  (csd_version)                              = (null)\n");
  printf("\n");
}


//
// MinidumpMiscInfo
//


MinidumpMiscInfo::MinidumpMiscInfo(Minidump* minidump)
    : MinidumpStream(minidump),
      misc_info_() {
}


bool MinidumpMiscInfo::Read(u_int32_t expected_size) {
  valid_ = false;

  if (expected_size != MD_MISCINFO_SIZE &&
      expected_size != MD_MISCINFO2_SIZE) {
    return false;
  }

  if (!minidump_->ReadBytes(&misc_info_, expected_size))
    return false;

  if (minidump_->swap()) {
    Swap(&misc_info_.size_of_info);
    Swap(&misc_info_.flags1);
    Swap(&misc_info_.process_id);
    Swap(&misc_info_.process_create_time);
    Swap(&misc_info_.process_user_time);
    Swap(&misc_info_.process_kernel_time);
    if (misc_info_.size_of_info > MD_MISCINFO_SIZE) {
      Swap(&misc_info_.processor_max_mhz);
      Swap(&misc_info_.processor_current_mhz);
      Swap(&misc_info_.processor_mhz_limit);
      Swap(&misc_info_.processor_max_idle_state);
      Swap(&misc_info_.processor_current_idle_state);
    }
  }

  if (misc_info_.size_of_info != expected_size)
    return false;

  valid_ = true;
  return true;
}


void MinidumpMiscInfo::Print() {
  if (!valid_)
    return;

  printf("MDRawMiscInfo\n");
  printf("  size_of_info                 = %d\n",   misc_info_.size_of_info);
  printf("  flags1                       = 0x%x\n", misc_info_.flags1);
  printf("  process_id                   = 0x%x\n", misc_info_.process_id);
  printf("  process_create_time          = 0x%x\n",
         misc_info_.process_create_time);
  printf("  process_user_time            = 0x%x\n",
         misc_info_.process_user_time);
  printf("  process_kernel_time          = 0x%x\n",
         misc_info_.process_kernel_time);
  if (misc_info_.size_of_info > MD_MISCINFO_SIZE) {
    printf("  processor_max_mhz            = %d\n",
           misc_info_.processor_max_mhz);
    printf("  processor_current_mhz        = %d\n",
           misc_info_.processor_current_mhz);
    printf("  processor_mhz_limit          = %d\n",
           misc_info_.processor_mhz_limit);
    printf("  processor_max_idle_state     = 0x%x\n",
           misc_info_.processor_max_idle_state);
    printf("  processor_current_idle_state = 0x%x\n",
           misc_info_.processor_current_idle_state);
  }
}


//
// Minidump
//


Minidump::Minidump(int fd)
    : header_(),
      directory_(NULL),
      stream_map_(NULL),
      fd_(fd),
      swap_(false),
      valid_(false) {
}


Minidump::~Minidump() {
  delete directory_;
  delete stream_map_;
}


bool Minidump::Read() {
  // Invalidate cached data.
  delete directory_;
  directory_ = NULL;
  delete stream_map_;
  stream_map_ = NULL;

  valid_ = false;

  if (!ReadBytes(&header_, sizeof(MDRawHeader)))
    return false;

  if (header_.signature != MD_HEADER_SIGNATURE) {
    // The file may be byte-swapped.  Under the present architecture, these
    // classes don't know or need to know what CPU (or endianness) the
    // minidump was produced on in order to parse it.  Use the signature as
    // a byte order marker.
    u_int32_t signature_swapped = header_.signature;
    Swap(&signature_swapped);
    if (signature_swapped != MD_HEADER_SIGNATURE) {
      // This isn't a minidump or a byte-swapped minidump.
      return false;
    }
    swap_ = true;
  } else {
    // The file is not byte-swapped.  Set swap_ false (it may have been true
    // if the object is being reused?)
    swap_ = false;
  }

  if (swap_) {
    Swap(&header_.signature);
    Swap(&header_.version);
    Swap(&header_.stream_count);
    Swap(&header_.stream_directory_rva);
    Swap(&header_.checksum);
    Swap(&header_.time_date_stamp);
    Swap(&header_.flags);
  }

  // Version check.  The high 16 bits of header_.version contain something
  // else "implementation specific."
  if ((header_.version & 0x0000ffff) != MD_HEADER_VERSION) {
    return false;
  }

  if (!SeekSet(header_.stream_directory_rva))
    return false;

  // TODO(mmentovai): verify rational size!
  auto_ptr<MinidumpDirectoryEntries> directory(
      new MinidumpDirectoryEntries(header_.stream_count));

  // Read the entire array in one fell swoop, instead of reading one entry
  // at a time in the loop.
  if (!ReadBytes(&(*directory)[0],
                 sizeof(MDRawDirectory) * header_.stream_count))
    return false;

  auto_ptr<MinidumpStreamMap> stream_map(new MinidumpStreamMap());

  for (unsigned int stream_index = 0;
       stream_index < header_.stream_count;
       ++stream_index) {
    MDRawDirectory* directory_entry = &(*directory)[stream_index];

    if (swap_) {
      Swap(&directory_entry->stream_type);
      Swap(&directory_entry->location);
    }

    // Initialize the stream_map map, which speeds locating a stream by
    // type.
    unsigned int stream_type = directory_entry->stream_type;
    switch(stream_type) {
      case THREAD_LIST_STREAM:
      case MODULE_LIST_STREAM:
      case MEMORY_LIST_STREAM:
      case EXCEPTION_STREAM:
      case SYSTEM_INFO_STREAM:
      case MISC_INFO_STREAM: {
        if (stream_map->find(stream_type) != stream_map->end()) {
          // Another stream with this type was already found.  A minidump
          // file should contain at most one of each of these stream types.
          return false;
        }
        // Fall through to default
      }

      default: {
        // Overwrites for stream types other than those above, but it's
        // expected to be the user's burden in that case.
        (*stream_map)[stream_type].stream_index = stream_index;
      }
    }
  }

  directory_ = directory.release();
  stream_map_ = stream_map.release();

  valid_ = true;
  return true;
}


MinidumpThreadList* Minidump::GetThreadList() {
  MinidumpThreadList* thread_list;
  return GetStream(&thread_list);
}


MinidumpModuleList* Minidump::GetModuleList() {
  MinidumpModuleList* module_list;
  return GetStream(&module_list);
}


MinidumpMemoryList* Minidump::GetMemoryList() {
  MinidumpMemoryList* memory_list;
  return GetStream(&memory_list);
}


MinidumpException* Minidump::GetException() {
  MinidumpException* exception;
  return GetStream(&exception);
}


MinidumpSystemInfo* Minidump::GetSystemInfo() {
  MinidumpSystemInfo* system_info;
  return GetStream(&system_info);
}


MinidumpMiscInfo* Minidump::GetMiscInfo() {
  MinidumpMiscInfo* misc_info;
  return GetStream(&misc_info);
}


void Minidump::Print() {
  if (!valid_)
    return;

  printf("MDRawHeader\n");
  printf("  signature            = 0x%x\n",    header_.signature);
  printf("  version              = 0x%x\n",    header_.version);
  printf("  stream_count         = %d\n",      header_.stream_count);
  printf("  stream_directory_rva = 0x%x\n",    header_.stream_directory_rva);
  printf("  checksum             = 0x%x\n",    header_.checksum);
  struct tm* timestruct = gmtime((time_t*)&header_.time_date_stamp);
  char timestr[20];
  strftime(timestr, 20, "%Y-%m-%d %H:%M:%S", timestruct);
  printf("  time_date_stamp      = 0x%x %s\n", header_.time_date_stamp,
                                               timestr);
  printf("  flags                = 0x%llx\n",  header_.flags);
  printf("\n");

  for (unsigned int stream_index = 0;
       stream_index < header_.stream_count;
       ++stream_index) {
    MDRawDirectory* directory_entry = &(*directory_)[stream_index];

    printf("mDirectory[%d]\n", stream_index);
    printf("MDRawDirectory\n");
    printf("  stream_type        = %d\n",   directory_entry->stream_type);
    printf("  location.data_size = %d\n",
           directory_entry->location.data_size);
    printf("  location.rva       = 0x%x\n", directory_entry->location.rva);
    printf("\n");
  }

  printf("Streams:\n");
  for (MinidumpStreamMap::const_iterator iterator = stream_map_->begin();
       iterator != stream_map_->end();
       ++iterator) {
    u_int32_t stream_type = iterator->first;
    MinidumpStreamInfo info = iterator->second;
    printf("  stream type %2d at index %d\n", stream_type, info.stream_index);
  }
  printf("\n");
}


const MDRawDirectory* Minidump::GetDirectoryEntryAtIndex(unsigned int index)
      const {
  if (!valid_ || index >= header_.stream_count)
    return NULL;

  return &(*directory_)[index];
}


bool Minidump::ReadBytes(void* bytes, size_t count) {
  // Can't check valid_ because Read needs to call this method before
  // validity can be determined.  The only member that this method
  // depends on is mFD, and an unset or invalid fd may generate an
  // error but should not cause a crash.
  ssize_t bytes_read = read(fd_, bytes, count);
  if (static_cast<size_t>(bytes_read) != count)
    return false;
  return true;
}


bool Minidump::SeekSet(off_t offset) {
  // Can't check valid_ because Read needs to call this method before
  // validity can be determined.  The only member that this method
  // depends on is mFD, and an unset or invalid fd may generate an
  // error but should not cause a crash.
  off_t sought = lseek(fd_, offset, SEEK_SET);
  if (sought != offset)
    return false;
  return true;
}


string* Minidump::ReadString(off_t offset) {
  if (!valid_)
    return NULL;
  if (!SeekSet(offset))
    return NULL;

  u_int32_t bytes;
  if (!ReadBytes(&bytes, sizeof(bytes)))
    return NULL;
  if (swap_)
    Swap(&bytes);

  if (bytes % 2 != 0)
    return NULL;
  unsigned int utf16_words = bytes / 2;

  // TODO(mmentovai): verify rational size!
  vector<u_int16_t> string_utf16(utf16_words);

  if (!ReadBytes(&string_utf16[0], bytes))
    return NULL;

  return UTF16ToUTF8(string_utf16, swap_);
}


bool Minidump::SeekToStreamType(u_int32_t  stream_type,
                                u_int32_t* stream_length) {
  if (!valid_ || !stream_length)
    return false;

  MinidumpStreamMap::const_iterator iterator = stream_map_->find(stream_type);
  if (iterator == stream_map_->end()) {
    // This stream type didn't exist in the directory.
    return false;
  }

  MinidumpStreamInfo info = iterator->second;
  if (info.stream_index >= header_.stream_count)
    return false;

  MDRawDirectory* directory_entry = &(*directory_)[info.stream_index];
  if (!SeekSet(directory_entry->location.rva))
    return false;

  *stream_length = directory_entry->location.data_size;

  return true;
}


template<typename T>
T* Minidump::GetStream(T** stream) {
  // stream is a garbage parameter that's present only to account for C++'s
  // inability to overload a method based solely on its return type.

  if (!stream)
    return NULL;
  *stream = NULL;

  if (!valid_)
    return NULL;

  u_int32_t stream_type = T::kStreamType;
  MinidumpStreamMap::iterator iterator = stream_map_->find(stream_type);
  if (iterator == stream_map_->end()) {
    // This stream type didn't exist in the directory.
    return NULL;
  }

  // Get a pointer so that the stored stream field can be altered.
  MinidumpStreamInfo* info = &iterator->second;

  if (info->stream) {
    // This cast is safe because info.stream is only populated by this
    // method, and there is a direct correlation between T and stream_type.
    *stream = static_cast<T*>(info->stream);
    return *stream;
  }

  u_int32_t stream_length;
  if (!SeekToStreamType(stream_type, &stream_length))
    return NULL;

  auto_ptr<T> new_stream(new T(this));

  if (!new_stream->Read(stream_length))
    return NULL;

  *stream = new_stream.release();
  info->stream = *stream;
  return *stream;
}


} // namespace google_airbag
