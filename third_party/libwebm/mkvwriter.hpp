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


#ifndef MKVWRITER_HPP
#define MKVWRITER_HPP

#include <stdio.h>

#include "mkvmuxer.hpp"
#include "mkvmuxertypes.hpp"

namespace mkvmuxer {

// Default implementation of the IMkvWriter interface on Windows.
class MkvWriter : public IMkvWriter {
 public:
  MkvWriter();
  explicit MkvWriter(FILE* fp);
  virtual ~MkvWriter();

  // IMkvWriter interface
  virtual int64 Position() const;
  virtual int32 Position(int64 position);
  virtual bool Seekable() const;
  virtual int32 Write(const void* buffer, uint32 length);
  virtual void ElementStartNotify(uint64 element_id, int64 position);

  // Creates and opens a file for writing. |filename| is the name of the file
  // to open. This function will overwrite the contents of |filename|. Returns
  // true on success.
  bool Open(const char* filename);

  // Closes an opened file.
  void Close();

 private:
  // File handle to output file.
  FILE* file_;
  bool writer_owns_file_;

  LIBWEBM_DISALLOW_COPY_AND_ASSIGN(MkvWriter);
};

}  // end namespace mkvmuxer

#endif  // MKVWRITER_HPP
