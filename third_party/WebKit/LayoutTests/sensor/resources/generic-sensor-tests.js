'use strict';

// Run a set of tests for a given |sensorType|. |updateReading| is
// a called by the test to provide the mock values for sensor. |verifyReading|
// is called so that the value read in JavaScript are the values expected (the ones
// sent by |updateReading|).
function runGenericSensorTests(sensorType, updateReading, verifyReading) {
  const prefix = sensorType.name + ': ';
  sensor_test(sensor => {
    sensor.mockSensorProvider.setGetSensorShouldFail(true);
    let sensorObject = new sensorType;
    sensorObject.start();
    return new Promise((resolve, reject) => {
      let wrapper = new CallbackWrapper(event => {
        assert_false(sensorObject.activated);
        assert_equals(event.error.name, 'NotReadableError');
        sensorObject.onerror = null;
        resolve();
      }, reject);

      sensorObject.onerror = wrapper.callback;
    });
  }, prefix + 'Test that "onerror" is send when sensor is not supported.');

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
              assert_false(sensorObject.activated);
              assert_equals(event.error.name, 'NotReadableError');
              sensorObject.onerror = null;
              resolve();
            }, reject);

            sensorObject.onerror = wrapper.callback;
          });
        });
      return testPromise;
  }, prefix + 'Test that "onerror" is send when start() call has failed.');

  sensor_test(sensor => {
    let sensorObject = new sensorType();
    sensorObject.start();

    return sensor.mockSensorProvider.getCreatedSensor()
      .then(mockSensor => {
        mockSensor.setStartShouldFail(true);
        return mockSensor.addConfigurationCalled(); })
      .then(mockSensor => mockSensor.removeConfigurationCalled());
  }, prefix + 'Test that no pending configuration left after start() failure.');

  sensor_test(sensor => {
      let sensorObject = new sensorType({frequency: 560});
      sensorObject.start();

      let testPromise = sensor.mockSensorProvider.getCreatedSensor()
          .then(mockSensor => { return mockSensor.addConfigurationCalled(); })
          .then(mockSensor => {
            return new Promise((resolve, reject) => {
              let wrapper = new CallbackWrapper(() => {
                assert_less_than_equal(mockSensor.getSamplingFrequency(), 60);
                sensorObject.stop();
                assert_false(sensorObject.activated);
                resolve(mockSensor);
              }, reject);
              sensorObject.onactivate = wrapper.callback;
              sensorObject.onerror = reject;
            });
          })
          .then(mockSensor => { return mockSensor.removeConfigurationCalled(); });
      return testPromise;
  }, prefix + 'Test that frequency is capped to allowed maximum.');

  sensor_test(sensor => {
    let sensorObject = new sensorType();
    sensorObject.start();
    return sensor.mockSensorProvider.getCreatedSensor()
        .then(mockSensor => mockSensor.addConfigurationCalled())
        .then(mockSensor => {
          return new Promise((resolve, reject) => {
            sensorObject.onactivate = () => {
              // Now sensor proxy is initialized.
              let anotherSensor = new sensorType({frequency: 21});
              anotherSensor.start();
              anotherSensor.stop();
              resolve(mockSensor);
            }
          });
        })
        .then(mockSensor => mockSensor.removeConfigurationCalled())
        .then(mockSensor => {
          sensorObject.stop();
          return mockSensor;
        })
        .then(mockSensor => mockSensor.removeConfigurationCalled());
  }, prefix + 'Test that configuration is removed for a stopped sensor.');

  sensor_test(sensor => {
    let maxSupportedFrequency = 5;
    sensor.mockSensorProvider.setMaximumSupportedFrequency(maxSupportedFrequency);
    let sensorObject = new sensorType({frequency: 50});
    sensorObject.start();
    let testPromise = sensor.mockSensorProvider.getCreatedSensor()
        .then(mockSensor => { return mockSensor.addConfigurationCalled(); })
        .then(mockSensor => {
          return new Promise((resolve, reject) => {
            let wrapper = new CallbackWrapper(() => {
              assert_equals(mockSensor.getSamplingFrequency(), maxSupportedFrequency);
              sensorObject.stop();
              assert_false(sensorObject.activated);
              resolve(mockSensor);
           }, reject);
           sensorObject.onactivate = wrapper.callback;
           sensorObject.onerror = reject;
          });
        })
        .then(mockSensor => { return mockSensor.removeConfigurationCalled(); });
    return testPromise;
  }, prefix + 'Test that frequency is capped to the maximum supported from frequency.');

  sensor_test(sensor => {
    let minSupportedFrequency = 2;
    sensor.mockSensorProvider.setMinimumSupportedFrequency(minSupportedFrequency);
    let sensorObject = new sensorType({frequency: -1});
    sensorObject.start();
    let testPromise = sensor.mockSensorProvider.getCreatedSensor()
        .then(mockSensor => { return mockSensor.addConfigurationCalled(); })
        .then(mockSensor => {
          return new Promise((resolve, reject) => {
            let wrapper = new CallbackWrapper(() => {
              assert_equals(mockSensor.getSamplingFrequency(), minSupportedFrequency);
              sensorObject.stop();
              assert_false(sensorObject.activated);
              resolve(mockSensor);
           }, reject);
           sensorObject.onactivate = wrapper.callback;
           sensorObject.onerror = reject;
          });
        })
        .then(mockSensor => { return mockSensor.removeConfigurationCalled(); });
    return testPromise;
  }, prefix + 'Test that frequency is limited to the minimum supported from frequency.');

  sensor_test(sensor => {
    let sensorObject = new sensorType({frequency: 60});
    assert_false(sensorObject.activated);
    sensorObject.start();
    assert_false(sensorObject.activated);
    let testPromise = sensor.mockSensorProvider.getCreatedSensor()
      .then((mockSensor) => {
        return new Promise((resolve, reject) => {
          let wrapper = new CallbackWrapper(() => {
            assert_true(sensorObject.activated);
            sensorObject.stop();
            assert_false(sensorObject.activated);
            resolve(mockSensor);
          }, reject);
          sensorObject.onactivate = wrapper.callback;
          sensorObject.onerror = reject;
        });
      })
      .then(mockSensor => { return mockSensor.removeConfigurationCalled(); });
    return testPromise;
  }, prefix + 'Test that sensor can be successfully created and its states are correct.');

  sensor_test(sensor => {
    let sensorObject = new sensorType();
    sensorObject.start();
    let testPromise = sensor.mockSensorProvider.getCreatedSensor()
        .then((mockSensor) => {
          return new Promise((resolve, reject) => {
            let wrapper = new CallbackWrapper(() => {
              assert_true(sensorObject.activated);
              sensorObject.stop();
              assert_false(sensorObject.activated);
              resolve(mockSensor);
            }, reject);

            sensorObject.onactivate = wrapper.callback;
            sensorObject.onerror = reject;
          });
        })
        .then(mockSensor => { return mockSensor.removeConfigurationCalled(); });
    return testPromise;
  }, prefix + 'Test that sensor can be constructed with default configuration.');

  sensor_test(sensor => {
    let sensorObject = new sensorType({frequency: 60});
    sensorObject.start();

    let testPromise = sensor.mockSensorProvider.getCreatedSensor()
        .then(mockSensor => { return mockSensor.addConfigurationCalled(); })
        .then(mockSensor => {
          return new Promise((resolve, reject) => {
            let wrapper = new CallbackWrapper(() => {
              assert_true(sensorObject.activated);
              sensorObject.stop();
              assert_false(sensorObject.activated);
              resolve(mockSensor);
           }, reject);
           sensorObject.onactivate = wrapper.callback;
           sensorObject.onerror = reject;
          });
        })
        .then(mockSensor => { return mockSensor.removeConfigurationCalled(); });

    return testPromise;
  }, prefix + 'Test that addConfiguration and removeConfiguration is called.');

  function checkOnChangeIsCalledAndReadingIsValid(sensor) {
    let sensorObject = new sensorType({frequency: 60});
    sensorObject.start();
    let testPromise = sensor.mockSensorProvider.getCreatedSensor()
        .then(mockSensor => {
          return mockSensor.setUpdateSensorReadingFunction(updateReading);
        })
        .then((mockSensor) => {
          return new Promise((resolve, reject) => {
            let wrapper = new CallbackWrapper(() => {
              assert_true(verifyReading(sensorObject));
              sensorObject.stop();
              assert_true(verifyReading(sensorObject, true /*should be null*/));
              resolve(mockSensor);
            }, reject);

            sensorObject.onreading = wrapper.callback;
            sensorObject.onerror = reject;
          });
        })
        .then(mockSensor => { return mockSensor.removeConfigurationCalled(); });

      return testPromise;
  }

  sensor_test(sensor => {
    return checkOnChangeIsCalledAndReadingIsValid(sensor);
  }, prefix + 'Test that onChange is called and sensor reading is valid (onreading reporting).');

  sensor_test(sensor => {
    sensor.mockSensorProvider.setContinuousReportingMode();
    return checkOnChangeIsCalledAndReadingIsValid(sensor);
  }, prefix + 'Test that onChange is called and sensor reading is valid (continuous reporting).');

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
              assert_true(verifyReading(sensorObject));
              resolve(mockSensor);
            }, reject);

            sensorObject.onreading = wrapper.callback;
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
  }, prefix + 'Test that sensor receives suspend / resume notifications when page'
      + ' visibility changes.');

  sensor_test(sensor => {
    let sensorObject = new sensorType;
    sensorObject.start();

    // Create a focused editbox inside a cross-origin iframe, sensor notification must suspend.
    const iframeSrc = 'data:text/html;charset=utf-8,<html><body><input type="text" autofocus></body></html>';
    let iframe = document.createElement('iframe');
    iframe.src = encodeURI(iframeSrc);

    return sensor.mockSensorProvider.getCreatedSensor()
        .then(mockSensor => mockSensor.setUpdateSensorReadingFunction(updateReading))
        .then(mockSensor => new Promise((resolve, reject) => {
          let wrapper = new CallbackWrapper(() => {
            assert_true(verifyReading(sensorObject));
            resolve(mockSensor);
          }, reject);

          sensorObject.onreading = wrapper.callback;
          sensorObject.onerror = reject;
        }))
        .then(mockSensor => {
          document.body.appendChild(iframe);
          return mockSensor.suspendCalled();
        })
        .then(mockSensor => {
          window.focus();
          return mockSensor.resumeCalled();
        })
        .then(mockSensor => new Promise((resolve, reject) => {
          sensorObject.stop();
          document.body.removeChild(iframe);
          resolve(mockSensor);
          sensorObject.onerror = reject;
        }))
        .then(mockSensor => mockSensor.removeConfigurationCalled());
  }, prefix + 'Test that sensor receives suspend / resume notifications when'
      + ' cross-origin subframe is focused');

  sensor_test(sensor => {
    let sensor1 = new sensorType({frequency: 60});
    sensor1.start();

    let sensor2 = new sensorType({frequency: 20});
    sensor2.start();
    let testPromise = sensor.mockSensorProvider.getCreatedSensor()
        .then(mockSensor => {
          return mockSensor.setUpdateSensorReadingFunction(updateReading);
        })
        .then((mockSensor) => {
          return new Promise((resolve, reject) => {
            let wrapper = new CallbackWrapper(() => {
              // Reading values are correct for both sensors.
              assert_true(verifyReading(sensor1));
              assert_true(verifyReading(sensor2));

              // After first sensor stops its reading values are null,
              // reading values for the second sensor sensor remain.
              sensor1.stop();
              assert_true(verifyReading(sensor1, true /*should be null*/));
              assert_true(verifyReading(sensor2));

              sensor2.stop();
              assert_true(verifyReading(sensor2, true /*should be null*/));

              resolve(mockSensor);
            }, reject);

            sensor1.onreading = wrapper.callback;
            sensor1.onerror = reject;
            sensor2.onerror = reject;
          });
        })
        .then(mockSensor => { return mockSensor.removeConfigurationCalled(); });

    return testPromise;
  }, prefix + 'Test that sensor reading is correct.');

  function checkFrequencyHintWorks(sensor) {
    let fastSensor = new sensorType({frequency: 30});
    let slowSensor = new sensorType({frequency: 9});
    slowSensor.start();

    let testPromise = sensor.mockSensorProvider.getCreatedSensor()
        .then(mockSensor => {
          return mockSensor.setUpdateSensorReadingFunction(updateReading);
        })
        .then(mockSensor => {
          return new Promise((resolve, reject) => {
            let fastSensorNotifiedCounter = 0;
            let slowSensorNotifiedCounter = 0;
            let readingUpdatesCounter = 0;

            let fastSensorWrapper = new CallbackWrapper(() => {
              fastSensorNotifiedCounter++;
            }, reject);

            let slowSensorWrapper = new CallbackWrapper(() => {
              slowSensorNotifiedCounter++;
              if (slowSensorNotifiedCounter == 1) {
                  fastSensor.start();
                  readingUpdatesCounter = mockSensor.readingUpdatesCount();
              } else if (slowSensorNotifiedCounter == 2) {
                let elapsedUpdates = mockSensor.readingUpdatesCount() - readingUpdatesCounter;
                // Approximation because 'slowSensor.onreading' is sometimes
                // called before 'fastSensor.onreading', in this case
                // 'fastSensorNotifiedCounter == elapsedUpdates - 1'.
                assert_approx_equals(fastSensorNotifiedCounter, elapsedUpdates, 1);
                fastSensor.stop();
                slowSensor.stop();
                resolve(mockSensor);
              }
            }, reject);

            fastSensor.onreading = fastSensorWrapper.callback;
            slowSensor.onreading = slowSensorWrapper.callback;
            fastSensor.onerror = reject;
            slowSensor.onerror = reject;
          });
        })
        .then(mockSensor => { return mockSensor.removeConfigurationCalled(); });

    return testPromise;
  }

  sensor_test(sensor => {
    return checkFrequencyHintWorks(sensor);
  }, prefix + 'Test that frequency hint works (onreading reporting).');

  sensor_test(sensor => {
    sensor.mockSensorProvider.setContinuousReportingMode();
    return checkFrequencyHintWorks(sensor);
  }, prefix + 'Test that frequency hint works (continuous reporting).');

  promise_test(() => {
    return new Promise((resolve,reject) => {
      let iframe = document.createElement('iframe');
      iframe.srcdoc = '<script>' +
                      '  window.onmessage = message => {' +
                      '    if (message.data === "LOADED") {' +
                      '      try {' +
                      '        new ' + sensorType.name + '();' +
                      '        parent.postMessage("FAIL", "*");' +
                      '      } catch (e) {' +
                      '        parent.postMessage("PASS", "*");' +
                      '      }' +
                      '    }' +
                      '   };' +
                      '<\/script>';
      iframe.onload = () => iframe.contentWindow.postMessage('LOADED', '*');
      document.body.appendChild(iframe);
      window.onmessage = message => {
        if (message.data == 'PASS') {
          resolve();
        } else if (message.data == 'FAIL') {
          reject();
        }
      }
    });
  }, prefix + 'Test that sensor cannot be constructed within iframe.');
}
