// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SERVICES_UI_WS_IDS_H_
#define SERVICES_UI_WS_IDS_H_

#include <stdint.h>

#include <string>
#include <tuple>

#include "base/containers/hash_tables.h"
#include "base/hash.h"
#include "base/strings/stringprintf.h"
#include "components/viz/common/surfaces/frame_sink_id.h"
#include "services/ui/common/types.h"
#include "services/ui/common/util.h"

namespace ui {
namespace ws {

// A client id used to indicate no client. That is, no WindowTree ever gets this
// id.
const ClientSpecificId kInvalidClientId = 0;

const Id kInvalidTransportId = 0;

// A client id used to indicate WindowServer.
const ClientSpecificId kWindowServerClientId = 1;

// Every window has a unique id associated with it (WindowId). The id is a
// combination of the id assigned to the client (the high order bits) and
// a unique id for the window. Each client (WindowTree) refers to the window
// by an id assigned by the client (ClientWindowId). To facilitate this
// WindowTree maintains a mapping between WindowId and ClientWindowId.
//
// This model works when the client initiates creation of windows, which is
// the typical use case. Embed roots and the WindowManager are special, they
// get access to windows created by other clients. These clients see the
// id assigned on the server. Such clients have to take care that they only
// create windows using their client id. To do otherwise could result in
// multiple windows having the same ClientWindowId. WindowTree enforces
// that embed roots use the client id in creating the window id to avoid
// possible conflicts.

// Used for ids assigned by the client.
using ClientWindowId = viz::FrameSinkId;

struct WindowId {
  constexpr WindowId(ClientSpecificId client_id, ClientSpecificId window_id)
      : client_id(client_id), window_id(window_id) {}
  constexpr WindowId() : client_id(0), window_id(0) {}

  bool operator==(const WindowId& other) const {
    return other.client_id == client_id && other.window_id == window_id;
  }

  bool operator!=(const WindowId& other) const { return !(*this == other); }

  bool operator<(const WindowId& other) const {
    return std::tie(client_id, window_id) <
           std::tie(other.client_id, other.window_id);
  }

  std::string ToString() const {
    return base::StringPrintf("%u:%u", client_id, window_id);
  }

  ClientSpecificId client_id;
  ClientSpecificId window_id;
};

inline WindowId WindowIdFromTransportId(Id id) {
  return WindowId(HiWord(id), LoWord(id));
}

// Returns a WindowId that is reserved to indicate no window. That is, no window
// will ever be created with this id.
inline WindowId InvalidWindowId() {
  return WindowId(kInvalidClientId, 0);
}

// Returns a root window id with a given index offset.
inline WindowId RootWindowId(uint16_t index) {
  return WindowId(kWindowServerClientId, 2 + index);
}

using ClientWindowIdHash = viz::FrameSinkIdHash;

struct WindowIdHash {
  size_t operator()(const WindowId& id) const {
    return (id.client_id << 16) | id.window_id;
  }
};

}  // namespace ws
}  // namespace ui

#endif  // SERVICES_UI_WS_IDS_H_
