/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/style/FilterOperations.h"

#include <numeric>

namespace blink {

FilterOperations::FilterOperations() = default;

void FilterOperations::Trace(blink::Visitor* visitor) {
  visitor->Trace(operations_);
}

FilterOperations& FilterOperations::operator=(const FilterOperations& other) =
    default;

bool FilterOperations::operator==(const FilterOperations& o) const {
  if (operations_.size() != o.operations_.size())
    return false;

  unsigned s = operations_.size();
  for (unsigned i = 0; i < s; i++) {
    if (*operations_[i] != *o.operations_[i])
      return false;
  }

  return true;
}

bool FilterOperations::CanInterpolateWith(const FilterOperations& other) const {
  for (size_t i = 0; i < Operations().size(); ++i) {
    if (!FilterOperation::CanInterpolate(Operations()[i]->GetType()))
      return false;
  }

  for (size_t i = 0; i < other.Operations().size(); ++i) {
    if (!FilterOperation::CanInterpolate(other.Operations()[i]->GetType()))
      return false;
  }

  size_t common_size = std::min(Operations().size(), other.Operations().size());
  for (size_t i = 0; i < common_size; ++i) {
    if (!Operations()[i]->IsSameType(*other.Operations()[i]))
      return false;
  }
  return true;
}

bool FilterOperations::HasReferenceFilter() const {
  for (const auto& operation : operations_) {
    if (operation->GetType() == FilterOperation::REFERENCE ||
        operation->GetType() == FilterOperation::BOX_REFLECT)
      return true;
  }
  return false;
}

FloatRect FilterOperations::MapRect(const FloatRect& rect) const {
  auto accumulate_mapped_rect = [](const FloatRect& rect,
                                   const Member<FilterOperation>& op) {
    return op->MapRect(rect);
  };
  return std::accumulate(operations_.begin(), operations_.end(), rect,
                         accumulate_mapped_rect);
}

bool FilterOperations::HasFilterThatAffectsOpacity() const {
  for (size_t i = 0; i < operations_.size(); ++i) {
    if (operations_[i]->AffectsOpacity())
      return true;
  }
  return false;
}

bool FilterOperations::HasFilterThatMovesPixels() const {
  for (size_t i = 0; i < operations_.size(); ++i) {
    if (operations_[i]->MovesPixels())
      return true;
  }
  return false;
}

void FilterOperations::AddClient(
    SVGResourceClient* client,
    base::SingleThreadTaskRunner* task_runner) const {
  for (FilterOperation* operation : operations_) {
    if (operation->GetType() == FilterOperation::REFERENCE)
      ToReferenceFilterOperation(*operation).AddClient(client, task_runner);
  }
}

void FilterOperations::RemoveClient(SVGResourceClient* client) const {
  for (FilterOperation* operation : operations_) {
    if (operation->GetType() == FilterOperation::REFERENCE)
      ToReferenceFilterOperation(*operation).RemoveClient(client);
  }
}

}  // namespace blink
