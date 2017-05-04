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

    [mojo_.bindings, {
      CentralState: mojo_.CentralState,
      FakeBluetooth: mojo_.FakeBluetooth,
      FakeBluetoothPtr: mojo_.FakeBluetoothPtr,
      FakeCentral: mojo_.FakeCentral,
      FakeCentralPtr: mojo_.FakeCentralPtr,
    }] = mojo_.modules;

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

    // Returns a promise that resolves with a FakeCentral that clients can use
    // to simulate events that a device in the Central/Observer role would
    // receive as well as monitor the operations performed by the device in the
    // Central/Observer role.
    // Calls sets LE as supported.
    //
    // A "Central" object would allow its clients to receive advertising events
    // and initiate connections to peripherals i.e. operations of two roles
    // defined by the Bluetooth Spec: Observer and Central.
    // See Bluetooth 4.2 Vol 3 Part C 2.2.2 "Roles when Operating over an
    // LE Physical Transport".
    async simulateCentral({state}) {
      // Call setBluetoothFakeAdapter() to clean up any fake adapters left over
      // by legacy tests.
      // Legacy tests that use setBluetoothFakeAdapter() sometimes fail to clean
      // their fake adapter. This is not a problem for these tests because the
      // next setBluetoothFakeAdapter() will clean it up anyway but it is a
      // problem for the new tests that do not use setBluetoothFakeAdapter().
      // TODO(crbug.com/569709): Remove once setBluetoothFakeAdapter is no
      // longer used.
      await setBluetoothFakeAdapter('');

      await this.setLESupported(true);
      let mojo = await loadFakeBluetoothInterfaces();

      let mojo_manager_state;
      switch (state) {
        case 'absent':
          mojo_manager_state = mojo.CentralState.ABSENT;
          break;
        case 'powered-off':
          mojo_manager_state = mojo.CentralState.POWERED_OFF;
          break;
        case 'powered-on':
          mojo_manager_state = mojo.CentralState.POWERED_ON;
          break;
        default:
          throw `Unsupported value ${state} for state.`;
      }

      let {fake_central:fake_central_ptr} =
        await (await this.getFakeBluetoothInterface_()).simulateCentral(
          mojo_manager_state);

      return new FakeCentral(fake_central_ptr);
    }

    async getFakeBluetoothInterface_() {
      if (typeof this.fake_bluetooth_ptr_ !== 'undefined') {
        return this.fake_bluetooth_ptr_;
      }

      let mojo = await loadFakeBluetoothInterfaces();

      this.fake_bluetooth_ptr_ = new mojo.FakeBluetoothPtr(
        mojo.interfaces.getInterface(mojo.FakeBluetooth.name));

      return this.fake_bluetooth_ptr_;
    }
  }

  // FakeCentral allows clients to simulate events that a device in the
  // Central/Observer role would receive as well as monitor the operations
  // performed by the device in the Central/Observer role.
  class FakeCentral {
    constructor(fake_central_ptr) {
      this.fake_central_ptr = fake_central_ptr;
    }
  }

  navigator.bluetooth.test = new FakeBluetooth();
})();
