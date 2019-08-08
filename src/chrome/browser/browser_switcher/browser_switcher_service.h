// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SERVICE_H_
#define CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SERVICE_H_

#include <memory>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "chrome/browser/browser_switcher/browser_switcher_prefs.h"
#include "components/keyed_service/core/keyed_service.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/simple_url_loader.h"
#include "url/gurl.h"

class Profile;

namespace browser_switcher {

class AlternativeBrowserDriver;
class BrowserSwitcherSitelist;
class ParsedXml;

// A definition of a source for an XML sitelist: a URL + what to do once it's
// downloaded.
struct RulesetSource {
  RulesetSource(std::string pref_name_,
                GURL url_,
                bool contains_inverted_rules,
                base::OnceCallback<void(ParsedXml xml)> parsed_callback_);
  RulesetSource(RulesetSource&&);
  ~RulesetSource();

  // Pref-name that should trigger a re-download when changed.
  std::string pref_name;
  // URL to download the ruleset from.
  GURL url;
  // If true, all the rules are inverted before being passed to the
  // callback. This is used for greylists.
  bool contains_inverted_rules;
  // What to do once the URL download + parsing is complete (or failed).
  base::OnceCallback<void(ParsedXml xml)> parsed_callback;

  std::unique_ptr<network::SimpleURLLoader> url_loader;
};

class BrowserSwitcherService;

class XmlDownloader {
 public:
  // Posts a task to start downloading+parsing the rulesets after 1
  // minute. Calls each source's callback once they're done (or failed). In
  // addition, calls |all_done_callback| once all the rulesets have been
  // processed.
  //
  // Schedules a refresh every 30 minutes, unless this object gets deleted
  // in the meantime.
  XmlDownloader(Profile* profile,
                BrowserSwitcherService* service,
                std::vector<RulesetSource> sources,
                base::RepeatingCallback<void()> all_done_callback);
  virtual ~XmlDownloader();

 private:
  // Returns true if any of the sources requires downloads. This is used to
  // avoid scheduling download tasks unnecessarily.
  bool HasValidSources() const;

  // Downloads the XML for every source, and calls ParseXml() for each source
  // once we have the response.
  void FetchXml();

  // Parses the XML for a source, and calls DoneParsing() on the UI thread when
  // done.
  void ParseXml(RulesetSource* source, std::unique_ptr<std::string> bytes);

  // Runs hooks on the source, and runs |all_done_callback| and
  // ScheduleRefresh() if this is the last source.
  void DoneParsing(RulesetSource* source, ParsedXml xml);

  // Schedules a call to Refresh() after |delay|.
  void ScheduleRefresh(base::TimeDelta delay);

  // Calls FetchXml() to refresh the sitelists.
  void Refresh();

  network::mojom::URLLoaderFactory* GetURLLoaderFactoryForURL(const GURL& url);

  std::unique_ptr<network::mojom::URLLoaderFactory> file_url_factory_;
  scoped_refptr<network::SharedURLLoaderFactory> other_url_factory_;

  // This |BrowserSwitcherService| owns this object.
  BrowserSwitcherService* service_;

  std::vector<RulesetSource> sources_;

  base::RepeatingCallback<void()> all_done_callback_;

  // Number of |RulesetSource|s that have finished processing. Used to
  // trigger the callback once they've all been parsed.
  unsigned int counter_ = 0;

  base::WeakPtrFactory<XmlDownloader> weak_ptr_factory_;
};

// Manages per-profile resources for BrowserSwitcher.
class BrowserSwitcherService : public KeyedService {
 public:
  explicit BrowserSwitcherService(Profile* profile);
  ~BrowserSwitcherService() override;

  void Init();

  // KeyedService:
  void Shutdown() override;

  AlternativeBrowserDriver* driver();
  BrowserSwitcherSitelist* sitelist();
  BrowserSwitcherPrefs& prefs();

  base::TimeDelta fetch_delay();
  base::TimeDelta refresh_delay();

  void SetDriverForTesting(std::unique_ptr<AlternativeBrowserDriver> driver);
  void SetSitelistForTesting(std::unique_ptr<BrowserSwitcherSitelist> sitelist);

  static void SetFetchDelayForTesting(base::TimeDelta delay);
  static void SetRefreshDelayForTesting(base::TimeDelta delay);

  // Return a platform-specific list of URLs to download, and what to do with
  // each of them once their XML has been parsed.
  virtual std::vector<RulesetSource> GetRulesetSources();

  // Loads the rules from prefs, for the 1 minute period before the download
  // happens.
  virtual void LoadRulesFromPrefs();

 protected:
  virtual void OnAllRulesetsParsed();
  virtual void OnBrowserSwitcherPrefsChanged(
      BrowserSwitcherPrefs* prefs,
      const std::vector<std::string>& changed_prefs);

  static base::TimeDelta fetch_delay_;
  static base::TimeDelta refresh_delay_;

 private:
  void StartDownload(std::vector<RulesetSource>&& sources);
  void OnExternalSitelistParsed(ParsedXml xml);
  void OnExternalGreylistParsed(ParsedXml xml);

  std::unique_ptr<XmlDownloader> sitelist_downloader_;

  Profile* profile_;
  BrowserSwitcherPrefs prefs_;
  std::unique_ptr<BrowserSwitcherPrefs::CallbackSubscription>
      prefs_subscription_;

  // Per-profile helpers.
  std::unique_ptr<AlternativeBrowserDriver> driver_;
  std::unique_ptr<BrowserSwitcherSitelist> sitelist_;

  base::WeakPtrFactory<BrowserSwitcherService> weak_ptr_factory_;

 private:
  DISALLOW_IMPLICIT_CONSTRUCTORS(BrowserSwitcherService);
};

}  // namespace browser_switcher

#endif  // CHROME_BROWSER_BROWSER_SWITCHER_BROWSER_SWITCHER_SERVICE_H_
