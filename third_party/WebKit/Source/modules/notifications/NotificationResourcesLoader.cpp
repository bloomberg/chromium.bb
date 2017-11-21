// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "modules/notifications/NotificationResourcesLoader.h"

#include <cmath>
#include "platform/Histogram.h"
#include "platform/weborigin/KURL.h"
#include "platform/wtf/Threading.h"
#include "platform/wtf/Time.h"
#include "public/platform/modules/notifications/WebNotificationData.h"
#include "public/platform/modules/notifications/WebNotificationResources.h"

namespace blink {

NotificationResourcesLoader::NotificationResourcesLoader(
    CompletionCallback completion_callback)
    : started_(false),
      completion_callback_(std::move(completion_callback)),
      pending_request_count_(0) {
  DCHECK(completion_callback_);
}

NotificationResourcesLoader::~NotificationResourcesLoader() {}

void NotificationResourcesLoader::Start(
    ExecutionContext* execution_context,
    const WebNotificationData& notification_data) {
  DCHECK(!started_);
  started_ = true;

  size_t num_actions = notification_data.actions.size();
  pending_request_count_ = 3 /* image, icon, badge */ + num_actions;

  // TODO(johnme): ensure image is not loaded when it will not be used.
  // TODO(mvanouwerkerk): ensure no badge is loaded when it will not be used.
  LoadImage(execution_context, NotificationImageLoader::Type::kImage,
            notification_data.image,
            WTF::Bind(&NotificationResourcesLoader::DidLoadImage,
                      WrapWeakPersistent(this)));
  LoadImage(execution_context, NotificationImageLoader::Type::kIcon,
            notification_data.icon,
            WTF::Bind(&NotificationResourcesLoader::DidLoadIcon,
                      WrapWeakPersistent(this)));
  LoadImage(execution_context, NotificationImageLoader::Type::kBadge,
            notification_data.badge,
            WTF::Bind(&NotificationResourcesLoader::DidLoadBadge,
                      WrapWeakPersistent(this)));

  action_icons_.resize(num_actions);
  for (size_t i = 0; i < num_actions; i++)
    LoadImage(execution_context, NotificationImageLoader::Type::kActionIcon,
              notification_data.actions[i].icon,
              WTF::Bind(&NotificationResourcesLoader::DidLoadActionIcon,
                        WrapWeakPersistent(this), i));
}

std::unique_ptr<WebNotificationResources>
NotificationResourcesLoader::GetResources() const {
  std::unique_ptr<WebNotificationResources> resources(
      new WebNotificationResources());
  resources->image = image_;
  resources->icon = icon_;
  resources->badge = badge_;
  resources->action_icons = action_icons_;
  return resources;
}

void NotificationResourcesLoader::Stop() {
  for (auto image_loader : image_loaders_)
    image_loader->Stop();
}

void NotificationResourcesLoader::Trace(blink::Visitor* visitor) {
  visitor->Trace(image_loaders_);
}

void NotificationResourcesLoader::LoadImage(
    ExecutionContext* execution_context,
    NotificationImageLoader::Type type,
    const KURL& url,
    NotificationImageLoader::ImageCallback image_callback) {
  if (url.IsNull() || url.IsEmpty() || !url.IsValid()) {
    DidFinishRequest();
    return;
  }

  NotificationImageLoader* image_loader = new NotificationImageLoader(type);
  image_loaders_.push_back(image_loader);
  image_loader->Start(execution_context, url, std::move(image_callback));
}

void NotificationResourcesLoader::DidLoadImage(const SkBitmap& image) {
  image_ = NotificationImageLoader::ScaleDownIfNeeded(
      image, NotificationImageLoader::Type::kImage);
  DidFinishRequest();
}

void NotificationResourcesLoader::DidLoadIcon(const SkBitmap& image) {
  icon_ = NotificationImageLoader::ScaleDownIfNeeded(
      image, NotificationImageLoader::Type::kIcon);
  DidFinishRequest();
}

void NotificationResourcesLoader::DidLoadBadge(const SkBitmap& image) {
  badge_ = NotificationImageLoader::ScaleDownIfNeeded(
      image, NotificationImageLoader::Type::kBadge);
  DidFinishRequest();
}

void NotificationResourcesLoader::DidLoadActionIcon(size_t action_index,
                                                    const SkBitmap& image) {
  DCHECK_LT(action_index, action_icons_.size());

  action_icons_[action_index] = NotificationImageLoader::ScaleDownIfNeeded(
      image, NotificationImageLoader::Type::kActionIcon);
  DidFinishRequest();
}

void NotificationResourcesLoader::DidFinishRequest() {
  DCHECK_GT(pending_request_count_, 0);
  pending_request_count_--;
  if (!pending_request_count_) {
    Stop();
    std::move(completion_callback_).Run(this);
    // The |this| pointer may have been deleted now.
  }
}

}  // namespace blink
