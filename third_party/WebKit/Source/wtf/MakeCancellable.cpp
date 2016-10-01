// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "wtf/MakeCancellable.h"

namespace WTF {

namespace internal {

FunctionCanceller::FunctionCanceller() = default;
FunctionCanceller::~FunctionCanceller() = default;

}  // namespace internal

ScopedFunctionCanceller::ScopedFunctionCanceller() = default;
ScopedFunctionCanceller::ScopedFunctionCanceller(
    PassRefPtr<internal::FunctionCanceller> canceller)
    : m_canceller(std::move(canceller)) {}

ScopedFunctionCanceller::ScopedFunctionCanceller(ScopedFunctionCanceller&&) =
    default;
ScopedFunctionCanceller& ScopedFunctionCanceller::operator=(
    ScopedFunctionCanceller&& other) {
  RefPtr<internal::FunctionCanceller> canceller = std::move(other.m_canceller);
  cancel();
  m_canceller = std::move(canceller);
  return *this;
}

ScopedFunctionCanceller::~ScopedFunctionCanceller() {
  cancel();
}

void ScopedFunctionCanceller::detach() {
  m_canceller = nullptr;
}

void ScopedFunctionCanceller::cancel() {
  if (!m_canceller)
    return;

  m_canceller->cancel();
  m_canceller = nullptr;
}

bool ScopedFunctionCanceller::isActive() const {
  return m_canceller && m_canceller->isActive();
}

}  // namespace WTF
