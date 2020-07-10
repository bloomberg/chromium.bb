// Copyright (c) 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "third_party/blink/renderer/modules/peerconnection/rtc_certificate_generator.h"

#include <string>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/single_thread_task_runner.h"
#include "third_party/blink/renderer/modules/peerconnection/peer_connection_dependency_factory.h"
#include "third_party/blink/renderer/platform/wtf/thread_safe_ref_counted.h"
#include "third_party/webrtc/api/scoped_refptr.h"
#include "third_party/webrtc/rtc_base/rtc_certificate.h"
#include "third_party/webrtc/rtc_base/rtc_certificate_generator.h"

namespace blink {
namespace {

// A certificate generation request spawned by
// |GenerateCertificateWithOptionalExpiration|. This
// is handled by a separate class so that reference counting can keep the
// request alive independently of the |RTCCertificateGenerator| that spawned it.
class RTCCertificateGeneratorRequest
    : public WTF::ThreadSafeRefCounted<RTCCertificateGeneratorRequest> {
 public:
  RTCCertificateGeneratorRequest(
      const scoped_refptr<base::SingleThreadTaskRunner>& main_thread,
      const scoped_refptr<base::SingleThreadTaskRunner>& worker_thread)
      : main_thread_(main_thread), worker_thread_(worker_thread) {
    DCHECK(main_thread_);
    DCHECK(worker_thread_);
  }

  void GenerateCertificateAsync(
      const rtc::KeyParams& key_params,
      const absl::optional<uint64_t>& expires_ms,
      blink::RTCCertificateCallback completion_callback) {
    DCHECK(main_thread_->BelongsToCurrentThread());
    DCHECK(completion_callback);

    worker_thread_->PostTask(
        FROM_HERE,
        base::BindOnce(
            &RTCCertificateGeneratorRequest::GenerateCertificateOnWorkerThread,
            this, key_params, expires_ms, std::move(completion_callback)));
  }

 private:
  friend class WTF::ThreadSafeRefCounted<RTCCertificateGeneratorRequest>;
  ~RTCCertificateGeneratorRequest() {}

  void GenerateCertificateOnWorkerThread(
      const rtc::KeyParams key_params,
      const absl::optional<uint64_t> expires_ms,
      blink::RTCCertificateCallback completion_callback) {
    DCHECK(worker_thread_->BelongsToCurrentThread());

    rtc::scoped_refptr<rtc::RTCCertificate> certificate =
        rtc::RTCCertificateGenerator::GenerateCertificate(key_params,
                                                          expires_ms);

    main_thread_->PostTask(
        FROM_HERE,
        base::BindOnce(&RTCCertificateGeneratorRequest::DoCallbackOnMainThread,
                       this, std::move(completion_callback), certificate));
  }

  void DoCallbackOnMainThread(
      blink::RTCCertificateCallback completion_callback,
      rtc::scoped_refptr<rtc::RTCCertificate> certificate) {
    DCHECK(main_thread_->BelongsToCurrentThread());
    DCHECK(completion_callback);
    std::move(completion_callback).Run(std::move(certificate));
  }

  // The main thread is the renderer thread.
  const scoped_refptr<base::SingleThreadTaskRunner> main_thread_;
  // The WebRTC worker thread.
  const scoped_refptr<base::SingleThreadTaskRunner> worker_thread_;
};

void GenerateCertificateWithOptionalExpiration(
    const rtc::KeyParams& key_params,
    const absl::optional<uint64_t>& expires_ms,
    blink::RTCCertificateCallback completion_callback,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  DCHECK(key_params.IsValid());
  auto* pc_dependency_factory =
      blink::PeerConnectionDependencyFactory::GetInstance();
  pc_dependency_factory->EnsureInitialized();

  scoped_refptr<RTCCertificateGeneratorRequest> request =
      base::MakeRefCounted<RTCCertificateGeneratorRequest>(
          task_runner, pc_dependency_factory->GetWebRtcWorkerTaskRunner());
  request->GenerateCertificateAsync(key_params, expires_ms,
                                    std::move(completion_callback));
}

}  // namespace

void RTCCertificateGenerator::GenerateCertificate(
    const rtc::KeyParams& key_params,
    blink::RTCCertificateCallback completion_callback,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  GenerateCertificateWithOptionalExpiration(
      key_params, absl::nullopt, std::move(completion_callback), task_runner);
}

void RTCCertificateGenerator::GenerateCertificateWithExpiration(
    const rtc::KeyParams& key_params,
    uint64_t expires_ms,
    blink::RTCCertificateCallback completion_callback,
    scoped_refptr<base::SingleThreadTaskRunner> task_runner) {
  GenerateCertificateWithOptionalExpiration(
      key_params, expires_ms, std::move(completion_callback), task_runner);
}

bool RTCCertificateGenerator::IsSupportedKeyParams(
    const rtc::KeyParams& key_params) {
  return key_params.IsValid();
}

rtc::scoped_refptr<rtc::RTCCertificate> RTCCertificateGenerator::FromPEM(
    String pem_private_key,
    String pem_certificate) {
  rtc::scoped_refptr<rtc::RTCCertificate> certificate =
      rtc::RTCCertificate::FromPEM(rtc::RTCCertificatePEM(
          pem_private_key.Utf8(), pem_certificate.Utf8()));
  if (!certificate)
    return nullptr;
  return certificate;
}

}  // namespace blink
