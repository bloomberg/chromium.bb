// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/local_site_characteristics_webcontents_observer.h"

#include "base/metrics/histogram_macros.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/resource_coordinator/local_site_characteristics_data_store_factory.h"
#include "chrome/browser/resource_coordinator/time.h"
#include "chrome/browser/resource_coordinator/utils.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"

namespace resource_coordinator {

namespace {

using LoadingState = TabLoadTracker::LoadingState;

TabVisibility ContentVisibilityToRCVisibility(content::Visibility visibility) {
  if (visibility == content::Visibility::VISIBLE)
    return TabVisibility::kForeground;
  return TabVisibility::kBackground;
}

bool g_skip_observer_registration_for_testing = false;

}  // namespace

// static
void LocalSiteCharacteristicsWebContentsObserver::
    SkipObserverRegistrationForTesting() {
  g_skip_observer_registration_for_testing = true;
}

LocalSiteCharacteristicsWebContentsObserver::
    LocalSiteCharacteristicsWebContentsObserver(
        content::WebContents* web_contents)
    : content::WebContentsObserver(web_contents) {
  if (!g_skip_observer_registration_for_testing) {
    // The PageSignalReceiver has to be enabled in order to properly track the
    // non-persistent notification events.
    DCHECK(PageSignalReceiver::IsEnabled());

    TabLoadTracker::Get()->AddObserver(this);
    page_signal_receiver_ = PageSignalReceiver::GetInstance();
    DCHECK(page_signal_receiver_);
    page_signal_receiver_->AddObserver(this);
  }
}

LocalSiteCharacteristicsWebContentsObserver::
    ~LocalSiteCharacteristicsWebContentsObserver() {
  DCHECK(!writer_);
}

void LocalSiteCharacteristicsWebContentsObserver::OnVisibilityChanged(
    content::Visibility visibility) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!writer_)
    return;

  auto rc_visibility = ContentVisibilityToRCVisibility(visibility);
  UpdateBackgroundedTime(rc_visibility);
  writer_->NotifySiteVisibilityChanged(rc_visibility);
}

void LocalSiteCharacteristicsWebContentsObserver::WebContentsDestroyed() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (!g_skip_observer_registration_for_testing) {
    TabLoadTracker::Get()->RemoveObserver(this);
    page_signal_receiver_->RemoveObserver(this);
  }
  writer_.reset();
  writer_origin_ = url::Origin();
}

void LocalSiteCharacteristicsWebContentsObserver::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(navigation_handle);

  // Ignore the navigation events happening in a subframe of in the same
  // document.
  if (!navigation_handle->IsInMainFrame() ||
      navigation_handle->IsSameDocument()) {
    return;
  }

  first_time_title_set_ = false;
  first_time_favicon_set_ = false;

  if (!navigation_handle->HasCommitted())
    return;

  const url::Origin new_origin =
      url::Origin::Create(navigation_handle->GetURL());

  if (writer_ && new_origin == writer_origin_)
    return;

  writer_.reset();
  writer_origin_ = url::Origin();

  if (!URLShouldBeStoredInLocalDatabase(navigation_handle->GetURL()))
    return;

  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  DCHECK(profile);
  SiteCharacteristicsDataStore* data_store =
      LocalSiteCharacteristicsDataStoreFactory::GetForProfile(profile);
  DCHECK(data_store);
  auto rc_visibility =
      ContentVisibilityToRCVisibility(web_contents()->GetVisibility());
  writer_ = data_store->GetWriterForOrigin(new_origin, rc_visibility);
  UpdateBackgroundedTime(rc_visibility);

  // The writer is initially in an unloaded state, load it if necessary.
  if (TabLoadTracker::Get()->GetLoadingState(web_contents()) ==
      LoadingState::LOADED) {
    OnSiteLoaded();
  }

  writer_origin_ = new_origin;
}

void LocalSiteCharacteristicsWebContentsObserver::TitleWasSet(
    content::NavigationEntry* entry) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // TODO(sebmarchand): Check if the title is always set at least once before
  // loading completes, in which case this check could be removed.
  if (!first_time_title_set_) {
    first_time_title_set_ = true;
    return;
  }

  MaybeNotifyBackgroundFeatureUsage(
      &SiteCharacteristicsDataWriter::NotifyUpdatesTitleInBackground,
      FeatureType::kTitleChange);
}

void LocalSiteCharacteristicsWebContentsObserver::DidUpdateFaviconURL(
    const std::vector<content::FaviconURL>& candidates) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!first_time_favicon_set_) {
    first_time_favicon_set_ = true;
    return;
  }

  MaybeNotifyBackgroundFeatureUsage(
      &SiteCharacteristicsDataWriter::NotifyUpdatesFaviconInBackground,
      FeatureType::kFaviconChange);
}

void LocalSiteCharacteristicsWebContentsObserver::OnAudioStateChanged(
    bool audible) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!audible)
    return;

  MaybeNotifyBackgroundFeatureUsage(
      &SiteCharacteristicsDataWriter::NotifyUsesAudioInBackground,
      FeatureType::kAudioUsage);
}

void LocalSiteCharacteristicsWebContentsObserver::OnLoadingStateChange(
    content::WebContents* contents,
    LoadingState old_loading_state,
    LoadingState new_loading_state) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (web_contents() != contents)
    return;

  if (!writer_)
    return;

  // Ignore the transitions from/to an UNLOADED state.
  if (new_loading_state == LoadingState::LOADED) {
    OnSiteLoaded();
  } else if (old_loading_state == LoadingState::LOADED) {
    writer_->NotifySiteUnloaded();
    loaded_time_ = base::TimeTicks();
  }
}

void LocalSiteCharacteristicsWebContentsObserver::
    OnNonPersistentNotificationCreated(
        content::WebContents* contents,
        const PageNavigationIdentity& page_navigation_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (web_contents() != contents)
    return;

  DCHECK_NE(nullptr, page_signal_receiver_);

  // Don't notify the writer if the origin of this navigation event isn't the
  // same as the one tracked by the writer.
  if ((page_signal_receiver_->GetNavigationIDForWebContents(contents) ==
       page_navigation_id.navigation_id) ||
      url::Origin::Create(GURL(page_navigation_id.url))
          .IsSameOriginWith(writer_origin_)) {
    MaybeNotifyBackgroundFeatureUsage(
        &SiteCharacteristicsDataWriter::NotifyUsesNotificationsInBackground,
        FeatureType::kNotificationUsage);
  }
}

void LocalSiteCharacteristicsWebContentsObserver::OnLoadTimePerformanceEstimate(
    content::WebContents* contents,
    const PageNavigationIdentity& page_navigation_id,
    base::TimeDelta cpu_usage_estimate,
    uint64_t private_footprint_kb_estimate) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (web_contents() != contents)
    return;

  DCHECK_NE(nullptr, page_signal_receiver_);

  bool late_notification = page_signal_receiver_->GetNavigationIDForWebContents(
                               contents) != page_navigation_id.navigation_id;

  UMA_HISTOGRAM_BOOLEAN(
      "ResourceCoordinator.Measurement.Memory.LateNotification",
      late_notification);

  // Don't notify the writer if the origin of this navigation event isn't the
  // same as the one tracked by the writer.
  // TODO(siggi): Deal with late notifications by getting a writer for
  //     the measurement's origin.
  if (!late_notification || url::Origin::Create(GURL(page_navigation_id.url))
                                .IsSameOriginWith(writer_origin_)) {
    if (writer_) {
      writer_->NotifyLoadTimePerformanceMeasurement(
          cpu_usage_estimate, private_footprint_kb_estimate);
    }
  }
}

bool LocalSiteCharacteristicsWebContentsObserver::ShouldIgnoreFeatureUsageEvent(
    FeatureType feature_type) {
  // The feature usage should be ignored if there's no writer for this tab.
  if (!writer_)
    return true;

  // Ignore all features happening before the website gets fully loaded except
  // for the non-persistent notifications.
  if (feature_type != FeatureType::kNotificationUsage &&
      TabLoadTracker::Get()->GetLoadingState(web_contents()) !=
          LoadingState::LOADED) {
    return true;
  }

  // Ignore events if the tab is not in background.
  if (ContentVisibilityToRCVisibility(web_contents()->GetVisibility()) !=
      TabVisibility::kBackground) {
    return true;
  }

  if (feature_type == FeatureType::kTitleChange ||
      feature_type == FeatureType::kFaviconChange) {
    DCHECK(!loaded_time_.is_null());
    if (NowTicks() - loaded_time_ < GetStaticSiteCharacteristicsDatabaseParams()
                                        .title_or_favicon_change_grace_period) {
      return true;
    }
  } else if (feature_type == FeatureType::kAudioUsage) {
    DCHECK(!backgrounded_time_.is_null());
    if (NowTicks() - backgrounded_time_ <
        GetStaticSiteCharacteristicsDatabaseParams().audio_usage_grace_period) {
      return true;
    }
  }

  return false;
}

void LocalSiteCharacteristicsWebContentsObserver::
    MaybeNotifyBackgroundFeatureUsage(
        void (SiteCharacteristicsDataWriter::*method)(),
        FeatureType feature_type) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (ShouldIgnoreFeatureUsageEvent(feature_type))
    return;

  (writer_.get()->*method)();
}

void LocalSiteCharacteristicsWebContentsObserver::OnSiteLoaded() {
  DCHECK(writer_);
  writer_->NotifySiteLoaded();
  loaded_time_ = NowTicks();
}

void LocalSiteCharacteristicsWebContentsObserver::UpdateBackgroundedTime(
    TabVisibility visibility) {
  if (visibility == TabVisibility::kBackground) {
    backgrounded_time_ = NowTicks();
  } else {
    backgrounded_time_ = base::TimeTicks();
  }
}

}  // namespace resource_coordinator
