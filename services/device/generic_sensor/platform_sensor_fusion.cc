// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_fusion.h"

#include "base/logging.h"
#include "services/device/generic_sensor/platform_sensor_fusion_algorithm.h"
#include "services/device/generic_sensor/platform_sensor_provider.h"

namespace device {

PlatformSensorFusion::PlatformSensorFusion(
    mojo::ScopedSharedBufferMapping mapping,
    PlatformSensorProvider* provider,
    const PlatformSensorProviderBase::CreateSensorCallback& callback,
    std::unique_ptr<PlatformSensorFusionAlgorithm> fusion_algorithm)
    : PlatformSensor(fusion_algorithm->fused_type(),
                     std::move(mapping),
                     provider),
      callback_(callback),
      fusion_algorithm_(std::move(fusion_algorithm)) {
  if (!provider)
    return;

  for (mojom::SensorType type : fusion_algorithm_->source_types()) {
    scoped_refptr<PlatformSensor> sensor = provider->GetSensor(type);
    if (sensor) {
      AddSourceSensor(std::move(sensor));
    } else {
      provider->CreateSensor(
          type, base::Bind(&PlatformSensorFusion::CreateSensorCallback, this));
    }
  }
}

// static
void PlatformSensorFusion::Create(
    mojo::ScopedSharedBufferMapping mapping,
    PlatformSensorProvider* provider,
    std::unique_ptr<PlatformSensorFusionAlgorithm> fusion_algorithm,
    const PlatformSensorProviderBase::CreateSensorCallback& callback) {
  // TODO(Mikhail): Consider splitting PlatformSensorFusion construction and
  // fetching source sensors.
  scoped_refptr<PlatformSensor>(new PlatformSensorFusion(
      std::move(mapping), provider, callback, std::move(fusion_algorithm)));
}

mojom::ReportingMode PlatformSensorFusion::GetReportingMode() {
  return reporting_mode_;
}

PlatformSensorConfiguration PlatformSensorFusion::GetDefaultConfiguration() {
  PlatformSensorConfiguration default_configuration;
  for (const auto& pair : source_sensors_) {
    double frequency = pair.second->GetDefaultConfiguration().frequency();
    if (frequency > default_configuration.frequency())
      default_configuration.set_frequency(frequency);
  }
  return default_configuration;
}

bool PlatformSensorFusion::StartSensor(
    const PlatformSensorConfiguration& configuration) {
  for (const auto& pair : source_sensors_) {
    if (!pair.second->StartSensor(configuration))
      return false;
  }

  fusion_algorithm_->SetFrequency(configuration.frequency());
  return true;
}

void PlatformSensorFusion::StopSensor() {
  for (const auto& pair : source_sensors_)
    pair.second->StopSensor();

  fusion_algorithm_->Reset();
}

bool PlatformSensorFusion::CheckSensorConfiguration(
    const PlatformSensorConfiguration& configuration) {
  for (const auto& pair : source_sensors_) {
    if (!pair.second->CheckSensorConfiguration(configuration))
      return false;
  }
  return true;
}

void PlatformSensorFusion::OnSensorReadingChanged(mojom::SensorType type) {
  SensorReading reading;
  reading.raw.timestamp =
      (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();

  if (!fusion_algorithm_->GetFusedData(type, &reading))
    return;

  if (GetReportingMode() == mojom::ReportingMode::ON_CHANGE &&
      !fusion_algorithm_->IsReadingSignificantlyDifferent(reading_, reading)) {
    return;
  }

  reading_ = reading;
  UpdateSensorReading(reading_);
}

void PlatformSensorFusion::OnSensorError() {
  NotifySensorError();
}

bool PlatformSensorFusion::IsSuspended() {
  for (auto& client : clients_) {
    if (!client.IsSuspended())
      return false;
  }
  return true;
}

bool PlatformSensorFusion::GetSourceReading(mojom::SensorType type,
                                            SensorReading* result) {
  auto it = source_sensors_.find(type);
  if (it != source_sensors_.end())
    return it->second->GetLatestReading(result);
  NOTREACHED();
  return false;
}

PlatformSensorFusion::~PlatformSensorFusion() {
  if (source_sensors_.size() != fusion_algorithm_->source_types().size())
    return;

  for (const auto& pair : source_sensors_)
    pair.second->RemoveClient(this);
}

void PlatformSensorFusion::CreateSensorCallback(
    scoped_refptr<PlatformSensor> sensor) {
  if (sensor)
    AddSourceSensor(std::move(sensor));
  else if (callback_)
    std::move(callback_).Run(nullptr);
}

void PlatformSensorFusion::AddSourceSensor(
    scoped_refptr<PlatformSensor> sensor) {
  DCHECK(sensor);
  mojom::SensorType type = sensor->GetType();
  source_sensors_[type] = std::move(sensor);

  if (source_sensors_.size() != fusion_algorithm_->source_types().size())
    return;

  reporting_mode_ = mojom::ReportingMode::CONTINUOUS;

  for (const auto& pair : source_sensors_) {
    pair.second->AddClient(this);
    if (pair.second->GetReportingMode() == mojom::ReportingMode::ON_CHANGE)
      reporting_mode_ = mojom::ReportingMode::ON_CHANGE;
  }

  fusion_algorithm_->set_fusion_sensor(this);

  callback_.Run(this);
}

}  // namespace device
