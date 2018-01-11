/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 * Copyright (C) 2012 Motorola Mobility Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "core/url/DOMURL.h"

#include "bindings/core/v8/ExceptionMessages.h"
#include "bindings/core/v8/ExceptionState.h"
#include "core/dom/ExceptionCode.h"
#include "core/dom/ExecutionContext.h"
#include "core/html/PublicURLManager.h"
#include "core/url/URLSearchParams.h"
#include "platform/loader/fetch/MemoryCache.h"
#include "platform/wtf/AutoReset.h"

namespace blink {

DOMURL::DOMURL(const String& url,
               const KURL& base,
               ExceptionState& exception_state) {
  if (!base.IsValid()) {
    exception_state.ThrowTypeError("Invalid base URL");
    return;
  }

  url_ = KURL(base, url);
  if (!url_.IsValid())
    exception_state.ThrowTypeError("Invalid URL");
}

DOMURL::~DOMURL() = default;

void DOMURL::Trace(blink::Visitor* visitor) {
  visitor->Trace(search_params_);
  ScriptWrappable::Trace(visitor);
}

void DOMURL::SetInput(const String& value) {
  KURL url(BlankURL(), value);
  if (url.IsValid()) {
    url_ = url;
    input_ = String();
  } else {
    url_ = KURL();
    input_ = value;
  }
  Update();
}

void DOMURL::setSearch(const String& value) {
  DOMURLUtils::setSearch(value);
  if (!value.IsEmpty() && value[0] == '?')
    UpdateSearchParams(value.Substring(1));
  else
    UpdateSearchParams(value);
}

String DOMURL::CreatePublicURL(ExecutionContext* execution_context,
                               URLRegistrable* registrable) {
  return execution_context->GetPublicURLManager().RegisterURL(registrable);
}

URLSearchParams* DOMURL::searchParams() {
  if (!search_params_)
    search_params_ = URLSearchParams::Create(Url().Query(), this);

  return search_params_;
}

void DOMURL::Update() {
  UpdateSearchParams(Url().Query());
}

void DOMURL::UpdateSearchParams(const String& query_string) {
  if (!search_params_)
    return;

  AutoReset<bool> scope(&is_in_update_, true);
#if DCHECK_IS_ON()
  DCHECK_EQ(search_params_->UrlObject(), this);
#endif
  search_params_->SetInputWithoutUpdate(query_string);
}

}  // namespace blink
