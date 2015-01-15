// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "pdf/pdfium/pdfium_api_string_buffer_adapter.h"

#include <string>

#include "base/logging.h"
#include "base/numerics/safe_math.h"
#include "base/strings/string16.h"
#include "base/strings/string_util.h"

namespace chrome_pdf {

template <class StringType>
PDFiumAPIStringBufferAdapter<StringType>::PDFiumAPIStringBufferAdapter(
    StringType* str,
    size_t expected_size,
    bool check_expected_size)
    : str_(str),
      data_(WriteInto(str, expected_size + 1)),
      expected_size_(expected_size),
      check_expected_size_(check_expected_size),
      is_closed_(false) {
}

template <class StringType>
PDFiumAPIStringBufferAdapter<StringType>::~PDFiumAPIStringBufferAdapter() {
  DCHECK(is_closed_);
}

template <class StringType>
void* PDFiumAPIStringBufferAdapter<StringType>::GetData() {
  DCHECK(!is_closed_);
  return data_;
}

template <class StringType>
void PDFiumAPIStringBufferAdapter<StringType>::Close(int actual_size) {
  base::CheckedNumeric<size_t> unsigned_size = actual_size;
  Close(unsigned_size.ValueOrDie());
}

template <class StringType>
void PDFiumAPIStringBufferAdapter<StringType>::Close(size_t actual_size) {
  DCHECK(!is_closed_);
  is_closed_ = true;

  if (check_expected_size_)
    DCHECK_EQ(expected_size_, actual_size);

  if (actual_size > 0) {
    DCHECK((*str_)[actual_size - 1] == 0);
    str_->resize(actual_size - 1);
  } else {
    str_->clear();
  }
}

// explicit instantiations
template class PDFiumAPIStringBufferAdapter<std::string>;
template class PDFiumAPIStringBufferAdapter<base::string16>;

}  // namespace chrome_pdf
