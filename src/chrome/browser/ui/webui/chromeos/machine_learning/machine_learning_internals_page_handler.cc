// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/machine_learning/machine_learning_internals_page_handler.h"

#include <string>
#include <utility>

#include "base/logging.h"
#include "chromeos/services/machine_learning/public/cpp/service_connection.h"

namespace chromeos {
namespace machine_learning {

MachineLearningInternalsPageHandler::MachineLearningInternalsPageHandler(
    mojom::PageHandlerRequest request)
    : binding_(this, std::move(request)) {}

MachineLearningInternalsPageHandler::~MachineLearningInternalsPageHandler() =
    default;

void MachineLearningInternalsPageHandler::LoadModel(
    mojom::ModelSpecPtr spec,
    mojom::ModelRequest request,
    LoadModelCallback callback) {
  ServiceConnection::GetInstance()->LoadModel(
      std::move(spec), std::move(request), std::move(callback));
}

}  // namespace machine_learning
}  // namespace chromeos
