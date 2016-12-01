// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/notifications/NotificationResourcesLoader.h"

#include "platform/Histogram.h"
#include "platform/weborigin/KURL.h"
#include "public/platform/modules/notifications/WebNotificationConstants.h"
#include "public/platform/modules/notifications/WebNotificationData.h"
#include "public/platform/modules/notifications/WebNotificationResources.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "wtf/CurrentTime.h"
#include "wtf/Threading.h"
#include <cmath>

namespace blink {

namespace {

// Scales down |image| to fit within |maxWidthPx|x|maxHeightPx| if it is larger
// and returns the result. Otherwise does nothing and returns |image| unchanged.
// TODO(mvanouwerkerk): Explore doing the scaling on a background thread.
SkBitmap scaleDownIfNeeded(const SkBitmap& image,
                           int maxWidthPx,
                           int maxHeightPx) {
  if (image.width() > maxWidthPx || image.height() > maxHeightPx) {
    double scale = std::min(static_cast<double>(maxWidthPx) / image.width(),
                            static_cast<double>(maxHeightPx) / image.height());
    DEFINE_THREAD_SAFE_STATIC_LOCAL(
        CustomCountHistogram, scaleTimeHistogram,
        new CustomCountHistogram("Notifications.Icon.ScaleDownTime", 1,
                                 1000 * 10 /* 10 seconds max */,
                                 50 /* buckets */));
    double startTime = monotonicallyIncreasingTimeMS();
    // TODO(peter): Try using RESIZE_BETTER for large images.
    SkBitmap scaledImage =
        skia::ImageOperations::Resize(image, skia::ImageOperations::RESIZE_BEST,
                                      std::lround(scale * image.width()),
                                      std::lround(scale * image.height()));
    scaleTimeHistogram.count(monotonicallyIncreasingTimeMS() - startTime);
    return scaledImage;
  }
  return image;
}

}  // namespace

NotificationResourcesLoader::NotificationResourcesLoader(
    std::unique_ptr<CompletionCallback> completionCallback)
    : m_started(false),
      m_completionCallback(std::move(completionCallback)),
      m_pendingRequestCount(0) {
  ThreadState::current()->registerPreFinalizer(this);
  DCHECK(m_completionCallback);
}

NotificationResourcesLoader::~NotificationResourcesLoader() {}

void NotificationResourcesLoader::start(
    ExecutionContext* executionContext,
    const WebNotificationData& notificationData) {
  DCHECK(!m_started);
  m_started = true;

  size_t numActions = notificationData.actions.size();
  m_pendingRequestCount = 3 /* image, icon, badge */ + numActions;

  // TODO(johnme): ensure image is not loaded when it will not be used.
  // TODO(mvanouwerkerk): ensure no badge is loaded when it will not be used.
  loadImage(executionContext, notificationData.image,
            WTF::bind(&NotificationResourcesLoader::didLoadImage,
                      wrapWeakPersistent(this)));
  loadImage(executionContext, notificationData.icon,
            WTF::bind(&NotificationResourcesLoader::didLoadIcon,
                      wrapWeakPersistent(this)));
  loadImage(executionContext, notificationData.badge,
            WTF::bind(&NotificationResourcesLoader::didLoadBadge,
                      wrapWeakPersistent(this)));

  m_actionIcons.resize(numActions);
  for (size_t i = 0; i < numActions; i++)
    loadImage(executionContext, notificationData.actions[i].icon,
              WTF::bind(&NotificationResourcesLoader::didLoadActionIcon,
                        wrapWeakPersistent(this), i));
}

std::unique_ptr<WebNotificationResources>
NotificationResourcesLoader::getResources() const {
  std::unique_ptr<WebNotificationResources> resources(
      new WebNotificationResources());
  resources->image = m_image;
  resources->icon = m_icon;
  resources->badge = m_badge;
  resources->actionIcons = m_actionIcons;
  return resources;
}

void NotificationResourcesLoader::stop() {
  for (auto imageLoader : m_imageLoaders)
    imageLoader->stop();
}

DEFINE_TRACE(NotificationResourcesLoader) {
  visitor->trace(m_imageLoaders);
}

void NotificationResourcesLoader::loadImage(
    ExecutionContext* executionContext,
    const KURL& url,
    std::unique_ptr<NotificationImageLoader::ImageCallback> imageCallback) {
  if (url.isNull() || url.isEmpty() || !url.isValid()) {
    didFinishRequest();
    return;
  }

  NotificationImageLoader* imageLoader = new NotificationImageLoader();
  m_imageLoaders.append(imageLoader);
  imageLoader->start(executionContext, url, std::move(imageCallback));
}

void NotificationResourcesLoader::didLoadImage(const SkBitmap& image) {
  m_image = scaleDownIfNeeded(image, kWebNotificationMaxImageWidthPx,
                              kWebNotificationMaxImageHeightPx);
  didFinishRequest();
}

void NotificationResourcesLoader::didLoadIcon(const SkBitmap& image) {
  m_icon = scaleDownIfNeeded(image, kWebNotificationMaxIconSizePx,
                             kWebNotificationMaxIconSizePx);
  didFinishRequest();
}

void NotificationResourcesLoader::didLoadBadge(const SkBitmap& image) {
  m_badge = scaleDownIfNeeded(image, kWebNotificationMaxBadgeSizePx,
                              kWebNotificationMaxBadgeSizePx);
  didFinishRequest();
}

void NotificationResourcesLoader::didLoadActionIcon(size_t actionIndex,
                                                    const SkBitmap& image) {
  DCHECK_LT(actionIndex, m_actionIcons.size());

  m_actionIcons[actionIndex] =
      scaleDownIfNeeded(image, kWebNotificationMaxActionIconSizePx,
                        kWebNotificationMaxActionIconSizePx);
  didFinishRequest();
}

void NotificationResourcesLoader::didFinishRequest() {
  DCHECK_GT(m_pendingRequestCount, 0);
  m_pendingRequestCount--;
  if (!m_pendingRequestCount) {
    stop();
    (*m_completionCallback)(this);
    // The |this| pointer may have been deleted now.
  }
}

}  // namespace blink
