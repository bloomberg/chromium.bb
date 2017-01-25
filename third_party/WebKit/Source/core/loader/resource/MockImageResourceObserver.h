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
  static std::unique_ptr<MockImageResourceObserver> create(
      ImageResourceContent* content) {
    return WTF::wrapUnique(new MockImageResourceObserver(content));
  }
  ~MockImageResourceObserver() override;

  void removeAsObserver();

  int imageChangedCount() const { return m_imageChangedCount; }
  bool imageNotifyFinishedCalled() const;

  int imageWidthOnLastImageChanged() const {
    return m_imageWidthOnLastImageChanged;
  }
  int imageWidthOnImageNotifyFinished() const {
    return m_imageWidthOnImageNotifyFinished;
  }
  ResourceStatus statusOnImageNotifyFinished() const {
    return m_statusOnImageNotifyFinished;
  }

 private:
  explicit MockImageResourceObserver(ImageResourceContent*);

  // ImageResourceObserver overrides.
  void imageNotifyFinished(ImageResourceContent*) override;
  void imageChanged(ImageResourceContent*, const IntRect*) override;
  String debugName() const override { return "MockImageResourceObserver"; }

  Persistent<ImageResourceContent> m_content;
  int m_imageChangedCount;
  int m_imageWidthOnLastImageChanged;
  int m_imageNotifyFinishedCount;
  int m_imageWidthOnImageNotifyFinished;
  ResourceStatus m_statusOnImageNotifyFinished = ResourceStatus::NotStarted;
};

}  // namespace blink

#endif  // MockImageResourceObserver_h
