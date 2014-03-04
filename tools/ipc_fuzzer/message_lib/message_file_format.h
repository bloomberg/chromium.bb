// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef TOOLS_IPC_FUZZER_MESSAGE_LIB_MESSAGE_FILE_FORMAT_H_
#define TOOLS_IPC_FUZZER_MESSAGE_LIB_MESSAGE_FILE_FORMAT_H_

#include "base/basictypes.h"

// Message file contains IPC messages and message names. Each message type
// has a NameTableEntry mapping the type to a name.
//
// |========================|
// | FileHeader             |
// |========================|
// | Message                |
// |------------------------|
// | Message                |
// |------------------------|
// | ...                    |
// |========================|
// | NameTableEntry         |
// |------------------------|
// | NameTableEntry         |
// |------------------------|
// | ...                    |
// |------------------------|
// | type = 0x0002070f      |
// | string_table_offset = ----+
// |------------------------|  |
// | ...                    |  |
// |========================|  |
// | message name           |  |
// |------------------------|  |
// | message name           |  |
// |------------------------|  |
// | ...                    |  |
// |------------------------|  |
// | "FrameHostMsg_OpenURL" <--+
// |------------------------|
// | ...                    |
// |========================|

namespace ipc_fuzzer {

struct FileHeader {
  static const uint32 kMagicValue = 0x1bcf11ee;
  static const uint32 kCurrentVersion = 1;

  uint32 magic;
  uint32 version;
  uint32 message_count;
  uint32 name_count;
};

struct NameTableEntry {
  uint32 type;
  uint32 string_table_offset;
};

}  // namespace ipc_fuzzer

#endif  // TOOLS_IPC_FUZZER_MESSAGE_LIB_MESSAGE_FILE_FORMAT_H_
