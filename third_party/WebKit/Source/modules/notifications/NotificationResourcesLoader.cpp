// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/notifications/NotificationResourcesLoader.h"

#include "platform/weborigin/KURL.h"
#include "public/platform/modules/notifications/WebNotificationConstants.h"
#include "public/platform/modules/notifications/WebNotificationData.h"
#include "public/platform/modules/notifications/WebNotificationResources.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace blink {

namespace {

// Scales down |image| to fit within |maxSizePx| if its width or height is
// larger than |maxSizePx| and returns the result. Otherwise does nothing and
// returns |image| unchanged.
// TODO(mvanouwerkerk): Explore doing the scaling on a background thread.
SkBitmap scaleDownIfNeeded(const SkBitmap& image, int maxSizePx)
{
    if (image.width() > maxSizePx || image.height() > maxSizePx)
        return skia::ImageOperations::Resize(image, skia::ImageOperations::RESIZE_BEST, std::min(image.width(), maxSizePx), std::min(image.height(), maxSizePx));
    return image;
}

} // namespace

NotificationResourcesLoader::NotificationResourcesLoader(PassOwnPtr<CompletionCallback> completionCallback)
    : m_started(false), m_completionCallback(completionCallback), m_pendingRequestCount(0)
{
    ThreadState::current()->registerPreFinalizer(this);
    DCHECK(m_completionCallback);
}

NotificationResourcesLoader::~NotificationResourcesLoader()
{
}

void NotificationResourcesLoader::start(ExecutionContext* executionContext, const WebNotificationData& notificationData)
{
    DCHECK(!m_started);
    m_started = true;

    size_t numActions = notificationData.actions.size();
    m_pendingRequestCount = 2 /* icon and badge */ + numActions;

    loadImage(executionContext, notificationData.icon, bind<const SkBitmap&>(&NotificationResourcesLoader::didLoadIcon, WeakPersistentThisPointer<NotificationResourcesLoader>(this)));
    loadImage(executionContext, notificationData.badge, bind<const SkBitmap&>(&NotificationResourcesLoader::didLoadBadge, WeakPersistentThisPointer<NotificationResourcesLoader>(this)));

    m_actionIcons.resize(numActions);
    for (size_t i = 0; i < numActions; i++)
        loadImage(executionContext, notificationData.actions[i].icon, bind<const SkBitmap&>(&NotificationResourcesLoader::didLoadActionIcon, WeakPersistentThisPointer<NotificationResourcesLoader>(this), i));
}

std::unique_ptr<WebNotificationResources> NotificationResourcesLoader::getResources() const
{
    std::unique_ptr<WebNotificationResources> resources(new WebNotificationResources());
    resources->icon = m_icon;
    resources->badge = m_badge;
    resources->actionIcons = m_actionIcons;
    return resources;
}

void NotificationResourcesLoader::stop()
{
    for (auto imageLoader : m_imageLoaders)
        imageLoader->stop();
}

DEFINE_TRACE(NotificationResourcesLoader)
{
    visitor->trace(m_imageLoaders);
}

void NotificationResourcesLoader::loadImage(ExecutionContext* executionContext, const KURL& url, PassOwnPtr<NotificationImageLoader::ImageCallback> imageCallback)
{
    if (url.isNull() || url.isEmpty() || !url.isValid()) {
        didFinishRequest();
        return;
    }

    NotificationImageLoader* imageLoader = new NotificationImageLoader();
    m_imageLoaders.append(imageLoader);
    imageLoader->start(executionContext, url, imageCallback);
}

void NotificationResourcesLoader::didLoadIcon(const SkBitmap& image)
{
    m_icon = scaleDownIfNeeded(image, kWebNotificationMaxIconSizePx);
    didFinishRequest();
}

void NotificationResourcesLoader::didLoadBadge(const SkBitmap& image)
{
    m_badge = scaleDownIfNeeded(image, kWebNotificationMaxBadgeSizePx);
    didFinishRequest();
}

void NotificationResourcesLoader::didLoadActionIcon(size_t actionIndex, const SkBitmap& image)
{
    DCHECK_LT(actionIndex, m_actionIcons.size());

    m_actionIcons[actionIndex] = scaleDownIfNeeded(image, kWebNotificationMaxActionIconSizePx);
    didFinishRequest();
}

void NotificationResourcesLoader::didFinishRequest()
{
    DCHECK_GT(m_pendingRequestCount, 0);
    m_pendingRequestCount--;
    if (!m_pendingRequestCount) {
        stop();
        (*m_completionCallback)(this);
        // The |this| pointer may have been deleted now.
    }
}

} // namespace blink
