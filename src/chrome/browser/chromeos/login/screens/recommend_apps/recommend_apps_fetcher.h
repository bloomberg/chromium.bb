// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RECOMMEND_APPS_RECOMMEND_APPS_FETCHER_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RECOMMEND_APPS_RECOMMEND_APPS_FETCHER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/interfaces/cros_display_config.mojom.h"
#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/timer/timer.h"
#include "chrome/browser/chromeos/login/screens/recommend_apps/device_configuration.pb.h"
#include "chrome/browser/chromeos/login/screens/recommend_apps_screen_view.h"
#include "components/arc/arc_features_parser.h"
#include "extensions/browser/api/system_display/display_info_provider.h"

namespace network {
class SimpleURLLoader;
}

namespace chromeos {

// This class handles the network request for the Recommend Apps screen. It is
// supposed to run on the UI thread. The request requires the following headers:
// 1. X-Device-Config
// 2. X-Sdk-Version
// Play requires Android device config information to filter apps.
// device_configuration.proto is used to encode all the info. The following
// fields will be retrieved and sent:
// 1. touch_screen
// 2. keyboard
// 3. navigation
// 4. screen_layout
// 5. has_hard_keyboard
// 6. has_five_way_navigation
// 7. screen_density
// 8. screen_width
// 9. screen_height
// 10. gl_es_version
// 11. system_available_feature
// 12. native_platform
// 13. gl_extension
class RecommendAppsFetcher {
 public:
  explicit RecommendAppsFetcher(RecommendAppsScreenView* view);
  ~RecommendAppsFetcher();

  // Provide a retry method to download the app list again.
  void Retry();

 private:
  // Populate the required device config info.
  void PopulateDeviceConfig();

  // Start the connection to ash. Send the request to get display unit info
  // list.
  void StartAshRequest();

  // Start to compress and encode the proto message if we finish ash request
  // and ARC feature is read.
  void MaybeStartCompressAndEncodeProtoMessage();

  // Callback function called when display unit info list is retrieved from ash.
  // It will populate the device config info related to the screen density.
  void OnAshResponse(
      std::vector<ash::mojom::DisplayUnitInfoPtr> all_displays_info);

  // Callback function called when ARC features are read by the parser.
  // It will populate the device config info related to ARC features.
  void OnArcFeaturesRead(base::Optional<arc::ArcFeatures> read_result);

  // Callback function called when the proto message has been compressed and
  // encoded.
  void OnProtoMessageCompressedAndEncoded(
      std::string encoded_device_configuration_proto);

  // Start downloading the recommended app list.
  void StartDownload();

  // Abort the attempt to download the recommended app list if it takes too
  // long.
  void OnDownloadTimeout();

  // Callback function called when SimpleURLLoader completes.
  void OnDownloaded(std::unique_ptr<std::string> response_body);

  // If the response is not a valid JSON, return base::nullopt.
  // If the response contains no app, return base::nullopt;
  // Value output, in true, is a list containing:
  // 1. name: the title of the app.
  // 2. package_name
  // 3. Possibly an Icon URL.
  // Parses an input string that looks somewhat like this:
  // [{"title_" : {"name_" : {title of app"}},
  //   "id_" : {"id_" : {com.package.name"}},
  //  "icon_": {"url_": {"privateDoNotAccessOrElseSafeUrlWrappedValue_":
  //  "http://icon_url.com/url"}}},
  //  {"title_" : "title of second app",
  //   "packageName_": "second package name.",
  //  }]
  base::Optional<base::Value> ParseResponse(base::StringPiece response);

  device_configuration::DeviceConfigurationProto device_config_;

  std::string android_sdk_version_;

  std::string play_store_version_;

  std::string encoded_device_configuration_proto_;

  bool ash_ready_ = false;
  bool arc_features_ready_ = false;
  bool has_started_proto_processing_ = false;
  bool proto_compressed_and_encoded_ = false;

  RecommendAppsScreenView* view_;

  std::unique_ptr<network::SimpleURLLoader> app_list_loader_;

  // Timer that enforces a custom (shorter) timeout on the attempt to download
  // the recommended app list.
  base::OneShotTimer download_timer_;

  base::TimeTicks start_time_;

  ash::mojom::CrosDisplayConfigControllerPtr cros_display_config_;
  base::WeakPtrFactory<RecommendAppsFetcher> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(RecommendAppsFetcher);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_SCREENS_RECOMMEND_APPS_RECOMMEND_APPS_FETCHER_H_
