// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "remoting/host/resizing_host_observer.h"

#include <set>

#include "base/logging.h"
#include "remoting/host/desktop_resizer.h"
#include "remoting/host/host_status_monitor.h"

namespace remoting {

ResizingHostObserver::ResizingHostObserver(
    DesktopResizer* desktop_resizer,
    base::WeakPtr<HostStatusMonitor> monitor)
    : desktop_resizer_(desktop_resizer),
      monitor_(monitor),
      original_size_(SkISize::Make(0, 0)) {
  monitor_->AddStatusObserver(this);
}

ResizingHostObserver::~ResizingHostObserver() {
  if (monitor_)
    monitor_->RemoveStatusObserver(this);
}

void ResizingHostObserver::OnClientAuthenticated(const std::string& jid) {
  // This implementation assumes a single connected client, which is what the
  // host currently supports
  DCHECK(client_jid_.empty());
  original_size_ = desktop_resizer_->GetCurrentSize();
}

void ResizingHostObserver::OnClientDisconnected(const std::string& jid) {
  if (!original_size_.isZero()) {
    desktop_resizer_->RestoreSize(original_size_);
    original_size_.set(0, 0);
  }
  client_jid_.clear();
}

class CandidateSize {
 public:
  CandidateSize(const SkISize& candidate, const SkISize& preferred)
      : size_(candidate) {
    // Protect against division by zero.
    CHECK(!candidate.isEmpty());
    DCHECK(!preferred.isEmpty());

    // The client scale factor is the smaller of the candidate:preferred ratios
    // for width and height.
    if ((candidate.width() > preferred.width()) ||
        (candidate.height() > preferred.height())) {
      const float width_ratio =
          static_cast<float>(preferred.width()) / candidate.width();
      const float height_ratio =
          static_cast<float>(preferred.height()) / candidate.height();
      client_scale_factor_ = std::min(width_ratio, height_ratio);
    } else {
      // Since clients do not scale up, 1.0 is the maximum.
      client_scale_factor_ = 1.0;
    }

    // The aspect ratio "goodness" is defined as being the ratio of the smaller
    // of the two aspect ratios (candidate and preferred) to the larger. The
    // best aspect ratio is the one that most closely matches the preferred
    // aspect ratio (in other words, the ideal aspect ratio "goodness" is 1.0).
    // By keeping the values < 1.0, it allows ratios that differ in opposite
    // directions to be compared numerically.
    float candidate_aspect_ratio =
        static_cast<float>(candidate.width()) / candidate.height();
    float preferred_aspect_ratio =
        static_cast<float>(preferred.width()) / preferred.height();
    if (candidate_aspect_ratio > preferred_aspect_ratio) {
      aspect_ratio_goodness_ = preferred_aspect_ratio / candidate_aspect_ratio;
    } else {
      aspect_ratio_goodness_ = candidate_aspect_ratio / preferred_aspect_ratio;
    }
  }

  const SkISize& size() const { return size_; }
  float client_scale_factor() const { return client_scale_factor_; }
  float aspect_ratio_goodness() const { return aspect_ratio_goodness_; }
  int64 area() const {
    return static_cast<int64>(size_.width()) * size_.height();
  }

  bool IsBetterThan(const CandidateSize& other) const {
    // If either size would require down-scaling, prefer the one that down-
    // scales the least (since the client scale factor is at most 1.0, this
    // does not differentiate between sizes that don't require down-scaling).
    if (client_scale_factor() < other.client_scale_factor()) {
      return false;
    } else if (client_scale_factor() > other.client_scale_factor()) {
      return true;
    }

    // If the scale factors are the same, pick the size with the largest area.
    if (area() < other.area()) {
      return false;
    } else if (area() > other.area()) {
      return true;
    }

    // If the areas are equal, pick the size with the "best" aspect ratio.
    if (aspect_ratio_goodness() < other.aspect_ratio_goodness()) {
      return false;
    } else if (aspect_ratio_goodness() > other.aspect_ratio_goodness()) {
      return true;
    }

    // If the aspect ratios are equally good (for example, comparing 640x480
    // to 480x640 w.r.t. 640x640), just pick the widest, since desktop UIs
    // are typically designed for landscape aspect ratios.
    return size().width() > other.size().width();
  }

 private:
  float client_scale_factor_;
  float aspect_ratio_goodness_;
  SkISize size_;
};

void ResizingHostObserver::OnClientResolutionChanged(
    const std::string& jid,
    const SkISize& preferred_size,
    const SkIPoint& dpi) {
  if (preferred_size.isEmpty()) {
    return;
  }

  // If the implementation returns any sizes, pick the best one according to
  // the algorithm described in CandidateSize::IsBetterThen.
  std::list<SkISize> sizes =
      desktop_resizer_->GetSupportedSizes(preferred_size);
  if (sizes.empty()) {
    return;
  }
  CandidateSize best_size(sizes.front(), preferred_size);
  for (std::list<SkISize>::const_iterator i = ++sizes.begin();
       i != sizes.end(); ++i) {
    CandidateSize candidate_size(*i, preferred_size);
    if (candidate_size.IsBetterThan(best_size)) {
      best_size = candidate_size;
    }
  }
  SkISize current_size = desktop_resizer_->GetCurrentSize();
  if (best_size.size() != current_size)
    desktop_resizer_->SetSize(best_size.size());
}

}  // namespace remoting
