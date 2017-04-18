(() => {
  let mojo_;

  async function loadFakeBluetoothInterfaces() {
    if(typeof mojo_ !== 'undefined') {
      return mojo_;
    }

    if (typeof loadMojoModules === 'undefined') {
      throw 'Mojo is required for this API.'
    }

    mojo_ = await loadMojoModules('fakeBluetooth', [
      'mojo/public/js/bindings',
      'device/bluetooth/public/interfaces/test/fake_bluetooth.mojom'
    ]);

   [mojo_.bindings, mojo_.FakeBluetooth] = mojo_.modules;
    return mojo_;
  }

  class FakeBluetooth {
    constructor() {
      this.fake_bluetooth_ptr_ = undefined;
    }

    // Set it to indicate whether the platform supports BLE. For example,
    // Windows 7 is a platform that doesn't support Low Energy. On the other
    // hand Windows 10 is a platform that does support LE, even if there is no
    // Bluetooth radio available.
    async setLESupported(available) {
      if (typeof available !== 'boolean') throw 'Type Not Supported';
      await (await this.getFakeBluetoothInterface_()).setLESupported(available);

      // TODO(crbug.com/569709): Remove once FakeBluetooth.setLESupported is
      // implemented in the browser.
      navigator.bluetooth.requestDevice = function() {
        return Promise.reject(new DOMException(
            'Bluetooth Low Energy is not supported on this platform.',
            'NotFoundError'));
        };
    }

    async getFakeBluetoothInterface_() {
      if (typeof this.fake_bluetooth_ptr_ !== 'undefined') {
        return this.fake_bluetooth_ptr_;
      }

      let mojo = await loadFakeBluetoothInterfaces();

      this.fake_bluetooth_ptr_ = new mojo.FakeBluetooth.FakeBluetoothPtr(
      mojo.interfaces.getInterface(
        mojo.FakeBluetooth.FakeBluetooth.name));

      return this.fake_bluetooth_ptr_;
    }
  }

  navigator.bluetooth.test = new FakeBluetooth();
})();
