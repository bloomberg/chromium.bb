// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/third_party/quiche/src/quic/core/qpack/value_splitting_header_list.h"

namespace quic {

ValueSplittingHeaderList::const_iterator::const_iterator(
    const spdy::SpdyHeaderBlock* header_list,
    spdy::SpdyHeaderBlock::const_iterator header_list_iterator)
    : header_list_(header_list),
      header_list_iterator_(header_list_iterator),
      value_start_(0) {
  UpdateHeaderField();
}

bool ValueSplittingHeaderList::const_iterator::operator==(
    const const_iterator& other) const {
  return header_list_iterator_ == other.header_list_iterator_ &&
         value_start_ == other.value_start_;
}

bool ValueSplittingHeaderList::const_iterator::operator!=(
    const const_iterator& other) const {
  return !(*this == other);
}

const ValueSplittingHeaderList::const_iterator&
ValueSplittingHeaderList::const_iterator::operator++() {
  if (value_end_ == QuicStringPiece::npos) {
    // This was the last frament within |*header_list_iterator_|,
    // move on to the next header element of |header_list_|.
    ++header_list_iterator_;
    value_start_ = 0;
  } else {
    // Find the next fragment within |*header_list_iterator_|.
    value_start_ = value_end_ + 1;
  }
  UpdateHeaderField();

  return *this;
}

const ValueSplittingHeaderList::value_type&
    ValueSplittingHeaderList::const_iterator::operator*() const {
  return header_field_;
}
const ValueSplittingHeaderList::value_type*
    ValueSplittingHeaderList::const_iterator::operator->() const {
  return &header_field_;
}

void ValueSplittingHeaderList::const_iterator::UpdateHeaderField() {
  DCHECK(value_start_ != QuicStringPiece::npos);

  if (header_list_iterator_ == header_list_->end()) {
    return;
  }

  const QuicStringPiece name = header_list_iterator_->first;

  value_end_ = header_list_iterator_->second.find('\0', value_start_);
  const QuicStringPiece::size_type value_length =
      value_end_ == QuicStringPiece::npos ? QuicStringPiece::npos
                                          : value_end_ - value_start_;
  const QuicStringPiece value =
      header_list_iterator_->second.substr(value_start_, value_length);

  header_field_ = std::make_pair(name, value);
}

ValueSplittingHeaderList::ValueSplittingHeaderList(
    const spdy::SpdyHeaderBlock* header_list)
    : header_list_(header_list) {
  DCHECK(header_list_);
}

ValueSplittingHeaderList::const_iterator ValueSplittingHeaderList::begin()
    const {
  return const_iterator(header_list_, header_list_->begin());
}

ValueSplittingHeaderList::const_iterator ValueSplittingHeaderList::end() const {
  return const_iterator(header_list_, header_list_->end());
}

}  // namespace quic
