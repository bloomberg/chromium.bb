// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FENCED_FRAME_FENCED_FRAME_URL_MAPPING_H_
#define CONTENT_BROWSER_FENCED_FRAME_FENCED_FRAME_URL_MAPPING_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/memory/raw_ptr.h"
#include "content/common/content_export.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "third_party/blink/public/mojom/fenced_frame/fenced_frame.mojom.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace content {

extern const char kURNUUIDprefix[];

struct AdAuctionData {
  url::Origin interest_group_owner;
  std::string interest_group_name;
};

using ReportingMetadata = blink::mojom::FencedFrameReporting;

// Keeps a mapping of fenced frames URN:UUID and URL. Also keeps a set of
// pending mapped URN:UUIDs to support asynchronous mapping. See
// https://github.com/WICG/fenced-frame/blob/master/explainer/opaque_src.md
class CONTENT_EXPORT FencedFrameURLMapping {
 public:
  // The metadata for the shared storage runURLSelectionOperation's budget,
  // which includes the shared storage's origin and the amount of budget to
  // charge when a fenced frame that originates from the URN is navigating a top
  // frame. Before the fenced frame results in a top navigation, this
  // `SharedStorageBudgetMetadata` will be stored/associated with the URN inside
  // the `FencedFrameURLMapping`.
  struct CONTENT_EXPORT SharedStorageBudgetMetadata {
    url::Origin origin;
    double budget_to_charge = 0;
  };

  // The runURLSelectionOperation's url mapping result. It contains the mapped
  // url and the `SharedStorageBudgetMetadata`.
  struct CONTENT_EXPORT SharedStorageURNMappingResult {
    GURL mapped_url;
    SharedStorageBudgetMetadata metadata;
  };

  // When the result of an ad auction is a main ad URL with a set of ad
  // component URLs (instead of just a single ad URL), a URN that maps to the
  // main ad URL needs to be loaded in a (parent) fenced frame, and then that
  // frame needs to have access to a new list of URNs, one for each ad component
  // URL, which it can then load in its own child fenced frames.
  //
  // To do this, the parent fenced frame needs two things, on commit:
  // 1) A list of URNs for the ad components, provided to the parent fenced
  //     frame via a Javascript API.
  // 2) Its URN to URL mapping needs to be updated to map those URNs to the ad
  //     component URLs returned by the auction.
  //
  // This class has functions that do both of these. GetURNs() returns the list
  // of URNs that need to be provided to the parent fenced frame so they are
  // accessible by the frame's scripts, and AddToMapping(), when invoked on the
  // parent fenced frame's FencedFrameURLMapping (not that of the frame that
  // actually ran the auction) adds those URNs and their corresponding URLs to
  // that mapping.
  class CONTENT_EXPORT PendingAdComponentsMap {
   public:
    PendingAdComponentsMap(PendingAdComponentsMap&) = delete;
    PendingAdComponentsMap(PendingAdComponentsMap&&);

    ~PendingAdComponentsMap();

    PendingAdComponentsMap& operator=(PendingAdComponentsMap&) = delete;
    PendingAdComponentsMap& operator=(PendingAdComponentsMap&&);

    // Returns the ordered list of URNs in this map.
    std::vector<GURL> GetURNs() const;

    // Exports URN to URL mappings to the passed in mapping. Generally only
    // called once per PendingAdComponentsMap, on the mapping associated with a
    // frame being navigated to a URN. Calling this twice with the
    // PendingAdComponentsMap on the same FencedFrameURLMapping will assert,
    // since it will result in adding the same URNs twice to the same mapping.
    void ExportToMapping(FencedFrameURLMapping& mapping) const;

   private:
    friend class FencedFrameURLMapping;

    // Contains an ad component URN and the URL it maps to.
    struct AdComponent {
      GURL urn;
      GURL url;
    };

    explicit PendingAdComponentsMap(const std::vector<GURL>& ad_component_urls);

    std::vector<AdComponent> component_ads_;
  };

  class MappingResultObserver {
   public:
    virtual ~MappingResultObserver() = default;

    // Called as soon as the URN mapping decision is made.
    //
    // On success, `mapped_url` will be populated with the result URL. If the
    // original URN is the result of an InterestGroup auction (which is only
    // possible when the initial URN is already mapped, and the
    // `OnFencedFrameURLMappingComplete` is invoked synchronously),
    // `pending_ad_components_map` will be populated with a
    // `PendingAdComponentsMap` and `ad_auction_data` will contain
    // AdAuctionData. In that case, the observer needs to use the
    // `PendingAdComponentsMap` to provide ad component URNs to the fenced frame
    // via its commit parameters, and to add those URNs to the
    // `FencedFrameURLMapping` of the committed frame.
    // `reporting_metadata` will contain a `ReportingMetadata` that populates
    // any metadata invoked by the worklet using `RegisterAdBeacon`. See
    // https://github.com/WICG/turtledove/blob/main/Fenced_Frames_Ads_Reporting.md#registeradbeacon
    //
    // On failure, both `mapped_url` and `pending_ad_components_map` will be
    // absl::nullopt.
    virtual void OnFencedFrameURLMappingComplete(
        absl::optional<GURL> mapped_url,
        absl::optional<AdAuctionData> ad_auction_data,
        absl::optional<PendingAdComponentsMap> pending_ad_components_map,
        ReportingMetadata& reporting_metadata) = 0;
  };

  FencedFrameURLMapping();
  ~FencedFrameURLMapping();
  FencedFrameURLMapping(FencedFrameURLMapping&) = delete;
  FencedFrameURLMapping& operator=(FencedFrameURLMapping&) = delete;

  // Adds a mapping for |url| to a URN:UUID that will be generated by this
  // function. Should only be invoked with a valid URL which is one of the
  // "potentially trustworthy URLs".
  // `reporting_metadata` will contain a `ReportingMetadata` that populates
  // any metadata invoked by the worklet using `RegisterAdBeacon`. See
  // https://github.com/WICG/turtledove/blob/main/Fenced_Frames_Ads_Reporting.md#registeradbeacon
  GURL AddFencedFrameURL(
      const GURL& url,
      const ReportingMetadata& reporting_metadata = ReportingMetadata());

  // Just like AddFencedFrameURL, but also takes additional ad auction data as
  // well as an ordered list of ad component URLs, as provided by a bidder
  // running an auction. These will to be made available to any fenced frame
  // navigated to the returned URN, via the InterestGroup API.
  //
  // See https://github.com/WICG/turtledove/blob/main/FLEDGE.md
  GURL AddFencedFrameURLWithInterestGroupInfo(
      const GURL& url,
      AdAuctionData auction_data,
      std::vector<GURL> ad_component_urls,
      const ReportingMetadata& reporting_metadata = ReportingMetadata());

  // Generate a URN that is not yet mapped to a URL. Used by the Shared Storage
  // API to return the URN for `sharedStorage.runURLSelectionOperation` before
  // the URL selection decision is made.
  GURL GeneratePendingMappedURN();

  // Register an observer for `urn_uuid`. The observer will be notified with the
  // mapping result and will be auto unregistered. If `urn_uuid` already exists
  // in `urn_uuid_to_url_map_`, or if it is not recognized at all, the observer
  // will be notified synchronously; if the mapping is pending (i.e. `urn_uuid`
  // exists in `pending_urn_uuid_to_url_map_`), the observer will be notified
  // asynchronously as soon as when the mapping decision is made.
  void ConvertFencedFrameURNToURL(const GURL& urn_uuid,
                                  MappingResultObserver* observer);

  // Explicitly unregister the observer for `urn_uuid`. This is only needed if
  // the observer is going to become invalid and the mapping is still pending.
  void RemoveObserverForURN(const GURL& urn_uuid,
                            MappingResultObserver* observer);

  // Called when the shared storage mapping decision is made for `urn_uuid`.
  // Should only be invoked on a `urn_uuid` pending to be mapped. This method
  // will trigger the observers' OnFencedFrameURLMappingComplete() method
  // associated with the `urn_uuid`, unregister those observers, and move the
  // `urn_uuid` from `pending_urn_uuid_to_url_map_` to `urn_uuid_to_url_map_`.
  void OnSharedStorageURNMappingResultDetermined(
      const GURL& urn_uuid,
      const SharedStorageURNMappingResult& mapping_result);

  // Get the `SharedStorageBudgetMetadata` associated with `urn_uuid`, and reset
  // the current metadata to absl::nullopt. Precondition: `urn_uuid` exists in
  // `urn_uuid_to_url_map_`.
  //
  // This method will be called when a fenced frame is navigating a top frame:
  // if the fenced frame originates from a URN generated from the shared
  // storage, then the shared storage origin's budget will be charged. For each
  // URN, we only need to charge the budget once, thus the value here is
  // released (i.e. returned and reset).
  absl::optional<SharedStorageBudgetMetadata>
  ReleaseSharedStorageBudgetMetadata(const GURL& urn_uuid);

  bool HasObserverForTesting(const GURL& urn_uuid,
                             MappingResultObserver* observer);

 private:
  // Contains the URL a particular URN is mapped to, along with any extra data
  // associated with the URL that needs to be used on commit.
  //
  // TODO(https://crbug.com/1260472): In order to only report FLEDGE results if
  // an ad is shown, InterestGroup reporting will also need to be wired up here.
  struct MapInfo {
    MapInfo();
    explicit MapInfo(const GURL& url);
    MapInfo(const GURL& url,
            const SharedStorageBudgetMetadata& shared_storage_budget_metadata);
    MapInfo(const MapInfo&);
    MapInfo(MapInfo&&);
    ~MapInfo();

    MapInfo& operator=(const MapInfo&);
    MapInfo& operator=(MapInfo&&);

    GURL mapped_url;

    // Extra data set if `mapped_url` is the result of a FLEDGE auction. Used
    // to fill in `AdAuctionDocumentData` for the fenced frame that navigates
    // to `mapped_url`.
    absl::optional<AdAuctionData> ad_auction_data;

    // Contains the metadata needed for shared storage budget charging. Will be
    // initialized to absl::nullopt if the associated URN is not generated from
    // shared storage; also will be reset to absl::nullopt if the budget has
    // already been charged for the associated URN.
    absl::optional<SharedStorageBudgetMetadata> shared_storage_budget_metadata;

    // Ad component URLs if `mapped_url` is the result of a FLEDGE auction. When
    // a fenced frame navigates to `mapped_url`, these will be mapped to URNs
    // themselves, and those URNs will be provided to the fenced frame.
    absl::optional<std::vector<GURL>> ad_component_urls;

    // If reporting events from fenced frames are registered, then this
    // information gets filled here.
    ReportingMetadata reporting_metadata;
  };

  using UrnUuidToUrlMap = std::map<GURL, MapInfo>;

  // Adds an entry to `urn_uuid_to_url_map_` for `url`, generating a unique URN
  // as the key.
  UrnUuidToUrlMap::iterator AddMappingForUrl(const GURL& url);

  bool IsMapped(const GURL& urn_uuid) const;
  bool IsPendingMapped(const GURL& urn_uuid) const;

  // The URNs that are already mapped to URLs, along with their mapping info.
  UrnUuidToUrlMap urn_uuid_to_url_map_;

  // The URNs that are not yet mapped to URLs, along with the associated
  // observers to be notified when the mapping decision is made.
  std::map<GURL, std::set<raw_ptr<MappingResultObserver>>>
      pending_urn_uuid_to_url_map_;
};

}  // namespace content

#endif  // CONTENT_BROWSER_FENCED_FRAME_FENCED_FRAME_URL_MAPPING_H_
