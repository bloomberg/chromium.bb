// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Contains the declaration of a few helper functions used for generating sync
// URLs.

#ifndef SYNC_ENGINE_NET_URL_TRANSLATOR_H_
#define SYNC_ENGINE_NET_URL_TRANSLATOR_H_

#include <string>

namespace syncer {

// Convenience wrappers around CgiEscapePath(), used by gaia_auth.
std::string CgiEscapeString(const char* src);
std::string CgiEscapeString(const std::string& src);

// This method appends the query string to the sync server path.
std::string MakeSyncServerPath(const std::string& path,
                               const std::string& query_string);

std::string MakeSyncQueryString(const std::string& client_id);

}  // namespace syncer

#endif  // SYNC_ENGINE_NET_URL_TRANSLATOR_H_
