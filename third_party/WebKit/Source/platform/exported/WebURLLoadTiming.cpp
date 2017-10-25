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

#include "public/platform/WebURLLoadTiming.h"

#include "platform/loader/fetch/ResourceLoadTiming.h"
#include "public/platform/WebString.h"

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
  return private_->RequestTime();
}

void WebURLLoadTiming::SetRequestTime(double time) {
  private_->SetRequestTime(time);
}

double WebURLLoadTiming::ProxyStart() const {
  return private_->ProxyStart();
}

void WebURLLoadTiming::SetProxyStart(double start) {
  private_->SetProxyStart(start);
}

double WebURLLoadTiming::ProxyEnd() const {
  return private_->ProxyEnd();
}

void WebURLLoadTiming::SetProxyEnd(double end) {
  private_->SetProxyEnd(end);
}

double WebURLLoadTiming::DnsStart() const {
  return private_->DnsStart();
}

void WebURLLoadTiming::SetDNSStart(double start) {
  private_->SetDnsStart(start);
}

double WebURLLoadTiming::DnsEnd() const {
  return private_->DnsEnd();
}

void WebURLLoadTiming::SetDNSEnd(double end) {
  private_->SetDnsEnd(end);
}

double WebURLLoadTiming::ConnectStart() const {
  return private_->ConnectStart();
}

void WebURLLoadTiming::SetConnectStart(double start) {
  private_->SetConnectStart(start);
}

double WebURLLoadTiming::ConnectEnd() const {
  return private_->ConnectEnd();
}

void WebURLLoadTiming::SetConnectEnd(double end) {
  private_->SetConnectEnd(end);
}

double WebURLLoadTiming::WorkerStart() const {
  return private_->WorkerStart();
}

void WebURLLoadTiming::SetWorkerStart(double start) {
  private_->SetWorkerStart(start);
}

double WebURLLoadTiming::WorkerReady() const {
  return private_->WorkerReady();
}

void WebURLLoadTiming::SetWorkerReady(double ready) {
  private_->SetWorkerReady(ready);
}

double WebURLLoadTiming::SendStart() const {
  return private_->SendStart();
}

void WebURLLoadTiming::SetSendStart(double start) {
  private_->SetSendStart(start);
}

double WebURLLoadTiming::SendEnd() const {
  return private_->SendEnd();
}

void WebURLLoadTiming::SetSendEnd(double end) {
  private_->SetSendEnd(end);
}

double WebURLLoadTiming::ReceiveHeadersEnd() const {
  return private_->ReceiveHeadersEnd();
}

void WebURLLoadTiming::SetReceiveHeadersEnd(double end) {
  private_->SetReceiveHeadersEnd(end);
}

double WebURLLoadTiming::SslStart() const {
  return private_->SslStart();
}

void WebURLLoadTiming::SetSSLStart(double start) {
  private_->SetSslStart(start);
}

double WebURLLoadTiming::SslEnd() const {
  return private_->SslEnd();
}

void WebURLLoadTiming::SetSSLEnd(double end) {
  private_->SetSslEnd(end);
}

double WebURLLoadTiming::PushStart() const {
  return private_->PushStart();
}

void WebURLLoadTiming::SetPushStart(double start) {
  private_->SetPushStart(start);
}

double WebURLLoadTiming::PushEnd() const {
  return private_->PushEnd();
}

void WebURLLoadTiming::SetPushEnd(double end) {
  private_->SetPushEnd(end);
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
