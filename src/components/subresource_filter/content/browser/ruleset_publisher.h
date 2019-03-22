// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_RULESET_PUBLISHER_H_
#define COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_RULESET_PUBLISHER_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/files/file.h"
#include "base/files/file_path.h"
#include "base/memory/scoped_refptr.h"
#include "base/single_thread_task_runner.h"
#include "components/subresource_filter/content/browser/verified_ruleset_dealer.h"

class PrefRegistrySimple;
class PrefService;

namespace base {
namespace trace_event {
class TracedValue;
}  // namespace trace_event
}  // namespace base

namespace subresource_filter {

// Encapsulates information about a version of unindexed subresource
// filtering rules on disk.
struct UnindexedRulesetInfo {
  UnindexedRulesetInfo();
  ~UnindexedRulesetInfo();

  // The version of the ruleset contents. Because the wire format of unindexed
  // rules is expected to be stable over time (at least backwards compatible),
  // the unindexed ruleset is uniquely identified by its content version.
  //
  // The version string must not be empty, but can be any string otherwise.
  // There is no ordering defined on versions.
  std::string content_version;

  // The path to the file containing the unindexed subresource filtering rules.
  base::FilePath ruleset_path;

  // The (optional) path to a file containing the applicable license, which will
  // be copied next to the indexed ruleset. For convenience, the lack of license
  // can be indicated not only by setting |license_path| to empty, but also by
  // setting it to any non existent path.
  base::FilePath license_path;
};

// Encapsulates the combination of the binary format version of the indexed
// ruleset, and the version of the ruleset contents.
//
// In contrast to the unindexed ruleset, the binary format of the index data
// structures is expected to evolve over time, so the indexed ruleset is
// identified by a pair of versions: the content version of the rules that have
// been indexed; and the binary format version of the indexed data structures.
// It also contains a checksum of the data, to ensure it hasn't been corrupted.
struct IndexedRulesetVersion {
  IndexedRulesetVersion();
  IndexedRulesetVersion(const std::string& content_version, int format_version);
  ~IndexedRulesetVersion();
  IndexedRulesetVersion& operator=(const IndexedRulesetVersion&);

  static void RegisterPrefs(PrefRegistrySimple* registry);
  static int CurrentFormatVersion();

  bool IsValid() const;
  bool IsCurrentFormatVersion() const;

  void SaveToPrefs(PrefService* local_state) const;
  void ReadFromPrefs(PrefService* local_state);

  std::unique_ptr<base::trace_event::TracedValue> ToTracedValue() const;

  std::string content_version;
  int format_version = 0;
  int checksum = 0;
};

// Interface for a RulesetService that defines operations that distribute the
// ruleset to the renderer processes via the RulesetDealer.
class RulesetPublisher {
 public:
  virtual ~RulesetPublisher() = default;

  // Schedules file open and use it as ruleset file. In the case of success,
  // the new and valid |base::File| is passed to |callback|. In the case of
  // error an invalid |base::File| is passed to |callback|. The previous
  // ruleset file will be used (if any).
  virtual void TryOpenAndSetRulesetFile(
      const base::FilePath& file_path,
      int expected_checksum,
      base::OnceCallback<void(base::File)> callback) = 0;

  // Redistributes the new version of the |ruleset| to all existing consumers,
  // and sets up |ruleset| to be distributed to all future consumers.
  virtual void PublishNewRulesetVersion(base::File ruleset_data) = 0;

  // Task queue for best effort tasks in the thread the object was created in.
  // Used for tasks triggered on RulesetService instantiation so it doesn't
  // interfere with startup.  Runs in the UI thread.
  virtual scoped_refptr<base::SingleThreadTaskRunner>
  BestEffortTaskRunner() = 0;

  // Gets the ruleset dealer associated with the RulesetPublisher.
  virtual VerifiedRulesetDealer::Handle* GetRulesetDealer() = 0;

  // Set the callback on publish associated with the RulesetPublisher.
  virtual void SetRulesetPublishedCallbackForTesting(
      base::OnceClosure callback) = 0;
};

}  // namespace subresource_filter

#endif  // COMPONENTS_SUBRESOURCE_FILTER_CONTENT_BROWSER_RULESET_SERVICE_H_
