// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_INDEX_HELPER_H_
#define EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_INDEX_HELPER_H_

#include <string>
#include <utility>
#include <vector>

#include "base/callback_forward.h"
#include "base/memory/ref_counted.h"
#include "base/optional.h"
#include "extensions/browser/api/declarative_net_request/ruleset_checksum.h"
#include "extensions/browser/api/declarative_net_request/ruleset_source.h"
#include "extensions/common/install_warning.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace extensions {
class Extension;
namespace declarative_net_request {

// A class to help in indexing multiple rulesets.
class IndexHelper : public base::RefCountedThreadSafe<IndexHelper> {
 public:
  struct Result {
    Result();
    ~Result();
    Result(Result&&);
    Result& operator=(Result&&);

    // Non-empty on failure.
    base::Optional<std::string> error;

    // Valid if |error| is base::nullopt. Clients should not use these fields in
    // case of a failure since these may be partially populated.
    std::vector<InstallWarning> warnings;
    std::vector<RulesetChecksum> ruleset_checksums;
  };

  // Starts indexing rulesets. Must be called on a sequence which supports file
  // IO. The |callback| will be dispatched to the same sequence on which
  // IndexStaticRulesets() is called.
  using IndexCallback = base::OnceCallback<void(Result result)>;
  static void IndexStaticRulesets(const Extension& extension,
                                  IndexCallback callback);

  // Synchronously indexes the static rulesets for an extension. Must be called
  // on a sequence which supports file IO. This is potentially unsafe since this
  // parses JSON in-process.
  static Result IndexStaticRulesetsUnsafe(const Extension& extension);

 private:
  friend class base::RefCountedThreadSafe<IndexHelper>;
  using IndexResults = std::vector<
      std::pair<const RulesetSource*, IndexAndPersistJSONRulesetResult>>;

  IndexHelper(std::vector<RulesetSource> sources, IndexCallback callback);
  ~IndexHelper();

  // Starts indexing the rulesets.
  void Start();

  // Callback invoked when indexing of all rulesets is completed.
  void OnAllRulesetsIndexed();

  // Callback invoked when indexing of a single ruleset is completed.
  // |source_index| is the index of the RulesetSource within |sources_|.
  void OnRulesetIndexed(base::OnceClosure ruleset_done_closure,
                        size_t source_index,
                        IndexAndPersistJSONRulesetResult result);

  const std::vector<RulesetSource> sources_;
  IndexCallback callback_;

  // Stores the result for each RulesetSource in |sources_|.
  IndexResults results_;

  // We use a single shared Data Decoder service instance to process all of the
  // rulesets for this IndexHelper.
  data_decoder::DataDecoder decoder_;
};

}  // namespace declarative_net_request
}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_DECLARATIVE_NET_REQUEST_INDEX_HELPER_H_
