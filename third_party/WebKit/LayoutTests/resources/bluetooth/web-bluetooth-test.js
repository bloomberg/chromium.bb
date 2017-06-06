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

  function toMojoCentralState(state) {
    switch (state) {
      case 'absent':
        return mojo_.CentralState.ABSENT;
      case 'powered-off':
        return mojo_.CentralState.POWERED_OFF;
      case 'powered-on':
        return mojo_.CentralState.POWERED_ON;
      default:
        throw `Unsupported value ${state} for state.`;
    }
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
      await this.initFakeBluetoothInterfacePtr_();

      if (typeof supported !== 'boolean') throw 'Type Not Supported';
      await this.fake_bluetooth_ptr_.setLESupported(supported);
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
      await this.initFakeBluetoothInterfacePtr_();

      await this.setLESupported(true);

      let {fake_central:fake_central_ptr} =
        await this.fake_bluetooth_ptr_.simulateCentral(
          toMojoCentralState(state));
      return new FakeCentral(fake_central_ptr);
    }

    async initFakeBluetoothInterfacePtr_() {
      if (typeof this.fake_bluetooth_ptr_ !== 'undefined') {
        return this.fake_bluetooth_ptr_;
      }

      let mojo = await loadFakeBluetoothInterfaces();

      this.fake_bluetooth_ptr_ = new mojo.FakeBluetoothPtr(
        mojo.interfaces.getInterface(mojo.FakeBluetooth.name));
    }
  }

  // FakeCentral allows clients to simulate events that a device in the
  // Central/Observer role would receive as well as monitor the operations
  // performed by the device in the Central/Observer role.
  class FakeCentral {
    constructor(fake_central_ptr) {
      this.fake_central_ptr_ = fake_central_ptr;
      this.peripherals_ = new Map();
    }

    // Simulates a peripheral with |address|, |name| and |known_service_uuids|
    // that has already been connected to the system. If the peripheral existed
    // already it updates its name and known UUIDs. |known_service_uuids| should
    // be an array of BluetoothServiceUUIDs
    // https://webbluetoothcg.github.io/web-bluetooth/#typedefdef-bluetoothserviceuuid
    //
    // Platforms offer methods to retrieve devices that have already been
    // connected to the system or weren't connected through the UA e.g. a user
    // connected a peripheral through the system's settings. This method is
    // intended to simulate peripherals that those methods would return.
    async simulatePreconnectedPeripheral({
      address, name, knownServiceUUIDs = []}) {

      // Canonicalize and convert to mojo UUIDs.
      knownServiceUUIDs.forEach((val, i, arr) => {
        knownServiceUUIDs[i] = {uuid: BluetoothUUID.getService(val)};
      });

      await this.fake_central_ptr_.simulatePreconnectedPeripheral(
        address, name, knownServiceUUIDs);

      let peripheral = this.peripherals_.get(address);
      if (peripheral === undefined) {
        peripheral = new FakePeripheral(address, this.fake_central_ptr_);
        this.peripherals_.set(address, peripheral);
      }

      return peripheral;
    }
  }

  class FakePeripheral {
    constructor(address, fake_central_ptr) {
      this.address = address;
      this.fake_central_ptr_ = fake_central_ptr;
    }

    // Sets the next GATT Connection request response to |code|. |code| could be
    // an HCI Error Code from BT 4.2 Vol 2 Part D 1.3 List Of Error Codes or a
    // number outside that range returned by specific platforms e.g. Android
    // returns 0x101 to signal a GATT failure
    // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#GATT_FAILURE
    async setNextGATTConnectionResponse({code}) {
      let {success} =
        await this.fake_central_ptr_.setNextGATTConnectionResponse(
          this.address, code);

      if (success !== true) throw 'setNextGATTConnectionResponse failed.';
    }

    // Sets the next GATT Discovery request response for peripheral with
    // |address| to |code|. |code| could be an HCI Error Code from
    // BT 4.2 Vol 2 Part D 1.3 List Of Error Codes or a number outside that
    // range returned by specific platforms e.g. Android returns 0x101 to signal
    // a GATT failure
    // https://developer.android.com/reference/android/bluetooth/BluetoothGatt.html#GATT_FAILURE
    //
    // The following procedures defined at BT 4.2 Vol 3 Part G Section 4.
    // "GATT Feature Requirements" are used to discover attributes of the
    // GATT Server:
    //  - Primary Service Discovery
    //  - Relationship Discovery
    //  - Characteristic Discovery
    //  - Characteristic Descriptor Discovery
    // This method aims to simulate the response once all of these procedures
    // have completed or if there was an error during any of them.
    async setNextGATTDiscoveryResponse({code}) {
      let {success} =
        await this.fake_central_ptr_.setNextGATTDiscoveryResponse(
          this.address, code);

      if (success !== true) throw 'setNextGATTDiscoveryResponse failed.';
    }
  }

  navigator.bluetooth.test = new FakeBluetooth();
})();
