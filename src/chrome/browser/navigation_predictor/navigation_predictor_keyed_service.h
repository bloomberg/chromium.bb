// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_KEYED_SERVICE_H_
#define CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_KEYED_SERVICE_H_

#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/single_thread_task_runner.h"
#include "chrome/browser/navigation_predictor/search_engine_preconnector.h"
#include "components/keyed_service/core/keyed_service.h"
#include "url/gurl.h"

namespace content {
class BrowserContext;
class WebContents;
}  // namespace content

// Keyed service that can be used to receive notifications about the URLs for
// the next predicted navigation.
class NavigationPredictorKeyedService : public KeyedService {
 public:
  // Indicates how the set of next navigation URLs were predicted.
  enum class PredictionSource {
    // Next navigation URLs were predicted by navigation predictor by parsing
    // the anchor element metrics on a webpage.
    kAnchorElementsParsedFromWebPage = 0,

    // Next navigation URLs were provided by an external Android app.
    kExternalAndroidApp = 1
  };

  // Stores the next set of URLs that the user is expected to navigate to.
  class Prediction {
   public:
    Prediction(const content::WebContents* web_contents,
               const base::Optional<GURL>& source_document_url,
               const base::Optional<std::vector<std::string>>&
                   external_app_packages_name,
               PredictionSource prediction_source,
               const std::vector<GURL>& sorted_predicted_urls);
    Prediction(const Prediction& other);
    Prediction& operator=(const Prediction& other);
    ~Prediction();
    const base::Optional<GURL>& source_document_url() const;
    const base::Optional<std::vector<std::string>>& external_app_packages_name()
        const;
    PredictionSource prediction_source() const { return prediction_source_; }
    const std::vector<GURL>& sorted_predicted_urls() const;

    // Null if the prediction source is kExternalAndroidApp.
    const content::WebContents* web_contents() const;

   private:
    // The WebContents from where the navigation may happen. Do not use this
    // pointer outside the observer's call stack unless its destruction is also
    // observed.
    const content::WebContents* web_contents_;

    // Current URL of the document from where the navigtion may happen.
    base::Optional<GURL> source_document_url_;

    // If the  |prediction_source_| is kExternalAndroidApp, then
    // |external_app_packages_name_| is the set of likely external Android apps
    // that generated the predictions. If the prediction source is
    // kExternalAndroidApp, then the external Android app that generated the
    // prediction is guaranteed to be one of the values in
    // |external_app_packages_name_|.
    base::Optional<std::vector<std::string>> external_app_packages_name_;

    // |prediction_source_| indicates how the prediction was generated and
    // affects how the prediction should be consumed. If the
    // |prediction_source_| is kAnchorElementsParsedFromWebPage, then
    // |source_document_url_| is the webpage from where the predictions were
    // generated.
    PredictionSource prediction_source_;

    // Ordered set of URLs that the user is expected to navigate to next. The
    // URLs are in the decreasing order of click probability.
    std::vector<GURL> sorted_predicted_urls_;
  };

  // Observer class can be implemented to receive notifications about the
  // predicted URLs for the next navigation. OnPredictionUpdated() is called
  // every time a new prediction is available. Prediction includes the source
  // document as well as the ordered list of URLs that the user may navigate to
  // next. OnPredictionUpdated() may be called multiple times for the same
  // source document URL.
  //
  // Observers must follow relevant privacy guidelines when consuming the
  // notifications.
  class Observer {
   public:
    virtual void OnPredictionUpdated(
        const base::Optional<Prediction> prediction) = 0;

   protected:
    Observer() {}
    virtual ~Observer() {}
  };

  explicit NavigationPredictorKeyedService(
      content::BrowserContext* browser_context);
  ~NavigationPredictorKeyedService() override;

  SearchEnginePreconnector* search_engine_preconnector();

  // |document_url| may be invalid. Called by navigation predictor.
  void OnPredictionUpdated(const content::WebContents* web_contents,
                           const GURL& document_url,
                           PredictionSource prediction_source,
                           const std::vector<GURL>& sorted_predicted_urls);

  // Notifies |this| of the next set of URLs that the user is expected to
  // navigate to. The set of URLs are reported by an external Android app.
  // The reporting app is guaranteed to be one of the apps reported in
  // |external_app_packages_name|. URLs are sorted in non-increasing order of
  // probability of navigation.
  void OnPredictionUpdatedByExternalAndroidApp(
      const std::vector<std::string>& external_app_packages_name,
      const std::vector<GURL>& sorted_predicted_urls);

  // Adds |observer| as the observer for next predicted navigation. When
  // |observer| is added via AddObserver, it's immediately notified of the last
  // known prediction. Observers must follow relevant privacy guidelines when
  // consuming the notifications.
  void AddObserver(Observer* observer);

  // Removes |observer| as the observer for next predicted navigation.
  void RemoveObserver(Observer* observer);

 private:
  // List of observers are currently registered to receive notifications for the
  // next predicted navigations.
  base::ObserverList<Observer>::Unchecked observer_list_;

  // Last known prediction.
  base::Optional<Prediction> last_prediction_;

  // Manages preconnecting to the user's default search engine.
  SearchEnginePreconnector search_engine_preconnector_;

  DISALLOW_COPY_AND_ASSIGN(NavigationPredictorKeyedService);
};

#endif  // CHROME_BROWSER_NAVIGATION_PREDICTOR_NAVIGATION_PREDICTOR_KEYED_SERVICE_H_
