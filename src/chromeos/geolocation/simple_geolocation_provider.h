// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_GEOLOCATION_SIMPLE_GEOLOCATION_PROVIDER_H_
#define CHROMEOS_GEOLOCATION_SIMPLE_GEOLOCATION_PROVIDER_H_

#include <memory>
#include <vector>

#include "base/component_export.h"
#include "base/gtest_prod_util.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/threading/thread_checker.h"
#include "base/time/time.h"
#include "chromeos/geolocation/simple_geolocation_request.h"
#include "url/gurl.h"

namespace network {
class SharedURLLoaderFactory;
}

namespace chromeos {
class GeolocationHandler;

// This class implements Google Maps Geolocation API.
//
// SimpleGeolocationProvider must be created and used on the same thread.
//
// Note: this should probably be a singleton to monitor requests rate.
// But as it is used only diring ChromeOS Out-of-Box, it can be owned by
// WizardController for now.
class COMPONENT_EXPORT(CHROMEOS_GEOLOCATION) SimpleGeolocationProvider {
 public:
  SimpleGeolocationProvider(
      scoped_refptr<network::SharedURLLoaderFactory> factory,
      const GURL& url);
  virtual ~SimpleGeolocationProvider();

  // Initiates new request. If |send_wifi_access_points|, WiFi AP information
  // will be added to the request, similarly for |send_cell_towers| and Cell
  // Tower information. See SimpleGeolocationRequest for the description of
  // the other parameters.
  void RequestGeolocation(base::TimeDelta timeout,
                          bool send_wifi_access_points,
                          bool send_cell_towers,
                          SimpleGeolocationRequest::ResponseCallback callback);

  // Returns default geolocation service URL.
  static GURL DefaultGeolocationProviderURL();

 private:
  friend class TestGeolocationAPILoaderFactory;
  FRIEND_TEST_ALL_PREFIXES(SimpleGeolocationWirelessTest, CellularExists);
  FRIEND_TEST_ALL_PREFIXES(SimpleGeolocationWirelessTest, WiFiExists);

  // Geolocation response callback. Deletes request from requests_.
  void OnGeolocationResponse(
      SimpleGeolocationRequest* request,
      SimpleGeolocationRequest::ResponseCallback callback,
      const Geoposition& geoposition,
      bool server_error,
      const base::TimeDelta elapsed);

  void set_geolocation_handler(GeolocationHandler* geolocation_handler) {
    geolocation_handler_ = geolocation_handler;
  }

  scoped_refptr<network::SharedURLLoaderFactory> shared_url_loader_factory_;

  // URL of the Google Maps Geolocation API.
  const GURL url_;

  // Requests in progress.
  // SimpleGeolocationProvider owns all requests, so this vector is deleted on
  // destroy.
  std::vector<std::unique_ptr<SimpleGeolocationRequest>> requests_;

  GeolocationHandler* geolocation_handler_ = nullptr;

  // Creation and destruction should happen on the same thread.
  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(SimpleGeolocationProvider);
};

}  // namespace chromeos

#endif  // CHROMEOS_GEOLOCATION_SIMPLE_GEOLOCATION_PROVIDER_H_
