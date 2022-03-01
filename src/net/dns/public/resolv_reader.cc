// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "net/dns/public/resolv_reader.h"

#include <netinet/in.h>
#include <resolv.h>
#include <sys/types.h>

#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/check_op.h"
#include "net/base/ip_endpoint.h"

namespace net {

std::unique_ptr<struct __res_state> ResolvReader::GetResState() {
  auto res = std::make_unique<struct __res_state>();
  memset(res.get(), 0, sizeof(struct __res_state));

  if (res_ninit(res.get()) != 0) {
    CloseResState(res.get());
    return nullptr;
  }

  return res;
}

void ResolvReader::CloseResState(struct __res_state* res) {
  res_nclose(res);
}

absl::optional<std::vector<IPEndPoint>> GetNameservers(
    const struct __res_state& res) {
  std::vector<IPEndPoint> nameservers;

  if (!(res.options & RES_INIT))
    return absl::nullopt;

  static_assert(std::extent<decltype(res.nsaddr_list)>() >= MAXNS &&
                    std::extent<decltype(res._u._ext.nsaddrs)>() >= MAXNS,
                "incompatible libresolv res_state");
  DCHECK_LE(res.nscount, MAXNS);
  // Initially, glibc stores IPv6 in |_ext.nsaddrs| and IPv4 in |nsaddr_list|.
  // In res_send.c:res_nsend, it merges |nsaddr_list| into |nsaddrs|,
  // but we have to combine the two arrays ourselves.
  for (int i = 0; i < res.nscount; ++i) {
    IPEndPoint ipe;
    const struct sockaddr* addr = nullptr;
    size_t addr_len = 0;
    if (res.nsaddr_list[i].sin_family) {  // The indicator used by res_nsend.
      addr = reinterpret_cast<const struct sockaddr*>(&res.nsaddr_list[i]);
      addr_len = sizeof res.nsaddr_list[i];
    } else if (res._u._ext.nsaddrs[i]) {
      addr = reinterpret_cast<const struct sockaddr*>(res._u._ext.nsaddrs[i]);
      addr_len = sizeof *res._u._ext.nsaddrs[i];
    } else {
      return absl::nullopt;
    }
    if (!ipe.FromSockAddr(addr, addr_len))
      return absl::nullopt;
    nameservers.push_back(ipe);
  }

  return nameservers;
}

}  // namespace net