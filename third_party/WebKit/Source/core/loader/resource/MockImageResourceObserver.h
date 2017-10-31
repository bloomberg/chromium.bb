// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MockImageResourceObserver_h
#define MockImageResourceObserver_h

#include "core/loader/resource/ImageResource.h"
#include "core/loader/resource/ImageResourceContent.h"
#include "core/loader/resource/ImageResourceObserver.h"
#include "platform/loader/fetch/ResourceStatus.h"
#include <memory>

namespace blink {

class MockImageResourceObserver final : public ImageResourceObserver {
 public:
  static std::unique_ptr<MockImageResourceObserver> Create(
      ImageResourceContent* content) {
    return WTF::WrapUnique(new MockImageResourceObserver(content));
  }
  ~MockImageResourceObserver() override;

  void RemoveAsObserver();

  int ImageChangedCount() const { return image_changed_count_; }
  bool ImageNotifyFinishedCalled() const;

  int ImageWidthOnLastImageChanged() const {
    return image_width_on_last_image_changed_;
  }
  int ImageWidthOnImageNotifyFinished() const {
    return image_width_on_image_notify_finished_;
  }
  ResourceStatus StatusOnImageNotifyFinished() const {
    return status_on_image_notify_finished_;
  }

  CanDeferInvalidation Defer() const { return defer_; }

 private:
  explicit MockImageResourceObserver(ImageResourceContent*);

  // ImageResourceObserver overrides.
  void ImageNotifyFinished(ImageResourceContent*) override;
  void ImageChanged(ImageResourceContent*,
                    CanDeferInvalidation,
                    const IntRect*) override;
  String DebugName() const override { return "MockImageResourceObserver"; }

  Persistent<ImageResourceContent> content_;
  int image_changed_count_;
  CanDeferInvalidation defer_;
  int image_width_on_last_image_changed_;
  int image_notify_finished_count_;
  int image_width_on_image_notify_finished_;
  ResourceStatus status_on_image_notify_finished_ = ResourceStatus::kNotStarted;
};

}  // namespace blink

#endif  // MockImageResourceObserver_h
