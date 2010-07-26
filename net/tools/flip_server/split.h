// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_TOOLS_FLIP_SERVER_SPLIT_H_
#define NET_TOOLS_FLIP_SERVER_SPLIT_H_
#pragma once

#include <vector>
#include "base/string_piece.h"

namespace net {

// Yea, this could be done with less code duplication using
// template magic, I know.
void SplitStringPieceToVector(const base::StringPiece& full,
                              const char* delim,
                              std::vector<base::StringPiece>* vec,
                              bool omit_empty_strings);

}  // namespace net

#endif  // NET_TOOLS_FLIP_SERVER_SPLIT_H_

