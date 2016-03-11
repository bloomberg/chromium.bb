// Copyright 2016 The Native Client Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "native_client/src/untrusted/pll_loader/pll_loader.h"

#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include "native_client/src/shared/platform/nacl_log.h"
#include "native_client/src/untrusted/pnacl_dynloader/dynloader.h"

namespace {

// This is a simple implementation that does not support multiple threads.
// It demonstrates how we can optimize if we know that pthread_create()
// will never be called.
void *TLSBlockGetter(PLLTLSBlockGetter *closure) {
  return closure->arg;
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

    // Initialize TLS.
    if (PLLTLSBlockGetter *tls_block_getter = module.root()->tls_block_getter) {
      tls_block_getter->func = TLSBlockGetter;
      tls_block_getter->arg = module.InstantiateTLSBlock();
    }
  }
}
