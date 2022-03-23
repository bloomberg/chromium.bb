// Copyright 2014 PDFium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Original code copyright 2014 Foxit Software Inc. http://www.foxitsoftware.com
// Original code is licensed as follows:
/*
 * Copyright 2007 ZXing authors
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "fxbarcode/common/BC_CommonBitMatrix.h"

#include "core/fxcrt/stl_util.h"
#include "third_party/base/check_op.h"

CBC_CommonBitMatrix::CBC_CommonBitMatrix(size_t width, size_t height)
    : m_height(height), m_rowSize((width + 31) >> 5) {
  static constexpr int32_t kMaxBits = 1024 * 1024 * 1024;  // 1 Gb.
  CHECK_LT(width, kMaxBits / height);

  m_bits = fxcrt::Vector2D<uint32_t>(m_rowSize, m_height);
}

CBC_CommonBitMatrix::~CBC_CommonBitMatrix() = default;

bool CBC_CommonBitMatrix::Get(size_t x, size_t y) const {
  size_t offset = y * m_rowSize + (x >> 5);
  CHECK_LT(offset, m_rowSize * m_height);
  return ((m_bits[offset] >> (x & 0x1f)) & 1) != 0;
}

void CBC_CommonBitMatrix::Set(size_t x, size_t y) {
  size_t offset = y * m_rowSize + (x >> 5);
  CHECK_LT(offset, m_rowSize * m_height);
  m_bits[offset] |= 1u << (x & 0x1f);
}
