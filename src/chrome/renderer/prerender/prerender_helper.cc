// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/prerender/prerender_helper.h"

#include "base/metrics/field_trial.h"
#include "base/metrics/histogram.h"
#include "chrome/common/prerender_messages.h"
#include "chrome/common/prerender_url_loader_throttle.h"
#include "content/public/renderer/document_state.h"
#include "content/public/renderer/render_frame.h"
#include "content/public/renderer/render_view.h"
#include "third_party/blink/public/web/web_frame.h"
#include "third_party/blink/public/web/web_view.h"

namespace prerender {

PrerenderHelper::PrerenderHelper(content::RenderFrame* render_frame,
                                 PrerenderMode prerender_mode,
                                 const std::string& histogram_prefix)
    : content::RenderFrameObserver(render_frame),
      content::RenderFrameObserverTracker<PrerenderHelper>(render_frame),
      prerender_mode_(prerender_mode),
      histogram_prefix_(histogram_prefix) {
  DCHECK_NE(prerender_mode_, NO_PRERENDER);
}

PrerenderHelper::~PrerenderHelper() = default;

void PrerenderHelper::AddThrottle(
    const base::WeakPtr<PrerenderURLLoaderThrottle>& throttle) {
  throttles_.push_back(throttle);
}

// static.
bool PrerenderHelper::IsPrerendering(const content::RenderFrame* render_frame) {
  return PrerenderHelper::GetPrerenderMode(render_frame) != NO_PRERENDER;
}

// static.
PrerenderMode PrerenderHelper::GetPrerenderMode(
    const content::RenderFrame* render_frame) {
  PrerenderHelper* helper = PrerenderHelper::Get(render_frame);
  if (!helper)
    return NO_PRERENDER;

  DCHECK_NE(helper->prerender_mode_, NO_PRERENDER);
  return helper->prerender_mode_;
}

bool PrerenderHelper::OnMessageReceived(
    const IPC::Message& message) {
  IPC_BEGIN_MESSAGE_MAP(PrerenderHelper, message)
    IPC_MESSAGE_HANDLER(PrerenderMsg_SetIsPrerendering, OnSetIsPrerendering)
  IPC_END_MESSAGE_MAP()
  // Return false on PrerenderMsg_SetIsPrerendering so other observers can see
  // the message.
  return false;
}

void PrerenderHelper::OnSetIsPrerendering(PrerenderMode mode,
                                          const std::string& histogram_prefix) {
  // Immediately after construction, |this| may receive the message that
  // triggered its creation.  If so, ignore it.
  if (mode != prerender::NO_PRERENDER)
    return;

  auto throttles = std::move(throttles_);

  // |this| must be deleted so PrerenderHelper::IsPrerendering returns false
  // when the visibility state is updated, so the visibility state string will
  // not be "prerendered".
  delete this;

  for (auto resource : throttles) {
    if (resource)
      resource->PrerenderUsed();
  }
}

void PrerenderHelper::OnDestruct() {
  delete this;
}

}  // namespace prerender
