// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MockImageResourceClient_h
#define MockImageResourceClient_h

#include "core/fetch/MockResourceClient.h"
#include "core/loader/resource/ImageResource.h"
#include "core/loader/resource/ImageResourceContent.h"
#include "core/loader/resource/ImageResourceObserver.h"

namespace blink {

class MockImageResourceClient final : public MockResourceClient,
                                      public ImageResourceObserver {
 public:
  explicit MockImageResourceClient(ImageResource*);
  ~MockImageResourceClient() override;

  void imageNotifyFinished(ImageResourceContent*) override;
  void imageChanged(ImageResourceContent*, const IntRect*) override;

  String debugName() const override { return "MockImageResourceClient"; }

  bool notifyFinishedCalled() const override;

  void removeAsClient() override;
  void dispose() override;

  int imageChangedCount() const { return m_imageChangedCount; }
  int imageNotifyFinishedCount() const { return m_imageNotifyFinishedCount; }

  size_t encodedSizeOnLastImageChanged() const {
    return m_encodedSizeOnLastImageChanged;
  }
  size_t encodedSizeOnImageNotifyFinished() const {
    return m_encodedSizeOnImageNotifyFinished;
  }

 private:
  int m_imageChangedCount;
  size_t m_encodedSizeOnLastImageChanged;
  int m_imageNotifyFinishedCount;
  size_t m_encodedSizeOnImageNotifyFinished;
};

}  // namespace blink

#endif  // MockImageResourceClient_h
