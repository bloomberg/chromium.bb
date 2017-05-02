// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "platform/graphics/gpu/SharedContextRateLimiter.h"

#include <memory>
#include "gpu/GLES2/gl2extchromium.h"
#include "platform/graphics/gpu/Extensions3DUtil.h"
#include "platform/wtf/PtrUtil.h"
#include "public/platform/Platform.h"
#include "public/platform/WebGraphicsContext3DProvider.h"
#include "third_party/khronos/GLES2/gl2.h"

namespace blink {

std::unique_ptr<SharedContextRateLimiter> SharedContextRateLimiter::Create(
    unsigned max_pending_ticks) {
  return WTF::WrapUnique(new SharedContextRateLimiter(max_pending_ticks));
}

SharedContextRateLimiter::SharedContextRateLimiter(unsigned max_pending_ticks)
    : max_pending_ticks_(max_pending_ticks), can_use_sync_queries_(false) {
  context_provider_ =
      Platform::Current()->CreateSharedOffscreenGraphicsContext3DProvider();
  if (!context_provider_)
    return;

  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
  if (gl && gl->GetGraphicsResetStatusKHR() == GL_NO_ERROR) {
    std::unique_ptr<Extensions3DUtil> extensions_util =
        Extensions3DUtil::Create(gl);
    // TODO(junov): when the GLES 3.0 command buffer is ready, we could use
    // fenceSync instead.
    can_use_sync_queries_ =
        extensions_util->SupportsExtension("GL_CHROMIUM_sync_query");
  }
}

void SharedContextRateLimiter::Tick() {
  if (!context_provider_)
    return;

  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
  if (!gl || gl->GetGraphicsResetStatusKHR() != GL_NO_ERROR)
    return;

  queries_.push_back(0);
  if (can_use_sync_queries_)
    gl->GenQueriesEXT(1, &queries_.back());
  if (can_use_sync_queries_) {
    gl->BeginQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM, queries_.back());
    gl->EndQueryEXT(GL_COMMANDS_COMPLETED_CHROMIUM);
  }
  if (queries_.size() > max_pending_ticks_) {
    if (can_use_sync_queries_) {
      GLuint result;
      gl->GetQueryObjectuivEXT(queries_.front(), GL_QUERY_RESULT_EXT, &result);
      gl->DeleteQueriesEXT(1, &queries_.front());
      queries_.pop_front();
    } else {
      gl->Finish();
      Reset();
    }
  }
}

void SharedContextRateLimiter::Reset() {
  if (!context_provider_)
    return;

  gpu::gles2::GLES2Interface* gl = context_provider_->ContextGL();
  if (gl && gl->GetGraphicsResetStatusKHR() == GL_NO_ERROR) {
    while (queries_.size() > 0) {
      gl->DeleteQueriesEXT(1, &queries_.front());
      queries_.pop_front();
    }
  } else {
    queries_.clear();
  }
}

}  // namespace blink
