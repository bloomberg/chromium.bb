// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_BASE_TRANSPORT_INFO_H_
#define NET_BASE_TRANSPORT_INFO_H_

#include <iosfwd>
#include <string>

#include "base/strings/string_piece.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_export.h"

namespace net {

// Specifies the type of a network transport.
enum class TransportType {
  // The transport was established directly to a peer.
  kDirect,
  // The transport was established to a proxy of some kind.
  kProxied,
};

// Returns a string representation of the given transport type.
// The returned StringPiece is static, has no lifetime restrictions.
NET_EXPORT base::StringPiece TransportTypeToString(TransportType type);

// Describes a network transport.
struct NET_EXPORT TransportInfo {
  TransportInfo();
  TransportInfo(TransportType type_arg, IPEndPoint endpoint_arg);
  ~TransportInfo();

  // Instances of this type are comparable for equality.
  bool operator==(const TransportInfo& other) const;
  bool operator!=(const TransportInfo& other) const;

  // Returns a string representation of this struct, suitable for debugging.
  std::string ToString() const;

  // The type of the transport.
  TransportType type = TransportType::kDirect;

  // If |type| is kDirect, then this identifies the peer endpoint.
  // If |type| is kProxied, then this identifies the proxy endpoint.
  IPEndPoint endpoint;
};

// Instances of these types are streamable for easier debugging.
NET_EXPORT std::ostream& operator<<(std::ostream& out, TransportType type);
NET_EXPORT std::ostream& operator<<(std::ostream& out,
                                    const TransportInfo& info);

}  // namespace net

#endif  // NET_BASE_TRANSPORT_INFO_H_
