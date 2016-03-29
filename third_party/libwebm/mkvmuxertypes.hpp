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


#ifndef MKVMUXERTYPES_HPP
#define MKVMUXERTYPES_HPP

// Copied from Chromium basictypes.h
// A macro to disallow the copy constructor and operator= functions
// This should be used in the private: declarations for a class
#define LIBWEBM_DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);                       \
  void operator=(const TypeName&)

namespace mkvmuxer {

typedef unsigned char uint8;
typedef short int16;
typedef int int32;
typedef unsigned int uint32;
typedef long long int64;
typedef unsigned long long uint64;

}  // end namespace mkvmuxer

#endif  // MKVMUXERTYPES_HPP
