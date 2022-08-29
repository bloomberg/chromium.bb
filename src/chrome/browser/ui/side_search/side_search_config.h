// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_CONFIG_H_
#define CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_CONFIG_H_

#include "base/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/observer_list_types.h"
#include "base/scoped_observation.h"
#include "base/supports_user_data.h"
#include "components/search_engines/template_url_service.h"
#include "components/search_engines/template_url_service_observer.h"

namespace content {
class BrowserContext;
}  // namespace content

class GURL;
class Profile;

// Stores per-profile configuration data for side search.
class SideSearchConfig : public base::SupportsUserData::Data,
                         public TemplateURLServiceObserver {
 public:
  using URLTestConditionCallback = base::RepeatingCallback<bool(const GURL&)>;
  using GenerateURLCallback = base::RepeatingCallback<GURL(const GURL&)>;

  // Config clients subclass this to be notified to changes in side search
  // config state.
  class Observer : public base::CheckedObserver {
   public:
    // Called after a change to the config's state.
    virtual void OnSideSearchConfigChanged() = 0;
  };

  explicit SideSearchConfig(Profile* profile);
  SideSearchConfig(const SideSearchConfig&) = delete;
  SideSearchConfig& operator=(const SideSearchConfig&) = delete;
  ~SideSearchConfig() override;

  // TemplateURLServiceObserver:
  void OnTemplateURLServiceChanged() override;
  void OnTemplateURLServiceShuttingDown() override;

  // Gets the instance of the config for `context`.
  static SideSearchConfig* Get(content::BrowserContext* context);

  // Returns whether a `url` in the side panel should be allowed to commit in
  // the side panel or if it should be redirected to the content frame.
  bool ShouldNavigateInSidePanel(const GURL& url);
  void SetShouldNavigateInSidePanelCallback(URLTestConditionCallback callback);

  // Returns whether the side panel can be shown for the `url`. This is used to
  // avoid having the side panel on pages on which it doesn't make sense to have
  // it appear (e.g. NTP).
  bool CanShowSidePanelForURL(const GURL& url);
  void SetCanShowSidePanelForURLCallback(URLTestConditionCallback callback);

  // Generates a side search URL for use in the side panel. The provided search
  // url must be a search page belonging to the default search engine.
  GURL GenerateSideSearchURL(const GURL& search_url);
  void SetGenerateSideSearchURLCallback(GenerateURLCallback callback);

  // Gets and sets the bit that determines whether or not the SRP is available.
  // TODO(tluk): Move the code that tests for availability into this class.
  bool is_side_panel_srp_available() { return is_side_panel_srp_available_; }
  void set_is_side_panel_srp_available(bool is_side_panel_srp_available) {
    is_side_panel_srp_available_ = is_side_panel_srp_available;
  }

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Resets any local config state and notifies observers when the configuration
  // changes.
  void ResetStateAndNotifyConfigChanged();

  // Called when the page action label is shown.
  void DidShowPageActionLabel();
  int page_action_label_shown_count() const {
    return page_action_label_shown_count_;
  }

  // TODO(crbug.com/1304513): Allow tests to specify the Google Search
  // configuration on all supported platforms until tests are fully migrated.
  void ApplyGoogleSearchConfigurationForTesting();

 private:
  // Whether or not the service providing the SRP for the side panel is
  // available or not.
  bool is_side_panel_srp_available_ = false;

  // Tracks the number of times the page action icon has animated-in its label
  // text for the config's associated profile.
  int page_action_label_shown_count_ = 0;

  raw_ptr<Profile> const profile_;

  base::ObserverList<Observer> observers_;

  URLTestConditionCallback should_navigate_in_side_panel_callback_;
  URLTestConditionCallback can_show_side_panel_for_url_callback_;
  GenerateURLCallback generate_side_search_url_callack_;

  // The ID of the current default TemplateURL instance. Keep track of this so
  // we update the page action's favicon only when the default instance changes.
  TemplateURLID default_template_url_id_ = kInvalidTemplateURLID;

  base::ScopedObservation<TemplateURLService, TemplateURLServiceObserver>
      template_url_service_observation_{this};
};

#endif  // CHROME_BROWSER_UI_SIDE_SEARCH_SIDE_SEARCH_CONFIG_H_
