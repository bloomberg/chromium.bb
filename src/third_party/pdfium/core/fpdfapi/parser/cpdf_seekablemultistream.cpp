// Copyright 2017 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com

#include "core/fpdfapi/parser/cpdf_seekablemultistream.h"

#include <algorithm>

#include "core/fpdfapi/parser/cpdf_stream_acc.h"
#include "third_party/base/logging.h"
#include "third_party/base/stl_util.h"

CPDF_SeekableMultiStream::CPDF_SeekableMultiStream(
    const std::vector<const CPDF_Stream*>& streams) {
  for (const CPDF_Stream* pStream : streams) {
    m_Data.push_back(pdfium::MakeRetain<CPDF_StreamAcc>(pStream));
    m_Data.back()->LoadAllDataFiltered();
  }
}

CPDF_SeekableMultiStream::~CPDF_SeekableMultiStream() {}

FX_FILESIZE CPDF_SeekableMultiStream::GetSize() {
  uint32_t dwSize = 0;
  for (const auto& acc : m_Data)
    dwSize += acc->GetSize();
  return dwSize;
}

bool CPDF_SeekableMultiStream::ReadBlockAtOffset(void* buffer,
                                                 FX_FILESIZE offset,
                                                 size_t size) {
  int32_t iCount = pdfium::CollectionSize<int32_t>(m_Data);
  int32_t index = 0;
  while (index < iCount) {
    const auto& acc = m_Data[index];
    FX_FILESIZE dwSize = acc->GetSize();
    if (offset < dwSize)
      break;

    offset -= dwSize;
    index++;
  }
  while (index < iCount) {
    const auto& acc = m_Data[index];
    uint32_t dwSize = acc->GetSize();
    size_t dwRead = std::min(size, static_cast<size_t>(dwSize - offset));
    memcpy(buffer, acc->GetSpan().subspan(offset, dwRead).data(), dwRead);
    size -= dwRead;
    if (size == 0)
      return true;

    buffer = static_cast<uint8_t*>(buffer) + dwRead;
    offset = 0;
    index++;
  }
  return false;
}

size_t CPDF_SeekableMultiStream::ReadBlock(void* buffer, size_t size) {
  NOTREACHED();
  return 0;
}

FX_FILESIZE CPDF_SeekableMultiStream::GetPosition() {
  return 0;
}

bool CPDF_SeekableMultiStream::IsEOF() {
  return false;
}

bool CPDF_SeekableMultiStream::Flush() {
  NOTREACHED();
  return false;
}

bool CPDF_SeekableMultiStream::WriteBlockAtOffset(const void* pData,
                                                  FX_FILESIZE offset,
                                                  size_t size) {
  NOTREACHED();
  return false;
}
