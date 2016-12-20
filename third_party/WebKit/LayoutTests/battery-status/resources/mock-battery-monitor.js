"use strict";

let mockBatteryMonitor = loadMojoModules(
    'mockBatteryMonitor',
    ['device/battery/battery_monitor.mojom',
     'device/battery/battery_status.mojom',
     'mojo/public/js/router',
    ]).then(mojo => {
  let [batteryMonitor, batteryStatus, router] = mojo.modules;

  class MockBatteryMonitor extends batteryMonitor.BatteryMonitor.stubClass {
    constructor(interfaceProvider) {
      super();
      interfaceProvider.addInterfaceOverrideForTesting(
          batteryMonitor.BatteryMonitor.name,
          handle => this.connect_(handle));

      this.interfaceProvider_ = interfaceProvider;
      this.pendingRequests_ = [];
      this.status_ = null;
    }

    connect_(handle) {
      this.router_ = new router.Router(handle);
      this.router_.setIncomingReceiver(this);
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
  return new MockBatteryMonitor(mojo.interfaces);
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
