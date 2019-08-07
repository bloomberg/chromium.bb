// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/subresource_filter/content/browser/verified_ruleset_dealer.h"

#include <utility>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/files/file.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task_runner_util.h"
#include "base/trace_event/trace_event.h"
#include "components/subresource_filter/core/common/indexed_ruleset.h"
#include "components/subresource_filter/core/common/memory_mapped_ruleset.h"

namespace subresource_filter {

// VerifiedRulesetDealer and its Handle. ---------------------------------------

VerifiedRulesetDealer::VerifiedRulesetDealer() = default;
VerifiedRulesetDealer::~VerifiedRulesetDealer() = default;

base::File VerifiedRulesetDealer::OpenAndSetRulesetFile(
    int expected_checksum,
    const base::FilePath& file_path) {
  DCHECK(CalledOnValidSequence());
  // On Windows, open the file with FLAG_SHARE_DELETE to allow deletion while
  // there are handles to it still open.
  base::File file(file_path, base::File::FLAG_OPEN | base::File::FLAG_READ |
                                 base::File::FLAG_SHARE_DELETE);
  TRACE_EVENT1(TRACE_DISABLED_BY_DEFAULT("loading"),
               "VerifiedRulesetDealer::OpenAndSetRulesetFile", "file_valid",
               file.IsValid());
  if (file.IsValid()) {
    SetRulesetFile(file.Duplicate());
    expected_checksum_ = expected_checksum;
  }
  return file;
}

void VerifiedRulesetDealer::SetRulesetFile(base::File ruleset_file) {
  RulesetDealer::SetRulesetFile(std::move(ruleset_file));
  status_ = RulesetVerificationStatus::kNotVerified;
  expected_checksum_ = 0;
}

scoped_refptr<const MemoryMappedRuleset> VerifiedRulesetDealer::GetRuleset() {
  DCHECK(CalledOnValidSequence());
  TRACE_EVENT0(TRACE_DISABLED_BY_DEFAULT("loading"),
               "VerifiedRulesetDealer::GetRuleset");

  switch (status_) {
    case RulesetVerificationStatus::kNotVerified: {
      auto ruleset = RulesetDealer::GetRuleset();
      if (ruleset) {
        if (IndexedRulesetMatcher::Verify(ruleset->data(), ruleset->length(),
                                          expected_checksum_)) {
          status_ = RulesetVerificationStatus::kIntact;
        } else {
          status_ = RulesetVerificationStatus::kCorrupt;
          ruleset.reset();
        }
      } else {
        status_ = RulesetVerificationStatus::kInvalidFile;
      }
      UMA_HISTOGRAM_ENUMERATION("SubresourceFilter.RulesetVerificationStatus",
                                status_);
      return ruleset;
    }
    case RulesetVerificationStatus::kIntact: {
      auto ruleset = RulesetDealer::GetRuleset();
      // Note, |ruleset| can still be nullptr here if mmap fails, despite the
      // fact that it must have succeeded previously.
      return ruleset;
    }
    case RulesetVerificationStatus::kCorrupt:
    case RulesetVerificationStatus::kInvalidFile:
      return nullptr;
    default:
      NOTREACHED();
      return nullptr;
  }
}

VerifiedRulesetDealer::Handle::Handle(
    scoped_refptr<base::SequencedTaskRunner> task_runner)
    : task_runner_(task_runner.get()),
      dealer_(new VerifiedRulesetDealer,
              base::OnTaskRunnerDeleter(std::move(task_runner))) {}

VerifiedRulesetDealer::Handle::~Handle() = default;

void VerifiedRulesetDealer::Handle::GetDealerAsync(
    base::OnceCallback<void(VerifiedRulesetDealer*)> callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());

  // NOTE: Properties of the sequenced |task_runner| guarantee that the
  // |callback| will always be provided with a valid pointer, because the
  // corresponding task will be posted *before* a task to delete the pointer
  // upon destruction of |this| Handler.
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(std::move(callback), dealer_.get()));
}

void VerifiedRulesetDealer::Handle::TryOpenAndSetRulesetFile(
    const base::FilePath& path,
    int expected_checksum,
    base::OnceCallback<void(base::File)> callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  // |base::Unretained| is safe here because the |OpenAndSetRulesetFile| task
  // will be posted before a task to delete the pointer upon destruction of
  // |this| Handler.
  base::PostTaskAndReplyWithResult(
      task_runner_, FROM_HERE,
      base::BindOnce(&VerifiedRulesetDealer::OpenAndSetRulesetFile,
                     base::Unretained(dealer_.get()), expected_checksum, path),
      std::move(callback));
}

// VerifiedRuleset and its Handle. ---------------------------------------------

VerifiedRuleset::VerifiedRuleset() {
  sequence_checker_.DetachFromSequence();
}

VerifiedRuleset::~VerifiedRuleset() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
}

void VerifiedRuleset::Initialize(VerifiedRulesetDealer* dealer) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  DCHECK(dealer);
  ruleset_ = dealer->GetRuleset();
}

VerifiedRuleset::Handle::Handle(VerifiedRulesetDealer::Handle* dealer_handle)
    : task_runner_(dealer_handle->task_runner()),
      ruleset_(new VerifiedRuleset, base::OnTaskRunnerDeleter(task_runner_)) {
  dealer_handle->GetDealerAsync(base::BindOnce(
      &VerifiedRuleset::Initialize, base::Unretained(ruleset_.get())));
}

VerifiedRuleset::Handle::~Handle() {
  DCHECK(sequence_checker_.CalledOnValidSequence());
}

void VerifiedRuleset::Handle::GetRulesetAsync(
    base::OnceCallback<void(VerifiedRuleset*)> callback) {
  DCHECK(sequence_checker_.CalledOnValidSequence());
  task_runner_->PostTask(FROM_HERE,
                         base::BindOnce(std::move(callback), ruleset_.get()));
}

}  // namespace subresource_filter
