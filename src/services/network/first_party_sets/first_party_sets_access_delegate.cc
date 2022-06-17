// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/network/first_party_sets/first_party_sets_access_delegate.h"

#include <utility>

#include "base/stl_util.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/first_party_set_metadata.h"

namespace network {

namespace {

bool IsEnabled(const mojom::FirstPartySetsAccessDelegateParamsPtr& params) {
  return params.is_null() || params->enabled;
}

}  // namespace

FirstPartySetsAccessDelegate::FirstPartySetsAccessDelegate(
    mojo::PendingReceiver<mojom::FirstPartySetsAccessDelegate> receiver,
    mojom::FirstPartySetsAccessDelegateParamsPtr params,
    FirstPartySetsManager* const manager)
    : manager_(manager),
      context_config_(FirstPartySetsContextConfig(IsEnabled(params))),
      pending_queries_(
          IsEnabled(params) && receiver.is_valid() && manager_->is_enabled()
              ? std::make_unique<base::circular_deque<base::OnceClosure>>()
              : nullptr) {
  if (receiver.is_valid())
    receiver_.Bind(std::move(receiver));
}

FirstPartySetsAccessDelegate::~FirstPartySetsAccessDelegate() = default;

void FirstPartySetsAccessDelegate::NotifyReady() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  InvokePendingQueries();
}

absl::optional<net::FirstPartySetMetadata>
FirstPartySetsAccessDelegate::ComputeMetadata(
    const net::SchemefulSite& site,
    const net::SchemefulSite* top_frame_site,
    const std::set<net::SchemefulSite>& party_context,
    base::OnceCallback<void(net::FirstPartySetMetadata)> callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pending_queries_) {
    // base::Unretained() is safe because `this` owns `pending_queries_` and
    // `pending_queries_` will not run the enqueued callbacks after `this` is
    // destroyed.
    EnqueuePendingQuery(base::BindOnce(
        &FirstPartySetsAccessDelegate::ComputeMetadataAndInvoke,
        base::Unretained(this), site, base::OptionalFromPtr(top_frame_site),
        party_context, std::move(callback)));
    return absl::nullopt;
  }

  return manager_->ComputeMetadata(site, top_frame_site, party_context,
                                   context_config_, std::move(callback));
}

absl::optional<FirstPartySetsAccessDelegate::SetsByOwner>
FirstPartySetsAccessDelegate::Sets(
    base::OnceCallback<void(FirstPartySetsAccessDelegate::SetsByOwner)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pending_queries_) {
    // base::Unretained() is safe because `this` owns `pending_queries_` and
    // `pending_queries_` will not run the enqueued callbacks after `this` is
    // destroyed.
    EnqueuePendingQuery(
        base::BindOnce(&FirstPartySetsAccessDelegate::SetsAndInvoke,
                       base::Unretained(this), std::move(callback)));
    return absl::nullopt;
  }

  return manager_->Sets(context_config_, std::move(callback));
}

absl::optional<FirstPartySetsAccessDelegate::OwnerResult>
FirstPartySetsAccessDelegate::FindOwner(
    const net::SchemefulSite& site,
    base::OnceCallback<void(FirstPartySetsAccessDelegate::OwnerResult)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pending_queries_) {
    // base::Unretained() is safe because `this` owns `pending_queries_` and
    // `pending_queries_` will not run the enqueued callbacks after `this` is
    // destroyed.
    EnqueuePendingQuery(
        base::BindOnce(&FirstPartySetsAccessDelegate::FindOwnerAndInvoke,
                       base::Unretained(this), site, std::move(callback)));
    return absl::nullopt;
  }

  return manager_->FindOwner(site, context_config_, std::move(callback));
}

absl::optional<FirstPartySetsAccessDelegate::OwnersResult>
FirstPartySetsAccessDelegate::FindOwners(
    const base::flat_set<net::SchemefulSite>& sites,
    base::OnceCallback<void(FirstPartySetsAccessDelegate::OwnersResult)>
        callback) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (pending_queries_) {
    // base::Unretained() is safe because `this` owns `pending_queries_` and
    // `pending_queries_` will not run the enqueued callbacks after `this` is
    // destroyed.
    EnqueuePendingQuery(
        base::BindOnce(&FirstPartySetsAccessDelegate::FindOwnersAndInvoke,
                       base::Unretained(this), sites, std::move(callback)));
    return absl::nullopt;
  }

  return manager_->FindOwners(sites, context_config_, std::move(callback));
}

void FirstPartySetsAccessDelegate::ComputeMetadataAndInvoke(
    const net::SchemefulSite& site,
    const absl::optional<net::SchemefulSite> top_frame_site,
    const std::set<net::SchemefulSite>& party_context,
    base::OnceCallback<void(net::FirstPartySetMetadata)> callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::pair<base::OnceCallback<void(net::FirstPartySetMetadata)>,
            base::OnceCallback<void(net::FirstPartySetMetadata)>>
      callbacks = base::SplitOnceCallback(std::move(callback));

  absl::optional<net::FirstPartySetMetadata> sync_result =
      manager_->ComputeMetadata(site, base::OptionalOrNullptr(top_frame_site),
                                party_context, context_config_,
                                std::move(callbacks.first));

  if (sync_result.has_value())
    std::move(callbacks.second).Run(std::move(sync_result.value()));
}

void FirstPartySetsAccessDelegate::SetsAndInvoke(
    base::OnceCallback<void(FirstPartySetsAccessDelegate::SetsByOwner)>
        callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::pair<base::OnceCallback<void(FirstPartySetsAccessDelegate::SetsByOwner)>,
            base::OnceCallback<void(FirstPartySetsAccessDelegate::SetsByOwner)>>
      callbacks = base::SplitOnceCallback(std::move(callback));

  absl::optional<FirstPartySetsAccessDelegate::SetsByOwner> sync_result =
      manager_->Sets(context_config_, std::move(callbacks.first));

  if (sync_result.has_value())
    std::move(callbacks.second).Run(std::move(sync_result.value()));
}

void FirstPartySetsAccessDelegate::FindOwnerAndInvoke(
    const net::SchemefulSite& site,
    base::OnceCallback<void(FirstPartySetsAccessDelegate::OwnerResult)>
        callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::pair<base::OnceCallback<void(FirstPartySetsAccessDelegate::OwnerResult)>,
            base::OnceCallback<void(FirstPartySetsAccessDelegate::OwnerResult)>>
      callbacks = base::SplitOnceCallback(std::move(callback));

  absl::optional<FirstPartySetsAccessDelegate::OwnerResult> sync_result =
      manager_->FindOwner(site, context_config_, std::move(callbacks.first));

  if (sync_result.has_value())
    std::move(callbacks.second).Run(sync_result.value());
}

void FirstPartySetsAccessDelegate::FindOwnersAndInvoke(
    const base::flat_set<net::SchemefulSite>& sites,
    base::OnceCallback<void(FirstPartySetsAccessDelegate::OwnersResult)>
        callback) const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  std::pair<
      base::OnceCallback<void(FirstPartySetsAccessDelegate::OwnersResult)>,
      base::OnceCallback<void(FirstPartySetsAccessDelegate::OwnersResult)>>
      callbacks = base::SplitOnceCallback(std::move(callback));

  absl::optional<FirstPartySetsAccessDelegate::OwnersResult> sync_result =
      manager_->FindOwners(sites, context_config_, std::move(callbacks.first));

  if (sync_result.has_value())
    std::move(callbacks.second).Run(sync_result.value());
}

void FirstPartySetsAccessDelegate::InvokePendingQueries() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!pending_queries_)
    return;

  while (!pending_queries_->empty()) {
    base::OnceClosure query_task = std::move(pending_queries_->front());
    pending_queries_->pop_front();
    std::move(query_task).Run();
  }

  pending_queries_ = nullptr;
}

void FirstPartySetsAccessDelegate::EnqueuePendingQuery(
    base::OnceClosure run_query) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(pending_queries_);

  pending_queries_->push_back(std::move(run_query));
}

}  // namespace network