/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "third_party/blink/public/platform/web_url_load_timing.h"

#include "third_party/blink/public/platform/web_string.h"
#include "third_party/blink/renderer/platform/loader/fetch/resource_load_timing.h"

namespace blink {

void WebURLLoadTiming::Initialize() {
  private_ = ResourceLoadTiming::Create();
}

void WebURLLoadTiming::Reset() {
  private_.Reset();
}

void WebURLLoadTiming::Assign(const WebURLLoadTiming& other) {
  private_ = other.private_;
}

double WebURLLoadTiming::RequestTime() const {
  return TimeTicksInSeconds(private_->RequestTime());
}

void WebURLLoadTiming::SetRequestTime(double time) {
  DCHECK_GE(time, 0.0);
  private_->SetRequestTime(TimeTicksFromSeconds(time));
}

double WebURLLoadTiming::ProxyStart() const {
  return TimeTicksInSeconds(private_->ProxyStart());
}

void WebURLLoadTiming::SetProxyStart(double start) {
  DCHECK_GE(start, 0.0);
  private_->SetProxyStart(TimeTicksFromSeconds(start));
}

double WebURLLoadTiming::ProxyEnd() const {
  return TimeTicksInSeconds(private_->ProxyEnd());
}

void WebURLLoadTiming::SetProxyEnd(double end) {
  DCHECK_GE(end, 0.0);
  private_->SetProxyEnd(TimeTicksFromSeconds(end));
}

double WebURLLoadTiming::DnsStart() const {
  return TimeTicksInSeconds(private_->DnsStart());
}

void WebURLLoadTiming::SetDNSStart(double start) {
  DCHECK_GE(start, 0.0);
  private_->SetDnsStart(TimeTicksFromSeconds(start));
}

double WebURLLoadTiming::DnsEnd() const {
  return TimeTicksInSeconds(private_->DnsEnd());
}

void WebURLLoadTiming::SetDNSEnd(double end) {
  DCHECK_GE(end, 0.0);
  private_->SetDnsEnd(TimeTicksFromSeconds(end));
}

double WebURLLoadTiming::ConnectStart() const {
  return TimeTicksInSeconds(private_->ConnectStart());
}

void WebURLLoadTiming::SetConnectStart(double start) {
  DCHECK_GE(start, 0.0);
  private_->SetConnectStart(TimeTicksFromSeconds(start));
}

double WebURLLoadTiming::ConnectEnd() const {
  return TimeTicksInSeconds(private_->ConnectEnd());
}

void WebURLLoadTiming::SetConnectEnd(double end) {
  DCHECK_GE(end, 0.0);
  private_->SetConnectEnd(TimeTicksFromSeconds(end));
}

double WebURLLoadTiming::WorkerStart() const {
  return TimeTicksInSeconds(private_->WorkerStart());
}

void WebURLLoadTiming::SetWorkerStart(double start) {
  DCHECK_GE(start, 0.0);
  private_->SetWorkerStart(TimeTicksFromSeconds(start));
}

double WebURLLoadTiming::WorkerReady() const {
  return TimeTicksInSeconds(private_->WorkerReady());
}

void WebURLLoadTiming::SetWorkerReady(double ready) {
  DCHECK_GE(ready, 0.0);
  private_->SetWorkerReady(TimeTicksFromSeconds(ready));
}

double WebURLLoadTiming::SendStart() const {
  return TimeTicksInSeconds(private_->SendStart());
}

void WebURLLoadTiming::SetSendStart(double start) {
  DCHECK_GE(start, 0.0);
  private_->SetSendStart(TimeTicksFromSeconds(start));
}

double WebURLLoadTiming::SendEnd() const {
  return TimeTicksInSeconds(private_->SendEnd());
}

void WebURLLoadTiming::SetSendEnd(double end) {
  DCHECK_GE(end, 0.0);
  private_->SetSendEnd(TimeTicksFromSeconds(end));
}

double WebURLLoadTiming::ReceiveHeadersEnd() const {
  return TimeTicksInSeconds(private_->ReceiveHeadersEnd());
}

void WebURLLoadTiming::SetReceiveHeadersEnd(double end) {
  DCHECK_GE(end, 0.0);
  private_->SetReceiveHeadersEnd(TimeTicksFromSeconds(end));
}

double WebURLLoadTiming::SslStart() const {
  return TimeTicksInSeconds(private_->SslStart());
}

void WebURLLoadTiming::SetSSLStart(double start) {
  DCHECK_GE(start, 0.0);
  private_->SetSslStart(TimeTicksFromSeconds(start));
}

double WebURLLoadTiming::SslEnd() const {
  return TimeTicksInSeconds(private_->SslEnd());
}

void WebURLLoadTiming::SetSSLEnd(double end) {
  DCHECK_GE(end, 0.0);
  private_->SetSslEnd(TimeTicksFromSeconds(end));
}

double WebURLLoadTiming::PushStart() const {
  return TimeTicksInSeconds(private_->PushStart());
}

void WebURLLoadTiming::SetPushStart(double start) {
  DCHECK_GE(start, 0.0);
  private_->SetPushStart(TimeTicksFromSeconds(start));
}

double WebURLLoadTiming::PushEnd() const {
  return TimeTicksInSeconds(private_->PushEnd());
}

void WebURLLoadTiming::SetPushEnd(double end) {
  DCHECK_GE(end, 0.0);
  private_->SetPushEnd(TimeTicksFromSeconds(end));
}

WebURLLoadTiming::WebURLLoadTiming(scoped_refptr<ResourceLoadTiming> value)
    : private_(std::move(value)) {}

WebURLLoadTiming& WebURLLoadTiming::operator=(
    scoped_refptr<ResourceLoadTiming> value) {
  private_ = std::move(value);
  return *this;
}

WebURLLoadTiming::operator scoped_refptr<ResourceLoadTiming>() const {
  return private_.Get();
}

}  // namespace blink
