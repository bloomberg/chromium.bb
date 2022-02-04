// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/handwriting/handwriting_recognition_service_impl_cros.h"

#include <memory>
#include <utility>

#include "base/command_line.h"
#include "base/memory/ptr_util.h"
#include "build/chromeos_buildflags.h"
#include "content/browser/handwriting/handwriting_recognizer_impl_cros.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/self_owned_receiver.h"
#include "third_party/blink/public/mojom/handwriting/handwriting.mojom.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "ash/constants/ash_switches.h"
#endif

#if BUILDFLAG(IS_CHROMEOS_LACROS)
#include "chromeos/lacros/lacros_service.h"
#endif

namespace content {

namespace {

// Currently, we do not consider that ondevice handwriting recognition may be
// supported by the CrOS Downloadable Content (DLC) service other than on
// rootfs.
bool IsCrOSLibHandwritingRootfsEnabled() {
#if BUILDFLAG(IS_CHROMEOS_ASH)
  // TODO(https://crbug.com/1168978): mlservice should provide an interface to
  // query this.
  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  return command_line->HasSwitch(ash::switches::kOndeviceHandwritingSwitch) &&
         command_line->GetSwitchValueASCII(
             ash::switches::kOndeviceHandwritingSwitch) == "use_rootfs";
#elif BUILDFLAG(IS_CHROMEOS_LACROS)
  auto* service = chromeos::LacrosService::Get();
  return service && service->init_params()->ondevice_handwriting_support ==
                        crosapi::mojom::OndeviceHandwritingSupport::kUseRootfs;
#else
  return false;
#endif
}
}  // namespace

// static
void CrOSHandwritingRecognitionServiceImpl::Create(
    mojo::PendingReceiver<handwriting::mojom::HandwritingRecognitionService>
        receiver) {
  mojo::MakeSelfOwnedReceiver<
      handwriting::mojom::HandwritingRecognitionService>(
      base::WrapUnique(new CrOSHandwritingRecognitionServiceImpl()),
      std::move(receiver));
}

CrOSHandwritingRecognitionServiceImpl::
    ~CrOSHandwritingRecognitionServiceImpl() = default;

void CrOSHandwritingRecognitionServiceImpl::CreateHandwritingRecognizer(
    handwriting::mojom::HandwritingModelConstraintPtr model_constraint,
    handwriting::mojom::HandwritingRecognitionService::
        CreateHandwritingRecognizerCallback callback) {
  if (!IsCrOSLibHandwritingRootfsEnabled()) {
    std::move(callback).Run(
        handwriting::mojom::CreateHandwritingRecognizerResult::kNotSupported,
        mojo::NullRemote());
    return;
  }

  CrOSHandwritingRecognizerImpl::Create(std::move(model_constraint),
                                        std::move(callback));
}

void CrOSHandwritingRecognitionServiceImpl::QueryHandwritingRecognizer(
    handwriting::mojom::HandwritingModelConstraintPtr model_constraint,
    QueryHandwritingRecognizerCallback callback) {
  if (!IsCrOSLibHandwritingRootfsEnabled()) {
    std::move(callback).Run(nullptr);
    return;
  }

  std::move(callback).Run(CrOSHandwritingRecognizerImpl::GetModelDescriptor(
      std::move(model_constraint)));
}

CrOSHandwritingRecognitionServiceImpl::CrOSHandwritingRecognitionServiceImpl() =
    default;

}  // namespace content
