// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_SERIALIZE_HOST_DESCRIPTIONS_H_
#define CHROME_BROWSER_DEVTOOLS_SERIALIZE_HOST_DESCRIPTIONS_H_

#include <string>
#include <vector>

#include "base/strings/string_piece.h"
#include "base/values.h"

// DevToolsAgentHost description to be serialized by SerializeHostDescriptions.
struct HostDescriptionNode {
  std::string name;
  std::string parent_name;
  base::DictionaryValue representation;
};

// A helper function taking a HostDescriptionNode representation of hosts and
// producing a ListValue representation. The representation contains a list of
// DictionaryValue for each rooti host, and has DictionaryValues of children
// injected into a ListValue keyed |child_key| in the parent's DictionaryValue.
base::ListValue SerializeHostDescriptions(
    std::vector<HostDescriptionNode> hosts,
    base::StringPiece child_key);

#endif  // CHROME_BROWSER_DEVTOOLS_SERIALIZE_HOST_DESCRIPTIONS_H_
