'use strict';

// Run a set of tests for a given |sensorType|. |updateReading| is
// a called by the test to provide the mock values for sensor. |verifyReading|
// is called so that the value read in JavaScript are the values expected (the ones
// sent by |updateReading|).
function runGenericSensorTests(sensorType, updateReading, verifyReading) {
  test(() => assert_throws(
    new RangeError(),
    () => new sensorType({frequency: -60})),
    'Test that negative frequency causes exception from constructor.');

  sensor_test(sensor => {
    sensor.mockSensorProvider.setGetSensorShouldFail(true);
    let sensorObject = new sensorType;
    sensorObject.start();
    return new Promise((resolve, reject) => {
      let wrapper = new CallbackWrapper(event => {
        assert_equals(sensorObject.state, 'errored');
        assert_equals(event.error.name, 'NotFoundError');
        sensorObject.onerror = null;
        resolve();
      }, reject);

      sensorObject.onerror = wrapper.callback;
    });
  }, 'Test that "onerror" is send when sensor is not supported.');

  sensor_test(sensor => {
      let sensorObject = new sensorType({frequency: 560});
      sensorObject.start();

      let testPromise = sensor.mockSensorProvider.getCreatedSensor()
        .then(mockSensor => {
          mockSensor.setStartShouldFail(true);
          return mockSensor.addConfigurationCalled(); })
        .then(mockSensor => {
          return new Promise((resolve, reject) => {
            let wrapper = new CallbackWrapper(event => {
              assert_equals(sensorObject.state, 'errored');
              assert_equals(event.error.name, 'OperationError');
              sensorObject.onerror = null;
              resolve();
            }, reject);

            sensorObject.onerror = wrapper.callback;
          });
        });
      return testPromise;
  }, 'Test that "onerror" is send when start() call has failed.');

  sensor_test(sensor => {
      let sensorObject = new sensorType({frequency: 560});
      sensorObject.start();

      let testPromise = sensor.mockSensorProvider.getCreatedSensor()
          .then(mockSensor => { return mockSensor.addConfigurationCalled(); })
          .then(mockSensor => {
            return new Promise((resolve, reject) => {
              let wrapper = new CallbackWrapper(() => {
                let configuration = mockSensor.active_sensor_configurations_[0];
                assert_equals(configuration.frequency, 60);
                sensorObject.stop();
                assert_equals(sensorObject.state, 'idle');
                resolve(mockSensor);
              }, reject);
              sensorObject.onactivate = wrapper.callback;
              sensorObject.onerror = reject;
            });
          })
          .then(mockSensor => { return mockSensor.removeConfigurationCalled(); });
      return testPromise;
  }, 'Test that frequency is capped to 60.0 Hz.');

  sensor_test(sensor => {
    let maxSupportedFrequency = 15;
    sensor.mockSensorProvider.setMaximumSupportedFrequency(maxSupportedFrequency);
    let sensorObject = new sensorType({frequency: 50});
    sensorObject.start();
    let testPromise = sensor.mockSensorProvider.getCreatedSensor()
        .then(mockSensor => { return mockSensor.addConfigurationCalled(); })
        .then(mockSensor => {
          return new Promise((resolve, reject) => {
            let wrapper = new CallbackWrapper(() => {
              let configuration = mockSensor.active_sensor_configurations_[0];
              assert_equals(configuration.frequency, maxSupportedFrequency);
              sensorObject.stop();
              assert_equals(sensorObject.state, 'idle');
              resolve(mockSensor);
           }, reject);
           sensorObject.onactivate = wrapper.callback;
           sensorObject.onerror = reject;
          });
        })
        .then(mockSensor => { return mockSensor.removeConfigurationCalled(); });
    return testPromise;
  }, 'Test that frequency is capped to the maximum supported from frequency.');

  sensor_test(sensor => {
  let sensorObject = new sensorType({frequency: 60});
  sensorObject.start();
  let testPromise = sensor.mockSensorProvider.getCreatedSensor()
      .then((mockSensor) => {
        return new Promise((resolve, reject) => {
          let wrapper = new CallbackWrapper(() => {
            assert_equals(sensorObject.state, 'activated');
            sensorObject.stop();
            assert_equals(sensorObject.state, 'idle');
            resolve(mockSensor);
          }, reject);
          sensorObject.onactivate = wrapper.callback;
          sensorObject.onerror = reject;
        });
      })
      .then(mockSensor => { return mockSensor.removeConfigurationCalled(); });
  return testPromise;
  }, 'Test that sensor can be successfully created if sensor is supported.');

  sensor_test(sensor => {
    let sensorObject = new sensorType();
    sensorObject.start();
    let testPromise = sensor.mockSensorProvider.getCreatedSensor()
        .then((mockSensor) => {
          return new Promise((resolve, reject) => {
            let wrapper = new CallbackWrapper(() => {
              assert_equals(sensorObject.state, 'activated');
              sensorObject.stop();
              assert_equals(sensorObject.state, 'idle');
              resolve(mockSensor);
            }, reject);

            sensorObject.onactivate = wrapper.callback;
            sensorObject.onerror = reject;
          });
        })
        .then(mockSensor => { return mockSensor.removeConfigurationCalled(); });
    return testPromise;
  }, 'Test that sensor can be constructed with default configuration.');

  sensor_test(sensor => {
    let sensorObject = new sensorType({frequency: 60});
    sensorObject.start();

    let testPromise = sensor.mockSensorProvider.getCreatedSensor()
        .then(mockSensor => { return mockSensor.addConfigurationCalled(); })
        .then(mockSensor => {
          return new Promise((resolve, reject) => {
            let wrapper = new CallbackWrapper(() => {
              assert_equals(sensorObject.state, 'activated');
              sensorObject.stop();
              assert_equals(sensorObject.state, 'idle');
              resolve(mockSensor);
           }, reject);
           sensorObject.onactivate = wrapper.callback;
           sensorObject.onerror = reject;
          });
        })
        .then(mockSensor => { return mockSensor.removeConfigurationCalled(); });

    return testPromise;
  }, 'Test that addConfiguration and removeConfiguration is called.');

  sensor_test(sensor => {
  let sensorObject = new sensorType({frequency: 60});
  sensorObject.start();
  let testPromise = sensor.mockSensorProvider.getCreatedSensor()
      .then(mockSensor => {
        return mockSensor.setUpdateSensorReadingFunction(updateReading);
      })
      .then((mockSensor) => {
        return new Promise((resolve, reject) => {
          let wrapper = new CallbackWrapper(() => {
            assert_true(verifyReading(sensorObject.reading));
            sensorObject.stop();
            assert_equals(sensorObject.reading, null);
            resolve(mockSensor);
          }, reject);

          sensorObject.onchange = wrapper.callback;
          sensorObject.onerror = reject;
        });
      })
      .then(mockSensor => { return mockSensor.removeConfigurationCalled(); });

  return testPromise;
  }, 'Test that onChange is called and sensor reading is valid.');

  sensor_test(sensor => {
    let sensorObject = new sensorType;
    sensorObject.start();
    let testPromise = sensor.mockSensorProvider.getCreatedSensor()
        .then(mockSensor => {
          return mockSensor.setUpdateSensorReadingFunction(updateReading);
        })
        .then(mockSensor => {
          return new Promise((resolve, reject) => {
            let wrapper = new CallbackWrapper(() => {
              assert_true(verifyReading(sensorObject.reading));
              resolve(mockSensor);
            }, reject);

            sensorObject.onchange = wrapper.callback;
            sensorObject.onerror = reject;
          });
        })
        .then(mockSensor => {
          testRunner.setPageVisibility('hidden');
          return mockSensor.suspendCalled();
        })
        .then(mockSensor => {
          testRunner.setPageVisibility('visible');
          return mockSensor.resumeCalled();
        })
        .then(mockSensor => {
          return new Promise((resolve, reject) => {
            sensorObject.stop();
            resolve(mockSensor);
            sensorObject.onerror = reject;
          });
        })
        .then(mockSensor => { return mockSensor.removeConfigurationCalled(); });

    return testPromise;
  }, 'Test that sensor receives suspend / resume notifications when page'
      + ' visibility changes.');

sensor_test(sensor => {
  let sensor1 = new sensorType({frequency: 60});
  sensor1.start();

  let sensor2 = new sensorType({frequency: 20});
  sensor2.start();
  let testPromise = sensor.mockSensorProvider.getCreatedSensor()
      .then(mockSensor => {
        return mockSensor.setUpdateSensorReadingFunction(update_sensor_reading);
      })
      .then((mockSensor) => {
        return new Promise((resolve, reject) => {
          let wrapper = new CallbackWrapper(() => {
            // Reading value is correct.
            assert_true(verifyReading(sensor1.reading));

            // Both sensors share the same reading instance.
            let reading = sensor1.reading;
            assert_equals(reading, sensor2.reading);

            // After first sensor stops its reading is null, reading for second
            // sensor sensor remains.
            sensor1.stop();
            assert_equals(sensor1.reading, null);
            assert_true(verifyReading(sensor2.reading));

            sensor2.stop();
            assert_equals(sensor2.reading, null);

            // Cached reading remains.
            assert_true(verifyReading(reading));
            resolve(mockSensor);
          }, reject);

          sensor1.onchange = wrapper.callback;
          sensor1.onerror = reject;
          sensor2.onerror = reject;
        });
      })
      .then(mockSensor => { return mockSensor.removeConfigurationCalled(); });

  return testPromise;
}, 'Test that sensor reading is correct.');
}
