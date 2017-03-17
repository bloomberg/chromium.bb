'use strict';

function vibration_mocks(mojo) {
  return define(
      'VibrationManager mocks',
      [
        'mojo/public/js/bindings',
        'device/vibration/vibration_manager.mojom',
      ],
      (bindings, vibrationManager) => {
        class MockVibrationManager {
          constructor() {
            this.bindingSet =
                new bindings.BindingSet(vibrationManager.VibrationManager);

            this.vibrate_milliseconds_ = -1;
            this.cancelled_ = false;
          }

          vibrate(milliseconds) {
            this.vibrate_milliseconds_ = milliseconds;
            window.postMessage('Vibrate', '*');
            return Promise.resolve();
          }

          cancel() {
            this.cancelled_ = true;
            window.postMessage('Cancel', '*');
          }

          getDuration() {
            return this.vibrate_milliseconds_;
          }

          isCancelled() {
            return this.cancelled_;
          }

          reset() {
            this.vibrate_milliseconds_ = -1;
            this.cancelled_ = false;
          }
        }

        let mockVibrationManager = new MockVibrationManager;
        mojo.frameInterfaces.addInterfaceOverrideForTesting(
            vibrationManager.VibrationManager.name, handle => {
              mockVibrationManager.bindingSet.addBinding(
                  mockVibrationManager, handle);
            });

        return Promise.resolve({
          // Interface instance bound to main frame.
          mockVibrationManager: mockVibrationManager,
          // Constructor for mock VibrationManager class.
          MockVibrationManager: MockVibrationManager,
          // Loaded mojom interface.
          VibrationManager: vibrationManager.VibrationManager,
        });
      });
}

function vibration_test(func, name, properties) {
  mojo_test(
      mojo => vibration_mocks(mojo).then(vibration => {
        let result = Promise.resolve(func(vibration));
        let cleanUp = () => vibration.mockVibrationManager.reset();
        result.then(cleanUp, cleanUp);
        return result;
      }),
      name, properties);
}
