// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "tools/binary_size/libsupersize/caspian/model.h"

#include "tools/binary_size/libsupersize/caspian/file_format.h"

using namespace caspian;

Symbol::Symbol() = default;
Symbol::Symbol(const Symbol& other) = default;

TreeNode::TreeNode() = default;
TreeNode::~TreeNode() = default;

SizeInfo::SizeInfo() = default;
SizeInfo::~SizeInfo() = default;
