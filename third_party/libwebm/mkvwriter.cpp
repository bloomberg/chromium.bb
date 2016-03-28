//
// Copyright (c) 2016, Alliance for Open Media. All rights reserved
//
// This source code is subject to the terms of the BSD 2 Clause License and
// the Alliance for Open Media Patent License 1.0. If the BSD 2 Clause License
// was not distributed with this source code in the LICENSE file, you can
// obtain it at www.aomedia.org/license/software. If the Alliance for Open
// Media Patent License 1.0 was not distributed with this source code in the
// PATENTS file, you can obtain it at www.aomedia.org/license/patent.
//
#include "mkvwriter.hpp"

#ifdef _MSC_VER
#include <share.h>  // for _SH_DENYWR
#endif

#include <new>

namespace mkvmuxer {

MkvWriter::MkvWriter() : file_(NULL), writer_owns_file_(true) {}

MkvWriter::MkvWriter(FILE* fp) : file_(fp), writer_owns_file_(false) {}

MkvWriter::~MkvWriter() { Close(); }

int32 MkvWriter::Write(const void* buffer, uint32 length) {
  if (!file_)
    return -1;

  if (length == 0)
    return 0;

  if (buffer == NULL)
    return -1;

  const size_t bytes_written = fwrite(buffer, 1, length, file_);

  return (bytes_written == length) ? 0 : -1;
}

bool MkvWriter::Open(const char* filename) {
  if (filename == NULL)
    return false;

  if (file_)
    return false;

#ifdef _MSC_VER
  file_ = _fsopen(filename, "wb", _SH_DENYWR);
#else
  file_ = fopen(filename, "wb");
#endif
  if (file_ == NULL)
    return false;
  return true;
}

void MkvWriter::Close() {
  if (file_ && writer_owns_file_) {
    fclose(file_);
  }
  file_ = NULL;
}

int64 MkvWriter::Position() const {
  if (!file_)
    return 0;

#ifdef _MSC_VER
  return _ftelli64(file_);
#else
  return ftell(file_);
#endif
}

int32 MkvWriter::Position(int64 position) {
  if (!file_)
    return -1;

#ifdef _MSC_VER
  return _fseeki64(file_, position, SEEK_SET);
#else
  return fseek(file_, position, SEEK_SET);
#endif
}

bool MkvWriter::Seekable() const { return true; }

void MkvWriter::ElementStartNotify(uint64, int64) {}

}  // namespace mkvmuxer
