"use strict";

let mockBatteryMonitor = loadMojoModules('mockBatteryMonitor', [
                           'services/device/public/interfaces/battery_monitor.mojom',
                           'services/device/public/interfaces/battery_status.mojom',
                           'services/device/public/interfaces/constants.mojom',
                           'mojo/public/js/bindings',
                         ]).then(mojo => {
  let [batteryMonitor, batteryStatus, deviceConstants, bindings] = mojo.modules;

  class MockBatteryMonitor {
    constructor(connector) {
      connector.addInterfaceOverrideForTesting(
          deviceConstants.kServiceName, batteryMonitor.BatteryMonitor.name,
          handle => this.bindingSet_.addBinding(this, handle));

      this.pendingRequests_ = [];
      this.status_ = null;
      this.bindingSet_ = new bindings.BindingSet(batteryMonitor.BatteryMonitor);
    }

    queryNextStatus() {
      let result = new Promise(resolve => this.pendingRequests_.push(resolve));
      this.runCallbacks_();
      return result;
    }

    updateBatteryStatus(charging, chargingTime, dischargingTime, level) {
      this.status_ = new batteryStatus.BatteryStatus();
      this.status_.charging = charging;
      this.status_.charging_time = chargingTime;
      this.status_.discharging_time = dischargingTime;
      this.status_.level = level;
      this.runCallbacks_();
    }

    runCallbacks_() {
      if (!this.status_ || !this.pendingRequests_.length)
        return;

      while (this.pendingRequests_.length) {
        this.pendingRequests_.pop()({status: this.status_});
      }
      this.status_ = null;
    }
  }
  return new MockBatteryMonitor(mojo.connector);
});

let batteryInfo;
let lastSetMockBatteryInfo;

function setAndFireMockBatteryInfo(charging, chargingTime, dischargingTime,
                                   level) {
  lastSetMockBatteryInfo = { charging: charging,
                      chargingTime: chargingTime,
                      dischargingTime: dischargingTime,
                      level: level };
  mockBatteryMonitor.then(mock => mock.updateBatteryStatus(
      charging, chargingTime, dischargingTime, level));
}

// compare obtained battery values with the mock values
function testIfBatteryStatusIsUpToDate(batteryManager) {
  batteryInfo = batteryManager;
  shouldBeDefined("batteryInfo");
  shouldBeDefined("lastSetMockBatteryInfo");
  shouldBe('batteryInfo.charging', 'lastSetMockBatteryInfo.charging');
  shouldBe('batteryInfo.chargingTime', 'lastSetMockBatteryInfo.chargingTime');
  shouldBe('batteryInfo.dischargingTime',
           'lastSetMockBatteryInfo.dischargingTime');
  shouldBe('batteryInfo.level', 'lastSetMockBatteryInfo.level');
}

function batteryStatusFailure() {
  testFailed('failed to successfully resolve the promise');
  setTimeout(finishJSTest, 0);
}

var mockBatteryMonitorReady = mockBatteryMonitor.then();
