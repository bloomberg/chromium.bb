// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/fake_file_access.h"

#include <algorithm>
#include <set>
#include <utility>

#include "core/fxcrt/fx_system.h"

namespace {

class FileAccessWrapper final : public FPDF_FILEACCESS {
 public:
  explicit FileAccessWrapper(FakeFileAccess* simulator)
      : simulator_(simulator) {
    m_FileLen = simulator_->GetFileSize();
    m_GetBlock = &GetBlockImpl;
    m_Param = this;
  }

  static int GetBlockImpl(void* param,
                          unsigned long position,
                          unsigned char* pBuf,
                          unsigned long size) {
    return static_cast<FileAccessWrapper*>(param)->simulator_->GetBlock(
        position, pBuf, size);
  }

 private:
  UnownedPtr<FakeFileAccess> simulator_;
};

class FileAvailImpl final : public FX_FILEAVAIL {
 public:
  explicit FileAvailImpl(FakeFileAccess* simulator) : simulator_(simulator) {
    version = 1;
    IsDataAvail = &IsDataAvailImpl;
  }

  static FPDF_BOOL IsDataAvailImpl(FX_FILEAVAIL* pThis,
                                   size_t offset,
                                   size_t size) {
    return static_cast<FileAvailImpl*>(pThis)->simulator_->IsDataAvail(offset,
                                                                       size);
  }

 private:
  UnownedPtr<FakeFileAccess> simulator_;
};

class DownloadHintsImpl final : public FX_DOWNLOADHINTS {
 public:
  explicit DownloadHintsImpl(FakeFileAccess* simulator)
      : simulator_(simulator) {
    version = 1;
    AddSegment = &AddSegmentImpl;
  }

  static void AddSegmentImpl(FX_DOWNLOADHINTS* pThis,
                             size_t offset,
                             size_t size) {
    return static_cast<DownloadHintsImpl*>(pThis)->simulator_->AddSegment(
        offset, size);
  }

 private:
  UnownedPtr<FakeFileAccess> simulator_;
};

}  // namespace

FakeFileAccess::FakeFileAccess(FPDF_FILEACCESS* file_access)
    : file_access_(file_access),
      file_access_wrapper_(std::make_unique<FileAccessWrapper>(this)),
      file_avail_(std::make_unique<FileAvailImpl>(this)),
      download_hints_(std::make_unique<DownloadHintsImpl>(this)) {
  ASSERT(file_access_);
}

FakeFileAccess::~FakeFileAccess() = default;

FPDF_BOOL FakeFileAccess::IsDataAvail(size_t offset, size_t size) const {
  return available_data_.Contains(RangeSet::Range(offset, offset + size));
}

void FakeFileAccess::AddSegment(size_t offset, size_t size) {
  requested_data_.Union(RangeSet::Range(offset, offset + size));
}

unsigned long FakeFileAccess::GetFileSize() {
  return file_access_->m_FileLen;
}

int FakeFileAccess::GetBlock(unsigned long position,
                             unsigned char* pBuf,
                             unsigned long size) {
  if (!pBuf || !size)
    return false;

  if (!IsDataAvail(static_cast<size_t>(position), static_cast<size_t>(size)))
    return false;

  return file_access_->m_GetBlock(file_access_->m_Param, position, pBuf, size);
}

void FakeFileAccess::SetRequestedDataAvailable() {
  available_data_.Union(requested_data_);
  requested_data_.Clear();
}

void FakeFileAccess::SetWholeFileAvailable() {
  available_data_.Union(RangeSet::Range(0, static_cast<size_t>(GetFileSize())));
  requested_data_.Clear();
}
