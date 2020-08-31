// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_CAST_MEDIA_NOTIFICATION_ITEM_H_
#define CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_CAST_MEDIA_NOTIFICATION_ITEM_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/bitmap_fetcher/bitmap_fetcher.h"
#include "chrome/browser/ui/global_media_controls/cast_media_session_controller.h"
#include "chrome/common/media_router/media_route.h"
#include "chrome/common/media_router/mojom/media_status.mojom.h"
#include "components/media_message_center/media_notification_item.h"
#include "services/media_session/public/cpp/media_metadata.h"

class Profile;

namespace media_message_center {
class MediaNotificationController;
}  // namespace media_message_center

namespace network {
class SharedURLLoaderFactory;
}  // namespace network

// Represents the media notification shown in the Global Media Controls dialog
// for a Cast session. It is responsible for showing/hiding a
// MediaNotificationView.
class CastMediaNotificationItem
    : public media_message_center::MediaNotificationItem,
      public media_router::mojom::MediaStatusObserver {
 public:
  using BitmapFetcherFactory =
      base::RepeatingCallback<std::unique_ptr<BitmapFetcher>(
          const GURL&,
          BitmapFetcherDelegate*,
          const net::NetworkTrafficAnnotationTag&)>;

  CastMediaNotificationItem(
      const media_router::MediaRoute& route,
      media_message_center::MediaNotificationController*
          notification_controller,
      std::unique_ptr<CastMediaSessionController> session_controller,
      Profile* profile);
  CastMediaNotificationItem(const CastMediaNotificationItem&) = delete;
  CastMediaNotificationItem& operator=(const CastMediaNotificationItem&) =
      delete;
  ~CastMediaNotificationItem() override;

  // media_message_center::MediaNotificationItem:
  void SetView(media_message_center::MediaNotificationView* view) override;
  void OnMediaSessionActionButtonPressed(
      media_session::mojom::MediaSessionAction action) override;
  void Dismiss() override;

  // media_router::mojom::MediaStatusObserver:
  void OnMediaStatusUpdated(
      media_router::mojom::MediaStatusPtr status) override;

  void OnRouteUpdated(const media_router::MediaRoute& route);

  // Returns a pending remote bound to |this|. This should not be called more
  // than once per instance.
  mojo::PendingRemote<media_router::mojom::MediaStatusObserver>
  GetObserverPendingRemote();

  base::WeakPtr<CastMediaNotificationItem> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  void set_bitmap_fetcher_factory_for_testing_(BitmapFetcherFactory factory) {
    image_downloader_.set_bitmap_fetcher_factory_for_testing(
        std::move(factory));
  }

 private:
  class ImageDownloader : public BitmapFetcherDelegate {
   public:
    ImageDownloader(Profile* profile,
                    base::RepeatingCallback<void(const SkBitmap&)> callback);
    ImageDownloader(const ImageDownloader&) = delete;
    ImageDownloader& operator=(const ImageDownloader&) = delete;
    ~ImageDownloader() override;

    // BitmapFetcherDelegate:
    void OnFetchComplete(const GURL& url, const SkBitmap* bitmap) override;

    // Downloads the image specified by |url| and passes the result to
    // |callback_|. No-ops if |url| is the same as the previous call.
    void Download(const GURL& url);

    // Resets the bitmap fetcher, the saved image, and the image URL.
    void Reset();

    const SkBitmap& bitmap() const { return bitmap_; }

    void set_bitmap_fetcher_factory_for_testing(BitmapFetcherFactory factory) {
      bitmap_fetcher_factory_for_testing_ = std::move(factory);
    }

   private:
    scoped_refptr<network::SharedURLLoaderFactory> url_loader_factory_;
    base::RepeatingCallback<void(const SkBitmap&)> callback_;
    std::unique_ptr<BitmapFetcher> bitmap_fetcher_;
    GURL url_;
    // The downloaded bitmap.
    SkBitmap bitmap_;

    BitmapFetcherFactory bitmap_fetcher_factory_for_testing_;
  };

  void UpdateView();
  void ImageChanged(const SkBitmap& bitmap);
  void RecordMetadataMetrics() const;

  bool recorded_metadata_metrics_ = false;

  media_message_center::MediaNotificationController* const
      notification_controller_;
  media_message_center::MediaNotificationView* view_ = nullptr;

  std::unique_ptr<CastMediaSessionController> session_controller_;
  const media_router::MediaRoute::Id media_route_id_;
  ImageDownloader image_downloader_;
  media_session::MediaMetadata metadata_;
  std::vector<media_session::mojom::MediaSessionAction> actions_;
  media_session::mojom::MediaSessionInfoPtr session_info_;
  mojo::Receiver<media_router::mojom::MediaStatusObserver> observer_receiver_{
      this};
  base::WeakPtrFactory<CastMediaNotificationItem> weak_ptr_factory_{this};
};

#endif  // CHROME_BROWSER_UI_GLOBAL_MEDIA_CONTROLS_CAST_MEDIA_NOTIFICATION_ITEM_H_
