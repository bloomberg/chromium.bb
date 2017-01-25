// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "core/loader/resource/MockImageResourceObserver.h"

#include "core/loader/resource/ImageResource.h"
#include "core/loader/resource/ImageResourceContent.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace blink {

MockImageResourceObserver::MockImageResourceObserver(
    ImageResourceContent* content)
    : m_content(content),
      m_imageChangedCount(0),
      m_imageWidthOnLastImageChanged(0),
      m_imageNotifyFinishedCount(0),
      m_imageWidthOnImageNotifyFinished(0) {
  m_content->addObserver(this);
}

MockImageResourceObserver::~MockImageResourceObserver() {
  removeAsObserver();
}

void MockImageResourceObserver::removeAsObserver() {
  if (!m_content)
    return;
  m_content->removeObserver(this);
  m_content = nullptr;
}

void MockImageResourceObserver::imageChanged(ImageResourceContent* image,
                                             const IntRect*) {
  m_imageChangedCount++;
  m_imageWidthOnLastImageChanged =
      m_content->hasImage() ? m_content->getImage()->width() : 0;
}

void MockImageResourceObserver::imageNotifyFinished(
    ImageResourceContent* image) {
  ASSERT_EQ(0, m_imageNotifyFinishedCount);
  m_imageNotifyFinishedCount++;
  m_imageWidthOnImageNotifyFinished =
      m_content->hasImage() ? m_content->getImage()->width() : 0;
  m_statusOnImageNotifyFinished = m_content->getStatus();
}

bool MockImageResourceObserver::imageNotifyFinishedCalled() const {
  DCHECK_LE(m_imageNotifyFinishedCount, 1);
  return m_imageNotifyFinishedCount;
}

}  // namespace blink
