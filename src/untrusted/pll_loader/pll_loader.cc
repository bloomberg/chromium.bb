// Copyright 2016 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/untrusted/pll_loader/pll_loader.h"

#include <pthread.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include <algorithm>
#include <vector>

#include "native_client/src/shared/platform/nacl_check.h"
#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/untrusted/pnacl_dynloader/dynloader.h"

namespace {

// Array of modules, used for instantiating their TLS blocks.  This is used
// concurrently and is protected by g_modules_mutex.
std::vector<const PLLRoot *> *g_modules;

pthread_mutex_t g_modules_mutex = PTHREAD_MUTEX_INITIALIZER;

// Array of modules' TLS blocks for the current thread.  An entry may be
// NULL if no TLS block has been instantiated for the module yet on this
// thread.
__thread void **thread_modules_array = NULL;
__thread uint32_t thread_modules_array_size = 0;

void *TLSBlockGetter(PLLTLSBlockGetter *closure) {
  uint32_t module_index = (uint32_t) closure->arg;

  // Fast path: Handles the case when the TLS block has been instantiated
  // already for the module.
  size_t array_size = thread_modules_array_size;
  if (module_index < array_size) {
    void *tls_block = thread_modules_array[module_index];
    if (tls_block != NULL)
      return tls_block;
  }

  // Slow path: This allocates the TLS block for the module on demand, and
  // it may need to grow the thread's module array too.
  CHECK(pthread_mutex_lock(&g_modules_mutex) == 0);
  size_t new_size = g_modules->size();
  const PLLRoot *pll_root = (*g_modules)[module_index];
  CHECK(pthread_mutex_unlock(&g_modules_mutex) == 0);

  // Use of the memory allocator here assumes that the allocator --
  // i.e. malloc()/realloc()/posix_memalign() -- does not depend on the TLS
  // facility we are providing here.  This is true when the allocator is
  // linked into the PLL loader.  If this assumption were broken, we could
  // get re-entrancy problems such as deadlocks.  This assumption could get
  // broken if the allocator were implemented by a dynamically-loaded libc
  // (e.g. by having libc override the PLL loader's use of malloc()).

  if (module_index >= array_size) {
    // Grow the array.  We grow it to the number of currently loaded
    // modules (rather than to module_index + 1) so that we won't need to
    // grow it again unless further modules are loaded.
    void **new_array = (void **) realloc(thread_modules_array,
                                         new_size * sizeof(void *));
    CHECK(new_array != NULL);
    memset(&new_array[array_size], 0,
           (new_size - array_size) * sizeof(void *));
    thread_modules_array = new_array;
    thread_modules_array_size = new_size;
  }

  void *tls_block = PLLModule(pll_root).InstantiateTLSBlock();
  thread_modules_array[module_index] = tls_block;
  return tls_block;
}

}  // namespace

uint32_t PLLModule::HashString(const char *sp) {
  uint32_t h = 5381;
  for (unsigned char c = *sp; c != '\0'; c = *++sp)
    h = h * 33 + c;
  return h;
}

bool PLLModule::IsMaybeExported(uint32_t hash1) {
  const int kWordSizeBits = 32;
  uint32_t hash2 = hash1 >> root_->bloom_filter_shift2;

  uint32_t word_num = (hash1 / kWordSizeBits) &
      root_->bloom_filter_maskwords_bitmask;
  uint32_t bitmask =
      (1 << (hash1 % kWordSizeBits)) | (1 << (hash2 % kWordSizeBits));
  return (root_->bloom_filter_data[word_num] & bitmask) == bitmask;
}

void *PLLModule::GetExportedSym(const char *name) {
  uint32_t hash = HashString(name);

  // Use the bloom filter to quickly reject symbols that aren't exported.
  if (!IsMaybeExported(hash))
    return NULL;

  uint32_t bucket_index = hash % root_->bucket_count;
  int32_t chain_index = root_->hash_buckets[bucket_index];
  // Bucket empty -- value not found.
  if (chain_index == -1)
    return NULL;

  for (; chain_index < root_->export_count; chain_index++) {
    uint32_t chain_value = root_->hash_chains[chain_index];
    if ((hash & ~1) == (chain_value & ~1) &&
        strcmp(name, GetExportedSymbolName(chain_index)) == 0)
      return root_->exported_ptrs[chain_index];

    // End of chain -- value not found.
    if ((chain_value & 1) == 1)
      return NULL;
  }

  NaClLog(LOG_FATAL, "GetExportedSym: "
          "Bad hash table in PLL: chain not terminated\n");
  return NULL;
}

void *PLLModule::InstantiateTLSBlock() {
  // posix_memalign() requires its alignment arg to be at least sizeof(void *).
  size_t alignment = std::max(root_->tls_template_alignment, sizeof(void *));
  void *base;
  if (posix_memalign(&base, alignment, root_->tls_template_total_size) != 0) {
    NaClLog(LOG_FATAL, "InstantiateTLSBlock: Allocation failed\n");
  }
  memcpy(base, root_->tls_template, root_->tls_template_data_size);
  size_t bss_size = (root_->tls_template_total_size -
                     root_->tls_template_data_size);
  memset((char *) base + root_->tls_template_data_size, 0, bss_size);
  return base;
}

void PLLModule::InitializeTLS() {
  if (PLLTLSBlockGetter *tls_block_getter = root_->tls_block_getter) {
    CHECK(pthread_mutex_lock(&g_modules_mutex) == 0);
    if (g_modules == NULL)
      g_modules = new std::vector<const PLLRoot *>();
    uint32_t module_index = g_modules->size();
    g_modules->push_back(root_);
    CHECK(pthread_mutex_unlock(&g_modules_mutex) == 0);

    tls_block_getter->func = TLSBlockGetter;
    tls_block_getter->arg = (void *) module_index;
  }
}

void ModuleSet::SetSonameSearchPath(const std::vector<std::string> &dir_list) {
  search_path_ = dir_list;
}

void ModuleSet::AddBySoname(const char *soname) {
  // TODO(smklein): Deduplicate rather than failing once dependencies are added.
  if (sonames_.count(soname) != 0) {
    NaClLog(LOG_FATAL, "PLL Loader found duplicate soname: %s\n", soname);
  }
  sonames_.insert(soname);

  // Actually load the module implied by the soname.
  for (auto path : search_path_) {
    // Appending "/" might be unnecessary, but "foo/bar" and "foo//bar" should
    // point to the same file.
    path.append("/");
    path.append(soname);
    struct stat buf;
    if (stat(path.c_str(), &buf) == 0) {
      AddByFilename(path.c_str());
      return;
    }
  }

  NaClLog(LOG_FATAL, "PLL Loader cannot find shared object file: %s\n", soname);
}

void ModuleSet::AddByFilename(const char *filename) {
  void *pso_root;
  int err = pnacl_load_elf_file(filename, &pso_root);
  if (err != 0) {
    NaClLog(LOG_FATAL, "pnacl_load_elf_file() failed: errno=%d\n", err);
  }
  modules_.push_back(PLLModule((const PLLRoot *) pso_root));
}

void *ModuleSet::GetSym(const char *name) {
  for (auto &module : modules_) {
    if (void *sym = module.GetExportedSym(name))
      return sym;
  }
  return NULL;
}

void ModuleSet::ResolveRefs() {
  for (auto &module : modules_) {
    for (size_t index = 0, count = module.root()->import_count;
         index < count; ++index) {
      const char *sym_name = module.GetImportedSymbolName(index);
      uintptr_t sym_value = (uintptr_t) GetSym(sym_name);
      if (sym_value == 0) {
        NaClLog(LOG_FATAL, "Undefined symbol: \"%s\"\n", sym_name);
      }
      *(uintptr_t *) module.root()->imported_ptrs[index] += sym_value;
    }

    module.InitializeTLS();
  }
}
