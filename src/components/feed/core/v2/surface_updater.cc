// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/feed/core/v2/surface_updater.h"

#include <tuple>

#include "base/check.h"
#include "base/strings/string_number_conversions.h"
#include "components/feed/core/v2/feed_stream.h"
#include "components/feed/core/v2/metrics_reporter.h"

namespace feed {
namespace {

using DrawState = SurfaceUpdater::DrawState;
using SurfaceInterface = FeedStreamApi::SurfaceInterface;

// Give each kind of zero state a unique name, so that the UI knows if it
// changes.
const char* GetZeroStateSliceId(feedui::ZeroStateSlice::Type type) {
  switch (type) {
    case feedui::ZeroStateSlice::NO_CARDS_AVAILABLE:
      return "no-cards";
    case feedui::ZeroStateSlice::CANT_REFRESH:  // fall-through
    default:
      return "cant-refresh";
  }
}

void AddSharedState(const StreamModel& model,
                    const std::string& shared_state_id,
                    feedui::StreamUpdate* stream_update) {
  const std::string* shared_state_data =
      model.FindSharedStateData(shared_state_id);
  DCHECK(shared_state_data);
  feedui::SharedState* added_shared_state =
      stream_update->add_new_shared_states();
  added_shared_state->set_id(shared_state_id);
  added_shared_state->set_xsurface_shared_state(*shared_state_data);
}

void AddSliceUpdate(const StreamModel& model,
                    ContentRevision content_revision,
                    bool is_content_new,
                    feedui::StreamUpdate* stream_update) {
  if (is_content_new) {
    feedui::Slice* slice = stream_update->add_updated_slices()->mutable_slice();
    slice->set_slice_id(ToString(content_revision));
    const feedstore::Content* content = model.FindContent(content_revision);
    DCHECK(content);
    slice->mutable_xsurface_slice()->set_xsurface_frame(content->frame());
  } else {
    stream_update->add_updated_slices()->set_slice_id(
        ToString(content_revision));
  }
}

void AddLoadingSpinner(bool is_at_top, feedui::StreamUpdate* update) {
  feedui::Slice* slice = update->add_updated_slices()->mutable_slice();
  slice->mutable_loading_spinner_slice()->set_is_at_top(is_at_top);
  slice->set_slice_id(is_at_top ? "loading-spinner" : "load-more-spinner");
}

feedui::StreamUpdate MakeStreamUpdate(
    const std::vector<std::string>& updated_shared_state_ids,
    const base::flat_set<ContentRevision>& already_sent_content,
    const StreamModel* model,
    const DrawState& state) {
  DCHECK(!state.loading_initial || !state.loading_more)
      << "logic bug: requested both top and bottom spinners.";
  feedui::StreamUpdate stream_update;
  // Add content from the model, if it's loaded.
  bool has_content = false;
  if (model) {
    for (ContentRevision content_revision : model->GetContentList()) {
      const bool is_updated = already_sent_content.count(content_revision) == 0;
      AddSliceUpdate(*model, content_revision, is_updated, &stream_update);
      has_content = true;
    }
    for (const std::string& name : updated_shared_state_ids) {
      AddSharedState(*model, name, &stream_update);
    }
  }

  feedui::ZeroStateSlice::Type zero_state_type = state.zero_state_type;
  // If there are no cards, and we aren't loading, force a zero-state.
  // This happens when a model is loaded, but it has no content.
  if (!state.loading_initial && !has_content &&
      state.zero_state_type == feedui::ZeroStateSlice::UNKNOWN) {
    zero_state_type = feedui::ZeroStateSlice::NO_CARDS_AVAILABLE;
  }

  if (zero_state_type != feedui::ZeroStateSlice::UNKNOWN) {
    feedui::Slice* slice = stream_update.add_updated_slices()->mutable_slice();
    slice->mutable_zero_state_slice()->set_type(zero_state_type);
    slice->set_slice_id(GetZeroStateSliceId(zero_state_type));
  } else {
    // Add the initial-load spinner if applicable.
    if (state.loading_initial) {
      AddLoadingSpinner(/*is_at_top=*/true, &stream_update);
    }
    // Add a loading-more spinner if applicable.
    if (state.loading_more) {
      AddLoadingSpinner(/*is_at_top=*/false, &stream_update);
    }
  }

  return stream_update;
}

feedui::StreamUpdate GetUpdateForNewSurface(const DrawState& state,
                                            const StreamModel* model) {
  std::vector<std::string> updated_shared_state_ids;
  if (model) {
    updated_shared_state_ids = model->GetSharedStateIds();
  }
  return MakeStreamUpdate(std::move(updated_shared_state_ids),
                          /*already_sent_content=*/{}, model, state);
}

base::flat_set<ContentRevision> GetContentSet(const StreamModel* model) {
  if (!model)
    return {};
  const std::vector<ContentRevision>& content_list = model->GetContentList();
  return base::flat_set<ContentRevision>(content_list.begin(),
                                         content_list.end());
}

feedui::ZeroStateSlice::Type GetZeroStateType(LoadStreamStatus status) {
  switch (status) {
    case LoadStreamStatus::kNoResponseBody:
    case LoadStreamStatus::kProtoTranslationFailed:
    case LoadStreamStatus::kCannotLoadFromNetworkOffline:
    case LoadStreamStatus::kCannotLoadFromNetworkThrottled:
      return feedui::ZeroStateSlice::CANT_REFRESH;
    case LoadStreamStatus::kNoStatus:
    case LoadStreamStatus::kLoadedFromStore:
    case LoadStreamStatus::kLoadedFromNetwork:
    case LoadStreamStatus::kFailedWithStoreError:
    case LoadStreamStatus::kNoStreamDataInStore:
    case LoadStreamStatus::kModelAlreadyLoaded:
    case LoadStreamStatus::kDataInStoreIsStale:
    case LoadStreamStatus::kDataInStoreIsStaleTimestampInFuture:
    case LoadStreamStatus::kCannotLoadFromNetworkSupressedForHistoryDelete:
    case LoadStreamStatus::kLoadNotAllowedEulaNotAccepted:
    case LoadStreamStatus::kLoadNotAllowedArticlesListHidden:
    case LoadStreamStatus::kCannotParseNetworkResponseBody:
    case LoadStreamStatus::kLoadMoreModelIsNotLoaded:
      break;
  }
  return feedui::ZeroStateSlice::NO_CARDS_AVAILABLE;
}

}  // namespace

bool SurfaceUpdater::DrawState::operator==(const DrawState& rhs) const {
  return std::tie(loading_more, loading_initial, zero_state_type) ==
         std::tie(rhs.loading_more, rhs.loading_initial, rhs.zero_state_type);
}

SurfaceUpdater::SurfaceUpdater(MetricsReporter* metrics_reporter)
    : metrics_reporter_(metrics_reporter) {}
SurfaceUpdater::~SurfaceUpdater() = default;

void SurfaceUpdater::SetModel(StreamModel* model) {
  if (model_ == model)
    return;
  if (model_)
    model_->SetObserver(nullptr);
  model_ = model;
  sent_content_.clear();
  if (model_) {
    model_->SetObserver(this);
    loading_initial_ = loading_initial_ && model_->GetContentList().empty();
    loading_more_ = false;
    SendStreamUpdate(model_->GetSharedStateIds());
    last_draw_state_ = GetState();
  }
}

void SurfaceUpdater::OnUiUpdate(const StreamModel::UiUpdate& update) {
  DCHECK(model_);  // The update comes from the model.
  loading_initial_ = loading_initial_ && model_->GetContentList().empty();
  loading_more_ = loading_more_ && !update.content_list_changed;

  std::vector<std::string> updated_shared_state_ids;
  for (const StreamModel::UiUpdate::SharedStateInfo& info :
       update.shared_states) {
    if (info.updated)
      updated_shared_state_ids.push_back(info.shared_state_id);
  }

  SendStreamUpdate(updated_shared_state_ids);
}

void SurfaceUpdater::SurfaceAdded(SurfaceInterface* surface) {
  SendUpdateToSurface(surface, GetUpdateForNewSurface(GetState(), model_));
  surfaces_.AddObserver(surface);
}

void SurfaceUpdater::SurfaceRemoved(SurfaceInterface* surface) {
  surfaces_.RemoveObserver(surface);
}

void SurfaceUpdater::LoadStreamStarted() {
  load_stream_failed_ = false;
  loading_initial_ = true;
  SendStreamUpdateIfNeeded();
}

void SurfaceUpdater::LoadStreamComplete(bool success,
                                        LoadStreamStatus load_stream_status) {
  loading_initial_ = false;
  load_stream_status_ = load_stream_status;
  load_stream_failed_ = !success;
  SendStreamUpdateIfNeeded();
}

int SurfaceUpdater::GetSliceIndexFromSliceId(const std::string& slice_id) {
  ContentRevision slice_rev = ToContentRevision(slice_id);
  if (slice_rev.is_null())
    return -1;
  int index = 0;
  for (const ContentRevision& rev : model_->GetContentList()) {
    if (rev == slice_rev)
      return index;
    ++index;
  }
  return -1;
}

bool SurfaceUpdater::HasSurfaceAttached() const {
  return surfaces_.might_have_observers();
}

void SurfaceUpdater::SetLoadingMore(bool is_loading) {
  DCHECK(!loading_initial_)
      << "SetLoadingMore while still loading the initial state";
  loading_more_ = is_loading;
  SendStreamUpdateIfNeeded();
}

DrawState SurfaceUpdater::GetState() const {
  DrawState new_state;
  new_state.loading_more = loading_more_;
  new_state.loading_initial = loading_initial_;
  if (load_stream_failed_)
    new_state.zero_state_type = GetZeroStateType(load_stream_status_);
  return new_state;
}

void SurfaceUpdater::SendStreamUpdateIfNeeded() {
  if (last_draw_state_ == GetState()) {
    return;
  }
  SendStreamUpdate({});
}

void SurfaceUpdater::SendStreamUpdate(
    const std::vector<std::string>& updated_shared_state_ids) {
  DrawState state = GetState();

  feedui::StreamUpdate stream_update =
      MakeStreamUpdate(updated_shared_state_ids, sent_content_, model_, state);

  for (SurfaceInterface& surface : surfaces_) {
    SendUpdateToSurface(&surface, stream_update);
  }

  sent_content_ = GetContentSet(model_);
  last_draw_state_ = state;
}

void SurfaceUpdater::SendUpdateToSurface(SurfaceInterface* surface,
                                         const feedui::StreamUpdate& update) {
  surface->StreamUpdate(update);

  // Call |MetricsReporter::SurfaceReceivedContent()| if appropriate.

  bool update_has_content = false;
  for (const feedui::StreamUpdate_SliceUpdate& slice_update :
       update.updated_slices()) {
    if (slice_update.has_slice() && slice_update.slice().has_xsurface_slice()) {
      update_has_content = true;
    }
  }
  if (!update_has_content)
    return;
  metrics_reporter_->SurfaceReceivedContent(surface->GetSurfaceId());
}

}  // namespace feed
