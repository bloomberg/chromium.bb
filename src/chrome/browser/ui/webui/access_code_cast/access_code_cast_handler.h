// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_ACCESS_CODE_CAST_ACCESS_CODE_CAST_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_ACCESS_CODE_CAST_ACCESS_CODE_CAST_HANDLER_H_

#include "base/scoped_observation.h"
#include "chrome/browser/media/router/discovery/access_code/access_code_cast_discovery_interface.h"
#include "chrome/browser/media/router/discovery/access_code/discovery_resources.pb.h"
#include "chrome/browser/media/router/discovery/mdns/cast_media_sink_service_impl.h"
#include "chrome/browser/media/router/discovery/mdns/media_sink_util.h"
#include "chrome/browser/ui/app_list/search/search_controller.h"
#include "chrome/browser/ui/media_router/media_cast_mode.h"
#include "chrome/browser/ui/webui/access_code_cast/access_code_cast.mojom.h"
#include "components/media_router/common/discovery/media_sink_internal.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"

using ::access_code_cast::mojom::AddSinkResultCode;
using ::media_router::AccessCodeCastDiscoveryInterface;
using ::media_router::CreateCastMediaSinkResult;
using ::media_router::MediaSinkInternal;

namespace content {
class WebContents;
}

namespace media_router {
class MediaRouter;
}

// TODO(b/213324920): Remove WebUI from the media_router namespace after
// expiration module has been completed.
namespace media_router {

class AccessCodeCastHandler : public access_code_cast::mojom::PageHandler {
 public:
  using DiscoveryDevice = chrome_browser_media::proto::DiscoveryDevice;

  AccessCodeCastHandler(
      mojo::PendingReceiver<access_code_cast::mojom::PageHandler> page_handler,
      mojo::PendingRemote<access_code_cast::mojom::Page> page,
      Profile* profile,
      media_router::MediaRouter* media_router,
      const media_router::CastModeSet& cast_mode_set,
      content::WebContents* web_contents);

  // Constructor that is used for testing.
  AccessCodeCastHandler(
      mojo::PendingReceiver<access_code_cast::mojom::PageHandler> page_handler,
      mojo::PendingRemote<access_code_cast::mojom::Page> page,
      Profile* profile,
      media_router::MediaRouter* media_router,
      const media_router::CastModeSet& cast_mode_set,
      content::WebContents* web_contents,
      CastMediaSinkServiceImpl* cast_media_sink_service_impl);

  ~AccessCodeCastHandler() override;

  // access_code_cast::mojom::PageHandler overrides:
  void AddSink(const std::string& access_code,
               access_code_cast::mojom::CastDiscoveryMethod discovery_method,
               AddSinkCallback callback) override;

  // access_code_cast::mojom::PageHandler overrides:
  void CastToSink(CastToSinkCallback callback) override;

  void SetSinkCallbackForTesting(AddSinkCallback callback);

 private:
  friend class AccessCodeCastHandlerTest;
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastHandlerTest,
                           DiscoveryDeviceMissingWithOk);
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastHandlerTest,
                           ValidDiscoveryDeviceAndCode);
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastHandlerTest, InvalidDiscoveryDevice);
  FRIEND_TEST_ALL_PREFIXES(AccessCodeCastHandlerTest, NonOKResultCode);

  void AddSinkToMediaRouter(MediaSinkInternal media_sink);

  void HandleSinkPresentInMediaRouter(MediaSinkInternal media_sink,
                                      bool has_sink);

  void OnAccessCodeValidated(
      absl::optional<DiscoveryDevice> discovery_device,
      access_code_cast::mojom::AddSinkResultCode result_code);

  mojo::Remote<access_code_cast::mojom::Page> page_;
  mojo::Receiver<access_code_cast::mojom::PageHandler> receiver_;

  std::unique_ptr<AccessCodeCastDiscoveryInterface> discovery_server_interface_;

  // The dispatcher only needs to cast to the most recent sink that was
  // added. Store this value after the call to add is made.
  const std::string recent_sink_id;

  // Used to fetch OAuth2 access tokens.
  raw_ptr<Profile> const profile_;

  const raw_ptr<media_router::MediaRouter> media_router_;
  const media_router::CastModeSet cast_mode_set_;
  const raw_ptr<content::WebContents> web_contents_;

  AddSinkCallback add_sink_callback_;

  raw_ptr<CastMediaSinkServiceImpl> const cast_media_sink_service_impl_;

  base::WeakPtrFactory<AccessCodeCastHandler> weak_ptr_factory_{this};
};

}  // namespace media_router

#endif  // CHROME_BROWSER_UI_WEBUI_ACCESS_CODE_CAST_ACCESS_CODE_CAST_HANDLER_H_
