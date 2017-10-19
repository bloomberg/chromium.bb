// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MediaDownloadInProductHelpManager_h
#define MediaDownloadInProductHelpManager_h

#include <memory>

#include "modules/ModulesExport.h"
#include "platform/heap/GarbageCollected.h"
#include "platform/heap/Heap.h"
#include "platform/heap/Member.h"
#include "public/platform/media_download_in_product_help.mojom-blink.h"

namespace blink {
class MediaControlsImpl;

class MODULES_EXPORT MediaDownloadInProductHelpManager final
    : public GarbageCollectedFinalized<MediaDownloadInProductHelpManager> {
  WTF_MAKE_NONCOPYABLE(MediaDownloadInProductHelpManager);

 public:
  explicit MediaDownloadInProductHelpManager(MediaControlsImpl&);
  virtual ~MediaDownloadInProductHelpManager();

  void SetControlsVisibility(bool can_show);
  void SetDownloadButtonVisibility(bool can_show);
  void SetIsPlaying(bool is_playing);
  bool IsShowingInProductHelp() const;
  void UpdateInProductHelp();

  virtual void Trace(blink::Visitor*);

 private:
  void StateUpdated();
  bool CanShowInProductHelp() const;
  void MaybeDispatchDownloadInProductHelpTrigger(bool create);
  void DismissInProductHelp();

  Member<MediaControlsImpl> controls_;

  bool controls_can_show_ = false;
  bool button_can_show_ = false;
  bool is_playing_ = false;
  bool media_download_in_product_trigger_observed_ = false;
  IntRect download_button_rect_;

  mojom::blink::MediaDownloadInProductHelpPtr media_in_product_help_;
};

}  // namespace blink

#endif  // MediaDownloadInProductHelpManager_h
