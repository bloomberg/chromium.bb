'use strict';

// Wraps callback and calls rejectFunc if callback throws an error.
class CallbackWrapper {
  constructor(callback, rejectFunc) {
    this.wrapperFunc_ = (args) => {
      try {
        callback(args);
      } catch(e) {
        rejectFunc(e);
      }
    }
  }

  get callback() {
    return this.wrapperFunc_;
  }
}

function sensorMocks() {
  // Helper function that returns resolved promise with result.
  function sensorResponse(success) {
    return Promise.resolve({success});
  }

  // Class that mocks Sensor interface defined in sensor.mojom
  class MockSensor {
    constructor(sensorRequest, handle, offset, size, reportingMode) {
      this.client_ = null;
      this.startShouldFail_ = false;
      this.reportingMode_ = reportingMode;
      this.sensorReadingTimerId_ = null;
      this.updateReadingFunction_ = null;
      this.readingUpdatesCount_ = 0;
      this.suspendCalled_ = null;
      this.resumeCalled_ = null;
      this.addConfigurationCalled_ = null;
      this.removeConfigurationCalled_ = null;
      this.activeSensorConfigurations_ = [];
      let rv = handle.mapBuffer(offset, size);
      assert_equals(rv.result, Mojo.RESULT_OK, "Failed to map shared buffer");
      this.bufferArray_ = rv.buffer;
      this.buffer_ = new Float64Array(this.bufferArray_);
      this.resetBuffer();
      this.binding_ = new mojo.Binding(device.mojom.Sensor, this,
                                       sensorRequest);
      this.binding_.setConnectionErrorHandler(() => {
        this.reset();
      });
    }

    // Returns default configuration.
    getDefaultConfiguration() {
      return Promise.resolve({frequency: 5});
    }

    readingUpdatesCount() {
      return this.readingUpdatesCount_;
    }
    // Adds configuration for the sensor and starts reporting fake data
    // through updateReadingFunction_ callback.
    addConfiguration(configuration) {
      assert_not_equals(configuration, null, "Invalid sensor configuration.");

      this.activeSensorConfigurations_.push(configuration);
      // Sort using descending order.
      this.activeSensorConfigurations_.sort(
          (first, second) => { return second.frequency - first.frequency });

      if (!this.startShouldFail_ )
        this.startReading();

      if (this.addConfigurationCalled_ != null)
        this.addConfigurationCalled_(this);

      return sensorResponse(!this.startShouldFail_);
    }

    // Removes sensor configuration from the list of active configurations and
    // stops notification about sensor reading changes if
    // activeSensorConfigurations_ is empty.
    removeConfiguration(configuration) {
      if (this.removeConfigurationCalled_ != null) {
        this.removeConfigurationCalled_(this);
      }

      let index = this.activeSensorConfigurations_.indexOf(configuration);
      if (index !== -1) {
        this.activeSensorConfigurations_.splice(index, 1);
      } else {
        return sensorResponse(false);
      }

      if (this.activeSensorConfigurations_.length === 0)
        this.stopReading();

      return sensorResponse(true);
    }

    // Suspends sensor.
    suspend() {
      this.stopReading();
      if (this.suspendCalled_ != null) {
        this.suspendCalled_(this);
      }
    }

    // Resumes sensor.
    resume() {
      assert_equals(this.sensorReadingTimerId_, null);
      this.startReading();
      if (this.resumeCalled_ != null) {
        this.resumeCalled_(this);
      }
    }

    // Mock functions

    // Resets mock Sensor state.
    reset() {
      this.stopReading();

      this.readingUpdatesCount_ = 0;
      this.startShouldFail_ = false;
      this.updateReadingFunction_ = null;
      this.activeSensorConfigurations_ = [];
      this.suspendCalled_ = null;
      this.resumeCalled_ = null;
      this.addConfigurationCalled_ = null;
      this.removeConfigurationCalled_ = null;
      this.resetBuffer();
      this.bufferArray_ = null;
      this.binding_.close();
    }

    // Zeroes shared buffer.
    resetBuffer() {
      for (let i = 0; i < this.buffer_.length; ++i) {
        this.buffer_[i] = 0;
      }
    }

    // Sets callback that is used to deliver sensor reading updates.
    setUpdateSensorReadingFunction(updateReadingFunction) {
      this.updateReadingFunction_ = updateReadingFunction;
      return Promise.resolve(this);
    }

    // Sets flag that forces sensor to fail when addConfiguration is invoked.
    setStartShouldFail(shouldFail) {
      this.startShouldFail_ = shouldFail;
    }

    // Returns resolved promise if suspend() was called, rejected otherwise.
    suspendCalled() {
      return new Promise((resolve, reject) => {
        this.suspendCalled_ = resolve;
      });
    }

    // Returns resolved promise if resume() was called, rejected otherwise.
    resumeCalled() {
      return new Promise((resolve, reject) => {
        this.resumeCalled_ = resolve;
      });
    }

    // Resolves promise when addConfiguration() is called.
    addConfigurationCalled() {
      return new Promise((resolve, reject) => {
        this.addConfigurationCalled_ = resolve;
      });
    }

    // Resolves promise when removeConfiguration() is called.
    removeConfigurationCalled() {
      return new Promise((resolve, reject) => {
        this.removeConfigurationCalled_ = resolve;
      });
    }

    startReading() {
      if (this.updateReadingFunction_ != null) {
        this.stopReading();
        let maxFrequencyUsed = this.activeSensorConfigurations_[0].frequency;
        let timeout = (1 / maxFrequencyUsed) * 1000;
        this.sensorReadingTimerId_ = window.setInterval(() => {
          if (this.updateReadingFunction_) {
            this.updateReadingFunction_(this.buffer_);
            // For all tests sensor reading should have monotonically
            // increasing timestamp in seconds.
            this.buffer_[1] = window.performance.now() * 0.001;
            this.readingUpdatesCount_++;
          }
          if (this.reportingMode_ === device.mojom.ReportingMode.ON_CHANGE) {
            this.client_.sensorReadingChanged();
          }
        }, timeout);
      }
    }

    stopReading() {
      if (this.sensorReadingTimerId_ != null) {
        window.clearInterval(this.sensorReadingTimerId_);
        this.sensorReadingTimerId_ = null;
      }
    }

  }

  // Helper function that returns resolved promise for getSensor() function.
  function getSensorResponse(initParams, clientRequest) {
    return Promise.resolve({initParams, clientRequest});
  }

  // Class that mocks SensorProvider interface defined in
  // sensor_provider.mojom
  class MockSensorProvider {
    constructor() {
      this.readingSizeInBytes_ =
          device.mojom.SensorInitParams.kReadBufferSizeForTests;
      this.sharedBufferSizeInBytes_ = this.readingSizeInBytes_ *
              device.mojom.SensorType.LAST;
      let rv = Mojo.createSharedBuffer(this.sharedBufferSizeInBytes_);
      assert_equals(rv.result, Mojo.RESULT_OK, "Failed to create buffer");
      this.sharedBufferHandle_ = rv.handle;
      this.activeSensor_ = null;
      this.getSensorShouldFail_ = false;
      this.resolveFunc_ = null;
      this.isContinuous_ = false;
      this.maxFrequency_ = 60;
      this.minFrequency_ = 1;
      this.binding_ = new mojo.Binding(device.mojom.SensorProvider, this);

      this.interceptor_ = new MojoInterfaceInterceptor(
          device.mojom.SensorProvider.name);
      this.interceptor_.oninterfacerequest = e => {
        this.bindToPipe(e.handle);
      };
      this.interceptor_.start();
    }

    // Returns initialized Sensor proxy to the client.
    getSensor(type, request) {
      if (this.getSensorShouldFail_) {
        var ignored = new device.mojom.SensorClientPtr();
        return getSensorResponse(null, mojo.makeRequest(ignored));
      }

      let offset = (device.mojom.SensorType.LAST - type) *
          this.readingSizeInBytes_;
      let reportingMode = device.mojom.ReportingMode.ON_CHANGE;
      if (this.isContinuous_) {
        reportingMode = device.mojom.ReportingMode.CONTINUOUS;
      }

      if (this.activeSensor_ == null) {
        let mockSensor = new MockSensor(request, this.sharedBufferHandle_,
            offset, this.readingSizeInBytes_, reportingMode);
        this.activeSensor_ = mockSensor;
      }

      let rv = this.sharedBufferHandle_.duplicateBufferHandle();

      assert_equals(rv.result, Mojo.RESULT_OK);

      let defaultConfig = {frequency: 5};

      let initParams =
          new device.mojom.SensorInitParams(
              { memory: rv.handle,
                bufferOffset: offset,
                mode: reportingMode,
                defaultConfiguration: defaultConfig,
                minimumFrequency: this.minFrequency_,
                maximumFrequency: this.maxFrequency_});

      if (this.resolveFunc_ !== null) {
        this.resolveFunc_(this.activeSensor_);
      }

      this.activeSensor_.client_ = new device.mojom.SensorClientPtr();
      return getSensorResponse(
          initParams, mojo.makeRequest(this.activeSensor_.client_));
    }

    // Binds object to mojo message pipe
    bindToPipe(pipe) {
      this.binding_.bind(pipe);
      this.binding_.setConnectionErrorHandler(() => {
        this.reset();
      });
    }

    // Mock functions

    // Resets state of mock SensorProvider between test runs.
    reset() {
      if (this.activeSensor_ != null) {
        this.activeSensor_.reset();
        this.activeSensor_ = null;
      }

      this.getSensorShouldFail_ = false;
      this.resolveFunc_ = null;
      this.maxFrequency_ = 60;
      this.minFrequency_ = 1;
      this.isContinuous_ = false;
      this.binding_.close();
      this.interceptor_.stop();
    }

    // Sets flag that forces mock SensorProvider to fail when getSensor() is
    // invoked.
    setGetSensorShouldFail(shouldFail) {
      this.getSensorShouldFail_ = shouldFail;
    }

    // Returns mock sensor that was created in getSensor to the layout test.
    getCreatedSensor() {
      if (this.activeSensor_ != null) {
        return Promise.resolve(this.activeSensor_);
      }

      return new Promise((resolve, reject) => {
        this.resolveFunc_ = resolve;
      });
    }

    // Forces sensor to use |reportingMode| as an update mode.
    setContinuousReportingMode() {
      this.isContinuous_ = true;
    }

    // Sets the maximum frequency for a concrete sensor.
    setMaximumSupportedFrequency(frequency) {
      this.maxFrequency_ = frequency;
    }

    // Sets the minimum frequency for a concrete sensor.
    setMinimumSupportedFrequency(frequency) {
      this.minFrequency_ = frequency;
    }
  }

  let mockSensorProvider = new MockSensorProvider;
  return {mockSensorProvider: mockSensorProvider};
}

function sensor_test(func, name, properties) {
  mojo_test(() => {
    let sensor = sensorMocks();

    // Clean up and reset mock sensor stubs asynchronously, so that the blink
    // side closes its proxies and notifies JS sensor objects before new test is
    // started.
    let onSuccess = () => {
      sensor.mockSensorProvider.reset();
      return new Promise((resolve, reject) => { setTimeout(resolve, 0); });
    };

    let onFailure = error => {
      sensor.mockSensorProvider.reset();
      return new Promise((resolve, reject) => { setTimeout(() => {reject(error);}, 0); });
    };

    return Promise.resolve(func(sensor)).then(onSuccess, onFailure);
  }, name, properties);
}
