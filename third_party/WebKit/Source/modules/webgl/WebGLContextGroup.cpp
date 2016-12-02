/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "modules/webgl/WebGLContextGroup.h"

namespace blink {

WebGLContextGroup::WebGLContextGroup() : m_numberOfContextLosses(0) {}

gpu::gles2::GLES2Interface* WebGLContextGroup::getAGLInterface() {
  ASSERT(!m_contexts.isEmpty());
  // Weak processing removes dead entries from the HeapHashSet, so it's
  // guaranteed that this will not return null for the reason that a
  // WeakMember was nulled out.
  return (*m_contexts.begin())->contextGL();
}

void WebGLContextGroup::addContext(WebGLRenderingContextBase* context) {
  m_contexts.add(context);
}

void WebGLContextGroup::loseContextGroup(
    WebGLRenderingContextBase::LostContextMode mode,
    WebGLRenderingContextBase::AutoRecoveryMethod autoRecoveryMethod) {
  ++m_numberOfContextLosses;
  for (WebGLRenderingContextBase* const context : m_contexts)
    context->loseContextImpl(mode, autoRecoveryMethod);
}

uint32_t WebGLContextGroup::numberOfContextLosses() const {
  return m_numberOfContextLosses;
}

DEFINE_TRACE_WRAPPERS(WebGLContextGroup) {
  // TODO(kbr, mlippautz): need to use the manual write barrier since we
  // need to use weak members to get the desired semantics in Oilpan, but
  // no such TraceWrapperMember (like TraceWrapperWeakMember) exists yet.
  for (auto context : m_contexts) {
    visitor->traceWrappersWithManualWriteBarrier(context);
  }
}

}  // namespace blink
