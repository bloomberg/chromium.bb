// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_SERVICE_CONNECTION_H_
#define CHROMEOS_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_SERVICE_CONNECTION_H_

#include "chromeos/services/machine_learning/public/mojom/machine_learning_service.mojom.h"

namespace chromeos {
namespace machine_learning {

// Encapsulates a connection to the Chrome OS ML Service daemon via its Mojo
// interface.
// Usage:
//   chromeos::machine_learning::mojom::ModelPtr model;
//   chromeos::machine_learning::mojom::ModelSpec spec = ...;
//   chromeos::machine_learning::ServiceConnection::GetInstance()
//       ->LoadModel(spec, mojom::MakeRequest(&model),
//                   base::BindOnce(&MyCallBack));
//   // Use |model| or wait for |MyCallBack|.
// Sequencing: Must be used on a single sequence (may be created on another).
class ServiceConnection {
 public:
  static ServiceConnection* GetInstance();
  // Overrides the result of GetInstance() for use in tests.
  // Does not take ownership of |fake_service_connection|.
  static void UseFakeServiceConnectionForTesting(
      ServiceConnection* fake_service_connection);

  // Instruct ML daemon to load the model specified in |spec|, binding a Model
  // implementation to |request|.
  // Bootstraps the initial Mojo connection to the daemon if necessary.
  virtual void LoadModel(
      mojom::ModelSpecPtr spec,
      mojom::ModelRequest request,
      mojom::MachineLearningService::LoadModelCallback result_callback) = 0;

 protected:
  ServiceConnection() = default;
  virtual ~ServiceConnection() {}
};

}  // namespace machine_learning
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_MACHINE_LEARNING_PUBLIC_CPP_SERVICE_CONNECTION_H_
