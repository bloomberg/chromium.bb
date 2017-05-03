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
    // Bluetooth radio present.
    async setLESupported(supported) {
      // Call setBluetoothFakeAdapter() to clean up any fake adapters left over
      // by legacy tests.
      // Legacy tests that use setBluetoothFakeAdapter() sometimes fail to clean
      // their fake adapter. This is not a problem for these tests because the
      // next setBluetoothFakeAdapter() will clean it up anyway but it is a
      // problem for the new tests that do not use setBluetoothFakeAdapter().
      // TODO(crbug.com/569709): Remove once setBluetoothFakeAdapter is no
      // longer used.
      await setBluetoothFakeAdapter('');

      if (typeof supported !== 'boolean') throw 'Type Not Supported';
      await (await this.getFakeBluetoothInterface_()).setLESupported(supported);
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
