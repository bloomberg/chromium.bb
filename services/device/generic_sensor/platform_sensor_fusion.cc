// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_fusion.h"

#include "services/device/generic_sensor/platform_sensor_fusion_algorithm.h"
#include "services/device/generic_sensor/platform_sensor_provider.h"

namespace device {

PlatformSensorFusion::PlatformSensorFusion(
    mojo::ScopedSharedBufferMapping mapping,
    PlatformSensorProvider* provider,
    const PlatformSensorProviderBase::CreateSensorCallback& callback,
    const std::vector<mojom::SensorType>& source_sensor_types,
    mojom::SensorType fusion_sensor_type,
    std::unique_ptr<PlatformSensorFusionAlgorithm> fusion_algorithm)
    : PlatformSensor(fusion_sensor_type, std::move(mapping), provider),
      callback_(callback),
      fusion_algorithm_(std::move(fusion_algorithm)) {
  source_sensors_.resize(source_sensor_types.size());

  for (size_t i = 0; i < source_sensor_types.size(); ++i) {
    source_sensors_[i] = provider->GetSensor(source_sensor_types[i]);
    if (source_sensors_[i]) {
      CreateSensorSucceeded();
    } else {
      provider->CreateSensor(
          source_sensor_types[i],
          base::Bind(&PlatformSensorFusion::CreateSensorCallback, this, i));
    }
  }
}

mojom::ReportingMode PlatformSensorFusion::GetReportingMode() {
  for (const auto& sensor : source_sensors_) {
    if (sensor->GetReportingMode() == mojom::ReportingMode::ON_CHANGE)
      return mojom::ReportingMode::ON_CHANGE;
  }
  return mojom::ReportingMode::CONTINUOUS;
}

PlatformSensorConfiguration PlatformSensorFusion::GetDefaultConfiguration() {
  PlatformSensorConfiguration default_configuration;
  for (const auto& sensor : source_sensors_) {
    double frequency = sensor->GetDefaultConfiguration().frequency();
    if (frequency > default_configuration.frequency())
      default_configuration.set_frequency(frequency);
  }
  return default_configuration;
}

bool PlatformSensorFusion::StartSensor(
    const PlatformSensorConfiguration& configuration) {
  for (const auto& sensor : source_sensors_) {
    if (!sensor->StartSensor(configuration))
      return false;
  }

  if (GetReportingMode() == mojom::ReportingMode::CONTINUOUS) {
    timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMicroseconds(base::Time::kMicrosecondsPerSecond /
                                          configuration.frequency()),
        this, &PlatformSensorFusion::OnSensorReadingChanged);
  }

  fusion_algorithm_->SetFrequency(configuration.frequency());

  return true;
}

void PlatformSensorFusion::StopSensor() {
  for (const auto& sensor : source_sensors_)
    sensor->StopSensor();

  if (timer_.IsRunning())
    timer_.Stop();

  fusion_algorithm_->Reset();
}

bool PlatformSensorFusion::CheckSensorConfiguration(
    const PlatformSensorConfiguration& configuration) {
  for (const auto& sensor : source_sensors_) {
    if (!sensor->CheckSensorConfiguration(configuration))
      return false;
  }
  return true;
}

void PlatformSensorFusion::OnSensorReadingChanged() {
  SensorReading reading;
  reading.timestamp = (base::TimeTicks::Now() - base::TimeTicks()).InSecondsF();

  std::vector<SensorReading> sensor_readings(source_sensors_.size());
  for (size_t i = 0; i < source_sensors_.size(); ++i) {
    if (!source_sensors_[i]->GetLatestReading(&sensor_readings[i]))
      return;
  }

  fusion_algorithm_->GetFusedData(sensor_readings, &reading);

  if (GetReportingMode() == mojom::ReportingMode::ON_CHANGE &&
      !fusion_algorithm_->IsReadingSignificantlyDifferent(reading_, reading)) {
    return;
  }

  reading_ = reading;
  UpdateSensorReading(reading_,
                      GetReportingMode() == mojom::ReportingMode::ON_CHANGE);
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

PlatformSensorFusion::~PlatformSensorFusion() {
  for (const auto& sensor : source_sensors_) {
    if (sensor)
      sensor->RemoveClient(this);
  }
}

void PlatformSensorFusion::CreateSensorCallback(
    size_t index,
    scoped_refptr<PlatformSensor> sensor) {
  source_sensors_[index] = sensor;
  if (source_sensors_[index])
    CreateSensorSucceeded();
  else if (callback_)
    std::move(callback_).Run(nullptr);
}

void PlatformSensorFusion::CreateSensorSucceeded() {
  ++num_sensors_created_;
  if (num_sensors_created_ != source_sensors_.size())
    return;

  for (const auto& sensor : source_sensors_)
    sensor->AddClient(this);

  callback_.Run(this);
}

}  // namespace device
