// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_MEMORY_RAW_PTR_ASAN_SERVICE_H_
#define BASE_MEMORY_RAW_PTR_ASAN_SERVICE_H_

#include "base/allocator/buildflags.h"

#if BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
#include <cstddef>
#include <cstdint>

#include "base/base_export.h"
#include "base/types/strong_alias.h"

namespace base {

using EnableDereferenceCheck =
    base::StrongAlias<class EnableDereferenceCheckTag, bool>;
using EnableExtractionCheck =
    base::StrongAlias<class EnableExtractionCheckTag, bool>;
using EnableInstantiationCheck =
    base::StrongAlias<class EnableInstantiationCheckTag, bool>;

class BASE_EXPORT RawPtrAsanService {
 public:
  enum class Mode {
    kUninitialized,
    kDisabled,
    kEnabled,
  };

  enum class ReportType {
    kDereference,
    kExtraction,
    kInstantiation,
  };

  void Configure(EnableDereferenceCheck,
                 EnableExtractionCheck,
                 EnableInstantiationCheck);
  Mode mode() const { return mode_; }

  bool IsSupportedAllocation(void*) const;

  bool is_dereference_check_enabled() const {
    return is_dereference_check_enabled_;
  }
  bool is_extraction_check_enabled() const {
    return is_extraction_check_enabled_;
  }
  bool is_instantiation_check_enabled() const {
    return is_instantiation_check_enabled_;
  }

  static RawPtrAsanService& GetInstance() { return instance_; }

  static void SetPendingReport(ReportType type, const volatile void* ptr);
  static void Log(const char* format, ...);

 private:
  struct PendingReport {
    ReportType type;
    uintptr_t allocation_base;
    size_t allocation_size;
  };

  static PendingReport& GetPendingReport() {
    static thread_local PendingReport report;
    return report;
  }

  uint8_t* GetShadow(void* ptr) const;

  static void MallocHook(const volatile void*, size_t);
  static void FreeHook(const volatile void*) {}
  static void ErrorReportCallback(const char* report);

  Mode mode_ = Mode::kUninitialized;
  bool is_dereference_check_enabled_ = false;
  bool is_extraction_check_enabled_ = false;
  bool is_instantiation_check_enabled_ = false;

  size_t shadow_offset_ = 0;

  static RawPtrAsanService instance_;  // Not a static local variable because
                                       // `GetInstance()` is used in hot paths.
};

}  // namespace base

#endif  // BUILDFLAG(USE_ASAN_BACKUP_REF_PTR)
#endif  // BASE_MEMORY_RAW_PTR_ASAN_SERVICE_H_
