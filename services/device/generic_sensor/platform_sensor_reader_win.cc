// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "services/device/generic_sensor/platform_sensor_reader_win.h"

#include <Sensors.h>
#include <objbase.h>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/time/time.h"
#include "base/win/iunknown_impl.h"
#include "base/win/scoped_propvariant.h"
#include "services/device/generic_sensor/generic_sensor_consts.h"
#include "services/device/public/cpp/generic_sensor/platform_sensor_configuration.h"
#include "services/device/public/cpp/generic_sensor/sensor_reading.h"

namespace device {

// Init params for the PlatformSensorReaderWin.
struct ReaderInitParams {
  // ISensorDataReport::GetSensorValue is not const, therefore, report
  // cannot be passed as const ref.
  // ISensorDataReport* report - report that contains new sensor data.
  // SensorReading* reading    - out parameter that must be populated.
  // Returns HRESULT           - S_OK on success, otherwise error code.
  using ReaderFunctor = base::Callback<HRESULT(ISensorDataReport* report,
                                               SensorReading* reading)>;
  SENSOR_TYPE_ID sensor_type_id;
  ReaderFunctor reader_func;
  unsigned long min_reporting_interval_ms = 0;
};

namespace {

// Gets value from  the report for provided key.
bool GetReadingValueForProperty(REFPROPERTYKEY key,
                                ISensorDataReport* report,
                                double* value) {
  DCHECK(value);
  base::win::ScopedPropVariant variant_value;
  if (SUCCEEDED(report->GetSensorValue(key, variant_value.Receive()))) {
    if (variant_value.get().vt == VT_R8)
      *value = variant_value.get().dblVal;
    else if (variant_value.get().vt == VT_R4)
      *value = variant_value.get().fltVal;
    else
      return false;
    return true;
  }

  *value = 0;
  return false;
}

// Ambient light sensor reader initialization parameters.
std::unique_ptr<ReaderInitParams> CreateAmbientLightReaderInitParams() {
  auto params = base::MakeUnique<ReaderInitParams>();
  params->sensor_type_id = SENSOR_TYPE_AMBIENT_LIGHT;
  params->reader_func =
      base::Bind([](ISensorDataReport* report, SensorReading* reading) {
        double lux = 0.0;
        if (!GetReadingValueForProperty(SENSOR_DATA_TYPE_LIGHT_LEVEL_LUX,
                                        report, &lux)) {
          return E_FAIL;
        }
        reading->values[0] = lux;
        return S_OK;
      });
  return params;
}

// Accelerometer sensor reader initialization parameters.
std::unique_ptr<ReaderInitParams> CreateAccelerometerReaderInitParams() {
  auto params = base::MakeUnique<ReaderInitParams>();
  params->sensor_type_id = SENSOR_TYPE_ACCELEROMETER_3D;
  params->reader_func =
      base::Bind([](ISensorDataReport* report, SensorReading* reading) {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
        if (!GetReadingValueForProperty(SENSOR_DATA_TYPE_ACCELERATION_X_G,
                                        report, &x) ||
            !GetReadingValueForProperty(SENSOR_DATA_TYPE_ACCELERATION_Y_G,
                                        report, &y) ||
            !GetReadingValueForProperty(SENSOR_DATA_TYPE_ACCELERATION_Z_G,
                                        report, &z)) {
          return E_FAIL;
        }

        // Windows uses coordinate system where Z axis points down from device
        // screen, therefore, using right hand notation, we have to reverse
        // sign for each axis. Values are converted from G/s^2 to m/s^2.
        reading->values[0] = -x * kMeanGravity;
        reading->values[1] = -y * kMeanGravity;
        reading->values[2] = -z * kMeanGravity;
        return S_OK;
      });
  return params;
}

// Gyroscope sensor reader initialization parameters.
std::unique_ptr<ReaderInitParams> CreateGyroscopeReaderInitParams() {
  auto params = base::MakeUnique<ReaderInitParams>();
  params->sensor_type_id = SENSOR_TYPE_GYROMETER_3D;
  params->reader_func =
      base::Bind([](ISensorDataReport* report, SensorReading* reading) {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
        if (!GetReadingValueForProperty(
                SENSOR_DATA_TYPE_ANGULAR_VELOCITY_X_DEGREES_PER_SECOND, report,
                &x) ||
            !GetReadingValueForProperty(
                SENSOR_DATA_TYPE_ANGULAR_VELOCITY_Y_DEGREES_PER_SECOND, report,
                &y) ||
            !GetReadingValueForProperty(
                SENSOR_DATA_TYPE_ANGULAR_VELOCITY_Z_DEGREES_PER_SECOND, report,
                &z)) {
          return E_FAIL;
        }

        // Windows uses coordinate system where Z axis points down from device
        // screen, therefore, using right hand notation, we have to reverse
        // sign for each axis. Values are converted from deg to rad.
        reading->values[0] = -x * kRadiansInDegrees;
        reading->values[1] = -y * kRadiansInDegrees;
        reading->values[2] = -z * kRadiansInDegrees;
        return S_OK;
      });
  return params;
}

// Magnetometer sensor reader initialization parameters.
std::unique_ptr<ReaderInitParams> CreateMagnetometerReaderInitParams() {
  auto params = base::MakeUnique<ReaderInitParams>();
  params->sensor_type_id = SENSOR_TYPE_COMPASS_3D;
  params->reader_func =
      base::Bind([](ISensorDataReport* report, SensorReading* reading) {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
        if (!GetReadingValueForProperty(
                SENSOR_DATA_TYPE_MAGNETIC_FIELD_STRENGTH_X_MILLIGAUSS, report,
                &x) ||
            !GetReadingValueForProperty(
                SENSOR_DATA_TYPE_MAGNETIC_FIELD_STRENGTH_Y_MILLIGAUSS, report,
                &y) ||
            !GetReadingValueForProperty(
                SENSOR_DATA_TYPE_MAGNETIC_FIELD_STRENGTH_Z_MILLIGAUSS, report,
                &z)) {
          return E_FAIL;
        }

        // Windows uses coordinate system where Z axis points down from device
        // screen, therefore, using right hand notation, we have to reverse
        // sign for each axis. Values are converted from Milligaus to
        // Microtesla.
        reading->values[0] = -x * kMicroteslaInMilligauss;
        reading->values[1] = -y * kMicroteslaInMilligauss;
        reading->values[2] = -z * kMicroteslaInMilligauss;
        return S_OK;
      });
  return params;
}

// AbsoluteOrientationEulerAngles sensor reader initialization parameters.
std::unique_ptr<ReaderInitParams>
CreateAbsoluteOrientationEulerAnglesReaderInitParams() {
  auto params = base::MakeUnique<ReaderInitParams>();
  params->sensor_type_id = SENSOR_TYPE_INCLINOMETER_3D;
  params->reader_func =
      base::Bind([](ISensorDataReport* report, SensorReading* reading) {
        double x = 0.0;
        double y = 0.0;
        double z = 0.0;
        if (!GetReadingValueForProperty(SENSOR_DATA_TYPE_TILT_X_DEGREES, report,
                                        &x) ||
            !GetReadingValueForProperty(SENSOR_DATA_TYPE_TILT_Y_DEGREES, report,
                                        &y) ||
            !GetReadingValueForProperty(SENSOR_DATA_TYPE_TILT_Z_DEGREES, report,
                                        &z)) {
          return E_FAIL;
        }

        reading->values[0] = x;
        reading->values[1] = y;
        reading->values[2] = z;
        return S_OK;
      });
  return params;
}

// AbsoluteOrientationQuaternion sensor reader initialization parameters.
std::unique_ptr<ReaderInitParams>
CreateAbsoluteOrientationQuaternionReaderInitParams() {
  auto params = base::MakeUnique<ReaderInitParams>();
  params->sensor_type_id = SENSOR_TYPE_AGGREGATED_DEVICE_ORIENTATION;
  params->reader_func =
      base::Bind([](ISensorDataReport* report, SensorReading* reading) {
        base::win::ScopedPropVariant quat_variant;
        HRESULT hr = report->GetSensorValue(SENSOR_DATA_TYPE_QUATERNION,
                                            quat_variant.Receive());
        if (FAILED(hr) || quat_variant.get().vt != (VT_VECTOR | VT_UI1) ||
            quat_variant.get().caub.cElems < 16) {
          return E_FAIL;
        }

        float* quat = reinterpret_cast<float*>(quat_variant.get().caub.pElems);

        // Windows uses coordinate system where Z axis points down from device
        // screen, therefore, using right hand notation, we have to reverse
        // sign for each quaternion component.
        reading->values[0] = -quat[0];  // x*sin(Theta/2)
        reading->values[1] = -quat[1];  // y*sin(Theta/2)
        reading->values[2] = -quat[2];  // z*sin(Theta/2)
        reading->values[3] = quat[3];   // cos(Theta/2)
        return S_OK;
      });
  return params;
}

// Creates ReaderInitParams params structure. To implement support for new
// sensor types, new switch case should be added and appropriate fields must
// be set:
// sensor_type_id - GUID of the sensor supported by Windows.
// reader_func    - Functor that is responsible to populate SensorReading from
//                  ISensorDataReport data.
std::unique_ptr<ReaderInitParams> CreateReaderInitParamsForSensor(
    mojom::SensorType type) {
  switch (type) {
    case mojom::SensorType::AMBIENT_LIGHT:
      return CreateAmbientLightReaderInitParams();
    case mojom::SensorType::ACCELEROMETER:
      return CreateAccelerometerReaderInitParams();
    case mojom::SensorType::GYROSCOPE:
      return CreateGyroscopeReaderInitParams();
    case mojom::SensorType::MAGNETOMETER:
      return CreateMagnetometerReaderInitParams();
    case mojom::SensorType::ABSOLUTE_ORIENTATION_EULER_ANGLES:
      return CreateAbsoluteOrientationEulerAnglesReaderInitParams();
    case mojom::SensorType::ABSOLUTE_ORIENTATION_QUATERNION:
      return CreateAbsoluteOrientationQuaternionReaderInitParams();
    default:
      NOTIMPLEMENTED();
      return nullptr;
  }
}

}  // namespace

// Class that implements ISensorEvents and IUnknown interfaces and used
// by ISensor interface to dispatch state and data change events.
class EventListener : public ISensorEvents, public base::win::IUnknownImpl {
 public:
  explicit EventListener(PlatformSensorReaderWin* platform_sensor_reader)
      : platform_sensor_reader_(platform_sensor_reader) {
    DCHECK(platform_sensor_reader_);
  }

  // IUnknown interface
  ULONG STDMETHODCALLTYPE AddRef() override { return IUnknownImpl::AddRef(); }
  ULONG STDMETHODCALLTYPE Release() override { return IUnknownImpl::Release(); }

  STDMETHODIMP QueryInterface(REFIID riid, void** ppv) override {
    if (riid == __uuidof(ISensorEvents)) {
      *ppv = static_cast<ISensorEvents*>(this);
      AddRef();
      return S_OK;
    }
    return IUnknownImpl::QueryInterface(riid, ppv);
  }

 protected:
  ~EventListener() override = default;

  // ISensorEvents interface
  STDMETHODIMP OnEvent(ISensor*, REFGUID, IPortableDeviceValues*) override {
    return S_OK;
  }

  STDMETHODIMP OnLeave(REFSENSOR_ID sensor_id) override {
    // If event listener is active and sensor is disconnected, notify client
    // about the error.
    platform_sensor_reader_->SensorError();
    platform_sensor_reader_->StopSensor();
    return S_OK;
  }

  STDMETHODIMP OnStateChanged(ISensor* sensor, SensorState state) override {
    if (sensor == nullptr)
      return E_INVALIDARG;

    if (state != SensorState::SENSOR_STATE_READY &&
        state != SensorState::SENSOR_STATE_INITIALIZING) {
      platform_sensor_reader_->SensorError();
      platform_sensor_reader_->StopSensor();
    }
    return S_OK;
  }

  STDMETHODIMP OnDataUpdated(ISensor* sensor,
                             ISensorDataReport* report) override {
    if (sensor == nullptr || report == nullptr)
      return E_INVALIDARG;

    // To get precise timestamp, we need to get delta between timestamp
    // provided in the report and current system time. Then the delta in
    // milliseconds is substracted from current high resolution timestamp.
    SYSTEMTIME report_time;
    HRESULT hr = report->GetTimestamp(&report_time);
    if (FAILED(hr))
      return hr;

    base::TimeTicks ticks_now = base::TimeTicks::Now();
    base::Time time_now = base::Time::NowFromSystemTime();

    base::Time::Exploded exploded;
    exploded.year = report_time.wYear;
    exploded.month = report_time.wMonth;
    exploded.day_of_week = report_time.wDayOfWeek;
    exploded.day_of_month = report_time.wDay;
    exploded.hour = report_time.wHour;
    exploded.minute = report_time.wMinute;
    exploded.second = report_time.wSecond;
    exploded.millisecond = report_time.wMilliseconds;

    base::Time timestamp;
    if (!base::Time::FromUTCExploded(exploded, &timestamp))
      return E_FAIL;

    base::TimeDelta delta = time_now - timestamp;

    SensorReading reading;
    reading.timestamp = ((ticks_now - delta) - base::TimeTicks()).InSecondsF();

    // Discard update events that have non-monotonically increasing timestamp.
    if (last_sensor_reading_.timestamp > reading.timestamp)
      return E_FAIL;

    hr = platform_sensor_reader_->SensorReadingChanged(report, &reading);
    if (SUCCEEDED(hr))
      last_sensor_reading_ = reading;
    return hr;
  }

 private:
  PlatformSensorReaderWin* const platform_sensor_reader_;
  SensorReading last_sensor_reading_;

  DISALLOW_COPY_AND_ASSIGN(EventListener);
};

// static
std::unique_ptr<PlatformSensorReaderWin> PlatformSensorReaderWin::Create(
    mojom::SensorType type,
    base::win::ScopedComPtr<ISensorManager> sensor_manager) {
  DCHECK(sensor_manager);

  auto params = CreateReaderInitParamsForSensor(type);
  if (!params)
    return nullptr;

  auto sensor = GetSensorForType(params->sensor_type_id, sensor_manager);
  if (!sensor)
    return nullptr;

  base::win::ScopedPropVariant min_interval;
  HRESULT hr = sensor->GetProperty(SENSOR_PROPERTY_MIN_REPORT_INTERVAL,
                                   min_interval.Receive());
  if (SUCCEEDED(hr) && min_interval.get().vt == VT_UI4)
    params->min_reporting_interval_ms = min_interval.get().ulVal;

  GUID interests[] = {SENSOR_EVENT_STATE_CHANGED, SENSOR_EVENT_DATA_UPDATED};
  hr = sensor->SetEventInterest(interests, arraysize(interests));
  if (FAILED(hr))
    return nullptr;

  return base::WrapUnique(
      new PlatformSensorReaderWin(sensor, std::move(params)));
}

// static
base::win::ScopedComPtr<ISensor> PlatformSensorReaderWin::GetSensorForType(
    REFSENSOR_TYPE_ID sensor_type,
    base::win::ScopedComPtr<ISensorManager> sensor_manager) {
  base::win::ScopedComPtr<ISensor> sensor;
  base::win::ScopedComPtr<ISensorCollection> sensor_collection;
  HRESULT hr = sensor_manager->GetSensorsByType(
      sensor_type, sensor_collection.GetAddressOf());
  if (FAILED(hr) || !sensor_collection)
    return sensor;

  ULONG count = 0;
  hr = sensor_collection->GetCount(&count);
  if (SUCCEEDED(hr) && count > 0)
    sensor_collection->GetAt(0, sensor.GetAddressOf());
  return sensor;
}

PlatformSensorReaderWin::PlatformSensorReaderWin(
    base::win::ScopedComPtr<ISensor> sensor,
    std::unique_ptr<ReaderInitParams> params)
    : init_params_(std::move(params)),
      task_runner_(base::ThreadTaskRunnerHandle::Get()),
      sensor_active_(false),
      client_(nullptr),
      sensor_(sensor),
      event_listener_(new EventListener(this)),
      weak_factory_(this) {
  DCHECK(init_params_);
  DCHECK(!init_params_->reader_func.is_null());
  DCHECK(sensor_);
}

void PlatformSensorReaderWin::SetClient(Client* client) {
  base::AutoLock autolock(lock_);
  // Can be null.
  client_ = client;
}

void PlatformSensorReaderWin::StopSensor() {
  base::AutoLock autolock(lock_);
  if (sensor_active_) {
    sensor_->SetEventSink(nullptr);
    sensor_active_ = false;
  }
}

PlatformSensorReaderWin::~PlatformSensorReaderWin() {
  DCHECK(task_runner_->BelongsToCurrentThread());
}

bool PlatformSensorReaderWin::StartSensor(
    const PlatformSensorConfiguration& configuration) {
  base::AutoLock autolock(lock_);

  if (!SetReportingInterval(configuration))
    return false;

  if (!sensor_active_) {
    task_runner_->PostTask(
        FROM_HERE, base::Bind(&PlatformSensorReaderWin::ListenSensorEvent,
                              weak_factory_.GetWeakPtr()));
    sensor_active_ = true;
  }

  return true;
}

void PlatformSensorReaderWin::ListenSensorEvent() {
  // Set event listener.
  if (FAILED(sensor_->SetEventSink(event_listener_.get()))) {
    SensorError();
    StopSensor();
  }
}

bool PlatformSensorReaderWin::SetReportingInterval(
    const PlatformSensorConfiguration& configuration) {
  base::win::ScopedComPtr<IPortableDeviceValues> props;
  if (SUCCEEDED(::CoCreateInstance(CLSID_PortableDeviceValues, nullptr,
                                   CLSCTX_ALL, IID_PPV_ARGS(&props)))) {
    unsigned interval =
        (1 / configuration.frequency()) * base::Time::kMillisecondsPerSecond;

    HRESULT hr = props->SetUnsignedIntegerValue(
        SENSOR_PROPERTY_CURRENT_REPORT_INTERVAL, interval);

    if (SUCCEEDED(hr)) {
      base::win::ScopedComPtr<IPortableDeviceValues> return_props;
      hr = sensor_->SetProperties(props.Get(), return_props.GetAddressOf());
      return SUCCEEDED(hr);
    }
  }
  return false;
}

HRESULT PlatformSensorReaderWin::SensorReadingChanged(
    ISensorDataReport* report,
    SensorReading* reading) const {
  if (!client_)
    return E_FAIL;

  HRESULT hr = init_params_->reader_func.Run(report, reading);
  if (SUCCEEDED(hr))
    client_->OnReadingUpdated(*reading);
  return hr;
}

void PlatformSensorReaderWin::SensorError() {
  if (client_)
    client_->OnSensorError();
}

unsigned long PlatformSensorReaderWin::GetMinimalReportingIntervalMs() const {
  return init_params_->min_reporting_interval_ms;
}

}  // namespace device
