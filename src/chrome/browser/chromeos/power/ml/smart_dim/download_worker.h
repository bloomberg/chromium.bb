// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_DOWNLOAD_WORKER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_DOWNLOAD_WORKER_H_

#include "base/containers/flat_map.h"
#include "chrome/browser/chromeos/power/ml/smart_dim/smart_dim_worker.h"
#include "services/data_decoder/public/cpp/data_decoder.h"

namespace chromeos {
namespace power {
namespace ml {

// The contents of metadata JSON, preprocessor proto and model flatbuffer.
using ComponentFileContents = std::tuple<std::string, std::string, std::string>;

// SmartDimWorker that loads meta info, preprocessor config and ML service model
// files from smart dim components.
class DownloadWorker : public SmartDimWorker {
 public:
  DownloadWorker();
  ~DownloadWorker() override;

  // SmartDimWorker overrides:
  const assist_ranker::ExamplePreprocessorConfig* GetPreprocessorConfig()
      override;
  const mojo::Remote<chromeos::machine_learning::mojom::GraphExecutor>&
  GetExecutor() override;

  // Remotes used to execute functions in the ML service side.
  void LoadModelCallback(
      chromeos::machine_learning::mojom::LoadModelResult result);
  void CreateGraphExecutorCallback(
      chromeos::machine_learning::mojom::CreateGraphExecutorResult result);

  // Returns true if it has loaded components successfully.
  bool IsReady();

  // Loads meta info, preprocessor config and ML service model from smart dim
  // components.
  // Called by component updater when it gets a verified smart dim component and
  // DownloadWorker is not ready.
  // If IsReady(), this function won't be called again.
  void InitializeFromComponent(const ComponentFileContents& contents);

 private:
  base::flat_map<std::string, int> inputs_;
  base::flat_map<std::string, int> outputs_;
  std::string metrics_model_name_;

  void LoadModelAndCreateGraphExecutor(const std::string& model_flatbuffer);
  void OnJsonParsed(const std::string& model_flatbuffer,
                    const data_decoder::DataDecoder::ValueOrError result);

  DISALLOW_COPY_AND_ASSIGN(DownloadWorker);
};

}  // namespace ml
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_ML_SMART_DIM_DOWNLOAD_WORKER_H_
