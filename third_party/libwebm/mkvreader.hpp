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


#ifndef MKVREADER_HPP
#define MKVREADER_HPP

#include "mkvparser.hpp"
#include <cstdio>

namespace mkvparser {

class MkvReader : public IMkvReader {
 public:
  MkvReader();
  explicit MkvReader(FILE* fp);
  virtual ~MkvReader();

  int Open(const char*);
  void Close();

  virtual int Read(long long position, long length, unsigned char* buffer);
  virtual int Length(long long* total, long long* available);

 private:
  MkvReader(const MkvReader&);
  MkvReader& operator=(const MkvReader&);

  // Determines the size of the file. This is called either by the constructor
  // or by the Open function depending on file ownership. Returns true on
  // success.
  bool GetFileSize();

  long long m_length;
  FILE* m_file;
  bool reader_owns_file_;
};

}  // end namespace mkvparser

#endif  // MKVREADER_HPP
