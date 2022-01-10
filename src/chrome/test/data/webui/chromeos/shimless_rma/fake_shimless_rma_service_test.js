// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {fakeCalibrationComponents} from 'chrome://shimless-rma/fake_data.js';
import {FakeShimlessRmaService} from 'chrome://shimless-rma/fake_shimless_rma_service.js';
import {CalibrationComponentStatus, CalibrationObserverRemote, CalibrationOverallStatus, CalibrationSetupInstruction, CalibrationStatus, ComponentRepairStatus, ComponentType, ErrorObserverRemote, FinalizationObserverRemote, FinalizationStatus, HardwareVerificationStatusObserverRemote, HardwareWriteProtectionStateObserverRemote, OsUpdateObserverRemote, OsUpdateOperation, PowerCableStateObserverRemote, ProvisioningObserverRemote, ProvisioningStatus, RmadErrorCode, State, UpdateRoFirmwareObserverRemote, UpdateRoFirmwareStatus, WriteProtectDisableCompleteAction} from 'chrome://shimless-rma/shimless_rma_types.js';

import {assertDeepEquals, assertEquals, assertFalse, assertTrue} from '../../chai_assert.js';

export function fakeShimlessRmaServiceTestSuite() {
  /** @type {?FakeShimlessRmaService} */
  let service = null;

  setup(() => {
    service = new FakeShimlessRmaService();
  });

  teardown(() => {
    service = null;
  });


  test('GetCurrentStateDefaultRmaNotRequired', () => {
    return service.getCurrentState().then((state) => {
      assertEquals(state.state, State.kUnknown);
      assertEquals(state.error, RmadErrorCode.kRmaNotRequired);
    });
  });

  test('GetCurrentStateWelcomeOk', () => {
    let states = [
      {
        state: State.kWelcomeScreen,
        canCancel: true,
        canGoBack: false,
        error: RmadErrorCode.kOk
      },
    ];
    service.setStates(states);

    return service.getCurrentState().then((state) => {
      assertEquals(state.state, State.kWelcomeScreen);
      assertTrue(state.canCancel);
      assertFalse(state.canGoBack);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('GetCurrentStateWelcomeError', () => {
    let states = [
      {state: State.kWelcomeScreen, error: RmadErrorCode.kMissingComponent},
    ];
    service.setStates(states);

    return service.getCurrentState().then((state) => {
      assertEquals(state.state, State.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kMissingComponent);
    });
  });

  test('TransitionPreviousStateWelcomeOk', () => {
    let states = [
      {state: State.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    service.beginFinalization().then((state) => {
      assertEquals(state.state, State.kUpdateOs);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
    return service.transitionPreviousState().then((state) => {
      assertEquals(state.state, State.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('TransitionPreviousStateWelcomeTransitionFailed', () => {
    let states = [
      {state: State.kWelcomeScreen, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.transitionPreviousState().then((state) => {
      assertEquals(state.state, State.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kTransitionFailed);
    });
  });

  test('AbortRmaDefaultUndefined', () => {
    return service.abortRma().then((error) => {
      assertEquals(error, undefined);
    });
  });

  test('SetAbortRmaResultUpdatesResult', () => {
    service.setAbortRmaResult(RmadErrorCode.kRequestInvalid);
    return service.abortRma().then((error) => {
      assertEquals(error.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('GetCurrentOsVersionDefaultUndefined', () => {
    return service.getCurrentOsVersion().then((version) => {
      assertEquals(version, undefined);
    });
  });

  test('SetGetCurrentOsVersionResultUpdatesResult', () => {
    service.setGetCurrentOsVersionResult('1234.56.78');
    return service.getCurrentOsVersion().then((version) => {
      assertEquals(version.version, '1234.56.78');
    });
  });

  test('SetUpdateOsResultTrueUpdatesResult', () => {
    service.setUpdateOsResult(true);

    return service.updateOs().then((result) => {
      assertEquals(result.updateStarted, true);
    });
  });

  test('SetUpdateOsResultFalseUpdatesResult', () => {
    service.setUpdateOsResult(false);

    return service.updateOs().then((result) => {
      assertEquals(result.updateStarted, false);
    });
  });

  test('UpdateOsSkippedOk', () => {
    let states = [
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
      {state: State.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.updateOsSkipped().then((state) => {
      assertEquals(state.state, State.kChooseDestination);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('UpdateOsSkippedWhenRmaNotRequired', () => {
    return service.updateOsSkipped().then((state) => {
      assertEquals(state.state, State.kUnknown);
      assertEquals(state.error, RmadErrorCode.kRmaNotRequired);
    });
  });

  test('UpdateOsSkippedWrongStateFails', () => {
    let states = [
      {state: State.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: State.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.updateOsSkipped().then((state) => {
      assertEquals(state.state, State.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('SetSameOwnerOk', () => {
    let states = [
      {state: State.kChooseDestination, error: RmadErrorCode.kOk},
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.setSameOwner().then((state) => {
      assertEquals(state.state, State.kUpdateOs);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('SetSameOwnerWhenRmaNotRequired', () => {
    return service.setSameOwner().then((state) => {
      assertEquals(state.state, State.kUnknown);
      assertEquals(state.error, RmadErrorCode.kRmaNotRequired);
    });
  });

  test('SetSameOwnerWrongStateFails', () => {
    let states = [
      {state: State.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.setSameOwner().then((state) => {
      assertEquals(state.state, State.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('SetDifferentOwnerOk', () => {
    let states = [
      {state: State.kChooseDestination, error: RmadErrorCode.kOk},
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.setDifferentOwner().then((state) => {
      assertEquals(state.state, State.kUpdateOs);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('SetDifferentOwnerWrongStateFails', () => {
    let states = [
      {state: State.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.setDifferentOwner().then((state) => {
      assertEquals(state.state, State.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('ChooseManuallyDisableWriteProtectOk', () => {
    let states = [
      {state: State.kChooseWriteProtectDisableMethod, error: RmadErrorCode.kOk},
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.chooseManuallyDisableWriteProtect().then((state) => {
      assertEquals(state.state, State.kUpdateOs);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('ChooseManuallyDisableWriteProtectWrongStateFails', () => {
    let states = [
      {state: State.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.chooseManuallyDisableWriteProtect().then((state) => {
      assertEquals(state.state, State.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('ChooseRsuDisableWriteProtectOk', () => {
    let states = [
      {state: State.kChooseWriteProtectDisableMethod, error: RmadErrorCode.kOk},
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.chooseRsuDisableWriteProtect().then((state) => {
      assertEquals(state.state, State.kUpdateOs);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('ChooseRsuDisableWriteProtectWrongStateFails', () => {
    let states = [
      {state: State.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.chooseRsuDisableWriteProtect().then((state) => {
      assertEquals(state.state, State.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('GetRsuDisableWriteProtectChallengeUndefined', () => {
    return service.getRsuDisableWriteProtectChallenge().then((serialNumber) => {
      assertEquals(serialNumber, undefined);
    });
  });

  test('SetGetRsuDisableWriteProtectChallengeResultUpdatesResult', () => {
    let expected_challenge = '9876543210';
    service.setGetRsuDisableWriteProtectChallengeResult(expected_challenge);
    return service.getRsuDisableWriteProtectChallenge().then((challenge) => {
      assertEquals(challenge.challenge, expected_challenge);
    });
  });

  test('SetRsuDisableWriteProtectCodeOk', () => {
    let states = [
      {state: State.kEnterRSUWPDisableCode, error: RmadErrorCode.kOk},
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.setRsuDisableWriteProtectCode('ignored').then((state) => {
      assertEquals(state.state, State.kUpdateOs);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('SetRsuDisableWriteProtectCodeWrongStateFails', () => {
    let states = [
      {state: State.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.setRsuDisableWriteProtectCode('ignored').then((state) => {
      assertEquals(state.state, State.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('GetComponentListDefaultEmpty', () => {
    return service.getComponentList().then((components) => {
      assertDeepEquals(components.components, []);
    });
  });

  test('SetGetComponentListResultUpdatesResult', () => {
    let expected_components = [
      {
        component: ComponentType.kKeyboard,
        state: ComponentRepairStatus.kOriginal
      },
      {
        component: ComponentType.kTouchpad,
        state: ComponentRepairStatus.kMissing
      },
    ];
    service.setGetComponentListResult(expected_components);
    return service.getComponentList().then((components) => {
      assertDeepEquals(components.components, expected_components);
    });
  });

  test('SetComponentListOk', () => {
    let components = [
      {
        component: ComponentType.kKeyboard,
        state: ComponentRepairStatus.kOriginal
      },
    ];
    let states = [
      {state: State.kSelectComponents, error: RmadErrorCode.kOk},
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.setComponentList(components).then((state) => {
      assertEquals(state.state, State.kUpdateOs);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('SetComponentListWrongStateFails', () => {
    let components = [
      {
        component: ComponentType.kKeyboard,
        state: ComponentRepairStatus.kOriginal
      },
    ];
    let states = [
      {state: State.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.setComponentList(components).then((state) => {
      assertEquals(state.state, State.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('ReworkMainboardOk', () => {
    let states = [
      {state: State.kSelectComponents, error: RmadErrorCode.kOk},
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.reworkMainboard().then((state) => {
      assertEquals(state.state, State.kUpdateOs);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('ReworkMainboardWrongStateFails', () => {
    let states = [
      {state: State.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.reworkMainboard().then((state) => {
      assertEquals(state.state, State.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('GetWriteProtectDisableCompleteActionDefaultUndefined', () => {
    return service.getWriteProtectDisableCompleteAction().then((res) => {
      assertEquals(undefined, res);
    });
  });

  test('SetGetWriteProtectDisableCompleteStateUpdatesAction', () => {
    service.setGetWriteProtectDisableCompleteAction(
        WriteProtectDisableCompleteAction.kCompleteKeepDeviceOpen);
    return service.getWriteProtectDisableCompleteAction().then((res) => {
      assertEquals(
          WriteProtectDisableCompleteAction.kCompleteKeepDeviceOpen,
          res.action);
    });
  });

  test('ReimageRoFirmwareUpdateCompleteOk', () => {
    let states = [
      {state: State.kUpdateRoFirmware, error: RmadErrorCode.kOk},
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.roFirmwareUpdateComplete().then((state) => {
      assertEquals(state.state, State.kUpdateOs);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('ReimageRoFirmwareUpdateCompleteWrongStateFails', () => {
    let states = [
      {state: State.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: State.kUpdateOs, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.roFirmwareUpdateComplete().then((state) => {
      assertEquals(state.state, State.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('GetRegionListDefaultUndefined', () => {
    return service.getRegionList().then((regions) => {
      assertEquals(regions, undefined);
    });
  });

  test('SetGetRegionListResultUpdatesResult', () => {
    let regionList = ['America', 'Asia', 'Europe'];
    service.setGetRegionListResult(regionList);
    return service.getRegionList().then((regions) => {
      assertDeepEquals(regions.regions, regionList);
    });
  });

  test('GetSkuListDefaultUndefined', () => {
    return service.getSkuList().then((skus) => {
      assertEquals(skus, undefined);
    });
  });

  test('SetGetSkuListResultUpdatesResult', () => {
    let skuList = [1, 202, 33];
    service.setGetSkuListResult(skuList);
    return service.getSkuList().then((skus) => {
      assertDeepEquals(skus.skus, skuList);
    });
  });

  test('GetWhiteLabelListDefaultUndefined', () => {
    return service.getWhiteLabelList().then((whiteLabels) => {
      assertEquals(whiteLabels, undefined);
    });
  });

  test('SetGetWhiteLabelListResultUpdatesResult', () => {
    const whiteLabelList =
        ['White-label 10', 'White-label 0', 'White-label 9999'];
    service.setGetWhiteLabelListResult(whiteLabelList);
    return service.getWhiteLabelList().then((whiteLabels) => {
      assertDeepEquals(whiteLabels.whiteLabels, whiteLabelList);
    });
  });

  test('GetOriginalSerialNumberDefaultUndefined', () => {
    return service.getOriginalSerialNumber().then((serialNumber) => {
      assertEquals(serialNumber, undefined);
    });
  });

  test('SetGetOriginalSerialNumberResultUpdatesResult', () => {
    let expected_serial_number = '123456789';
    service.setGetOriginalSerialNumberResult(expected_serial_number);
    return service.getOriginalSerialNumber().then((serial_number) => {
      assertEquals(serial_number.serialNumber, expected_serial_number);
    });
  });

  test('GetOriginalRegionDefaultUndefined', () => {
    return service.getOriginalRegion().then((region) => {
      assertEquals(region, undefined);
    });
  });

  test('SetGetOriginalRegionResultUpdatesResult', () => {
    let expected_region = 1;
    service.setGetOriginalRegionResult(expected_region);
    return service.getOriginalRegion().then((region) => {
      assertEquals(region.regionIndex, expected_region);
    });
  });

  test('GetOriginalSkuDefaultUndefined', () => {
    return service.getOriginalSku().then((sku) => {
      assertEquals(sku, undefined);
    });
  });

  test('SetGetOriginalSkuResultUpdatesResult', () => {
    let expected_sku = 1;
    service.setGetOriginalSkuResult(expected_sku);
    return service.getOriginalSku().then((sku) => {
      assertEquals(sku.skuIndex, expected_sku);
    });
  });

  test('GetOriginalWhiteLabelDefaultUndefined', () => {
    return service.getOriginalWhiteLabel().then((whiteLabel) => {
      assertEquals(whiteLabel, undefined);
    });
  });

  test('SetGetOriginalRegionResultUpdatesResult', () => {
    const expected_whiteLabel = 1;
    service.setGetOriginalWhiteLabelResult(expected_whiteLabel);
    return service.getOriginalWhiteLabel().then((whiteLabel) => {
      assertEquals(whiteLabel.whiteLabelIndex, expected_whiteLabel);
    });
  });

  test('SetDeviceInformationOk', () => {
    let states = [
      {state: State.kUpdateDeviceInformation, error: RmadErrorCode.kOk},
      {state: State.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.setDeviceInformation('serial number', 1, 2).then((state) => {
      assertEquals(state.state, State.kChooseDestination);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('GetCalibrationComponentList', () => {
    let states = [
      {state: State.kCheckCalibration, error: RmadErrorCode.kOk},
      {state: State.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);
    let expectedCalibrationComponents = [
      /** @type {!CalibrationComponentStatus} */
      ({
        component: ComponentType.kLidAccelerometer,
        status: CalibrationStatus.kCalibrationInProgress,
        progress: 0.5
      }),
      /** @type {!CalibrationComponentStatus} */
      ({
        component: ComponentType.kBaseAccelerometer,
        status: CalibrationStatus.kCalibrationComplete,
        progress: 1.0
      }),
    ];
    service.setGetCalibrationComponentListResult(expectedCalibrationComponents);

    return service.getCalibrationComponentList().then((result) => {
      assertDeepEquals(expectedCalibrationComponents, result.components);
    });
  });

  test('GetCalibrationInstructions', () => {
    let states = [
      {state: State.kCheckCalibration, error: RmadErrorCode.kOk},
      {state: State.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);
    service.setGetCalibrationSetupInstructionsResult(
        CalibrationSetupInstruction
            .kCalibrationInstructionPlaceBaseOnFlatSurface);

    return service.getCalibrationSetupInstructions().then((result) => {
      assertDeepEquals(
          CalibrationSetupInstruction
              .kCalibrationInstructionPlaceBaseOnFlatSurface,
          result.instructions);
    });
  });

  test('StartCalibrationOk', () => {
    let states = [
      {state: State.kCheckCalibration, error: RmadErrorCode.kOk},
      {state: State.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);
    service.setGetCalibrationSetupInstructionsResult(
        CalibrationSetupInstruction
            .kCalibrationInstructionPlaceBaseOnFlatSurface);

    return service.startCalibration(fakeCalibrationComponents).then((state) => {
      assertEquals(state.state, State.kChooseDestination);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('RunCalibrationStepOk', () => {
    let states = [
      {state: State.kSetupCalibration, error: RmadErrorCode.kOk},
      {state: State.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.runCalibrationStep().then((state) => {
      assertEquals(state.state, State.kChooseDestination);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('ContinueCalibrationOk', () => {
    let states = [
      {state: State.kRunCalibration, error: RmadErrorCode.kOk},
      {state: State.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.continueCalibration().then((state) => {
      assertEquals(state.state, State.kChooseDestination);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('CalibrationCompleteOk', () => {
    let states = [
      {state: State.kRunCalibration, error: RmadErrorCode.kOk},
      {state: State.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.calibrationComplete().then((state) => {
      assertEquals(state.state, State.kChooseDestination);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('GetLog', () => {
    let states = [{state: State.kRepairComplete, error: RmadErrorCode.kOk}];
    service.setStates(states);
    const expectedLog = 'fake log';
    service.setGetLogResult(expectedLog);
    return service.getLog().then((res) => {
      assertEquals(expectedLog, res.log);
    });
  });

  test('EndRmaAndRebootOk', () => {
    let states = [
      {state: State.kRepairComplete, error: RmadErrorCode.kOk},
      {state: State.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.endRmaAndReboot().then((state) => {
      assertEquals(state.state, State.kChooseDestination);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('EndRmaAndRebootWhenRmaNotRequired', () => {
    return service.endRmaAndReboot().then((state) => {
      assertEquals(state.state, State.kUnknown);
      assertEquals(state.error, RmadErrorCode.kRmaNotRequired);
    });
  });

  test('EndRmaAndRebootWrongStateFails', () => {
    let states = [
      {state: State.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: State.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.endRmaAndReboot().then((state) => {
      assertEquals(state.state, State.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('EndRmaAndShutdownOk', () => {
    let states = [
      {state: State.kRepairComplete, error: RmadErrorCode.kOk},
      {state: State.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.endRmaAndShutdown().then((state) => {
      assertEquals(state.state, State.kChooseDestination);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('EndRmaAndShutdownWhenRmaNotRequired', () => {
    return service.endRmaAndShutdown().then((state) => {
      assertEquals(state.state, State.kUnknown);
      assertEquals(state.error, RmadErrorCode.kRmaNotRequired);
    });
  });

  test('EndRmaAndShutdownWrongStateFails', () => {
    let states = [
      {state: State.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: State.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.endRmaAndShutdown().then((state) => {
      assertEquals(state.state, State.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('EndRmaAndCutoffBatteryOk', () => {
    let states = [
      {state: State.kRepairComplete, error: RmadErrorCode.kOk},
      {state: State.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.endRmaAndCutoffBattery().then((state) => {
      assertEquals(state.state, State.kChooseDestination);
      assertEquals(state.error, RmadErrorCode.kOk);
    });
  });

  test('EndRmaAndCutoffBatteryWhenRmaNotRequired', () => {
    return service.endRmaAndCutoffBattery().then((state) => {
      assertEquals(state.state, State.kUnknown);
      assertEquals(state.error, RmadErrorCode.kRmaNotRequired);
    });
  });

  test('EndRmaAndCutoffBatteryWrongStateFails', () => {
    let states = [
      {state: State.kWelcomeScreen, error: RmadErrorCode.kOk},
      {state: State.kChooseDestination, error: RmadErrorCode.kOk},
    ];
    service.setStates(states);

    return service.endRmaAndCutoffBattery().then((state) => {
      assertEquals(state.state, State.kWelcomeScreen);
      assertEquals(state.error, RmadErrorCode.kRequestInvalid);
    });
  });

  test('ObserveError', () => {
    /** @type {!ErrorObserverRemote} */
    const errorObserver = /** @type {!ErrorObserverRemote} */ ({
      /**
       * Implements ErrorObserverRemote.onError()
       * @param {!RmadErrorCode} error
       */
      onError(error) {
        assertEquals(error, RmadErrorCode.kRequestInvalid);
      }
    });
    service.observeError(errorObserver);
    return service.triggerErrorObserver(RmadErrorCode.kRequestInvalid, 0);
  });

  test('ObserveOsUpdate', () => {
    /** @type {!OsUpdateObserverRemote} */
    const osUpdateObserver = /** @type {!OsUpdateObserverRemote} */ ({
      /**
       * Implements OsUpdateObserverRemote.onOsUpdateProgressUpdated()
       * @param {!OsUpdateOperation} operation
       * @param {number} progress
       */
      onOsUpdateProgressUpdated(operation, progress) {
        assertEquals(operation, OsUpdateOperation.kDownloading);
        assertEquals(progress, 0.75);
      }
    });
    service.observeOsUpdateProgress(osUpdateObserver);
    return service.triggerOsUpdateObserver(
        OsUpdateOperation.kDownloading, 0.75, 0);
  });

  test('ObserveRoFirmwareUpdate', () => {
    /** @type {!UpdateRoFirmwareObserverRemote} */
    const roFirmwareUpdateObserver =
        /** @type {!UpdateRoFirmwareObserverRemote} */ ({
          /**
           * Implements
           * UpdateRoFirmwareObserver.onUpdateRoFirmwareStatusChanged()
           * @param {!UpdateRoFirmwareStatus} status
           */
          onUpdateRoFirmwareStatusChanged(status) {
            assertEquals(UpdateRoFirmwareStatus.kDownloading, status);
          }
        });
    service.observeRoFirmwareUpdateProgress(roFirmwareUpdateObserver);
    return service.triggerUpdateRoFirmwareObserver(
        UpdateRoFirmwareStatus.kDownloading, 0);
  });

  test('ObserveCalibrationUpdated', () => {
    /** @type {!CalibrationObserverRemote} */
    const calibrationObserver = /** @type {!CalibrationObserverRemote} */ ({
      /**
       * Implements CalibrationObserverRemote.onCalibrationUpdated()
       * @param {!CalibrationComponentStatus} calibrationStatus
       */
      onCalibrationUpdated(calibrationStatus) {
        assertEquals(
            calibrationStatus.component, ComponentType.kBaseAccelerometer);
        assertEquals(
            calibrationStatus.status, CalibrationStatus.kCalibrationComplete);
        assertEquals(calibrationStatus.progress, 0.5);
      }
    });
    service.observeCalibrationProgress(calibrationObserver);
    return service.triggerCalibrationObserver(
        /** @type {!CalibrationComponentStatus} */
        ({
          component: ComponentType.kBaseAccelerometer,
          status: CalibrationStatus.kCalibrationComplete,
          progress: 0.5
        }),
        0);
  });

  test('ObserveCalibrationStepComplete', () => {
    /** @type {!CalibrationObserverRemote} */
    const calibrationObserver = /** @type {!CalibrationObserverRemote} */ ({
      /**
       * Implements CalibrationObserverRemote.onCalibrationUpdated()
       * @param {!CalibrationOverallStatus} status
       */
      onCalibrationStepComplete(status) {
        assertEquals(
            status, CalibrationOverallStatus.kCalibrationOverallComplete);
      }
    });
    service.observeCalibrationProgress(calibrationObserver);
    return service.triggerCalibrationOverallObserver(
        CalibrationOverallStatus.kCalibrationOverallComplete, 0);
  });

  test('ObserveProvisioningUpdated', () => {
    /** @type {!ProvisioningObserverRemote} */
    const provisioningObserver = /** @type {!ProvisioningObserverRemote} */ ({
      /**
       * Implements ProvisioningObserverRemote.onProvisioningUpdated()
       * @param {!ProvisioningStatus} status
       * @param {number} progress
       */
      onProvisioningUpdated(status, progress) {
        assertEquals(status, ProvisioningStatus.kInProgress);
        assertEquals(progress, 0.25);
      }
    });
    service.observeProvisioningProgress(provisioningObserver);
    return service.triggerProvisioningObserver(
        ProvisioningStatus.kInProgress, 0.25, 0);
  });

  test('ObserveHardwareWriteProtectionStateChange', () => {
    /** @type {!HardwareWriteProtectionStateObserverRemote} */
    const hardwareWriteProtectionStateObserver =
        /** @type {!HardwareWriteProtectionStateObserverRemote} */ ({
          /**
           * Implements
           * HardwareWriteProtectionStateObserverRemote.
           *     onHardwareWriteProtectionStateChanged()
           * @param {boolean} enable
           */
          onHardwareWriteProtectionStateChanged(enable) {
            assertEquals(enable, true);
          }
        });
    service.observeHardwareWriteProtectionState(
        hardwareWriteProtectionStateObserver);
    return service.triggerHardwareWriteProtectionObserver(true, 0);
  });

  test('ObservePowerCableStateChange', () => {
    /** @type {!PowerCableStateObserverRemote} */
    const powerCableStateObserver =
        /** @type {!PowerCableStateObserverRemote} */ ({
          /**
           * Implements PowerCableStateObserverRemote.onPowerCableStateChanged()
           * @param {boolean} enable
           */
          onPowerCableStateChanged(enable) {
            assertEquals(enable, true);
          }
        });
    service.observePowerCableState(powerCableStateObserver);
    return service.triggerPowerCableObserver(true, 0);
  });

  test('ObserveHardwareVerificationStatus', () => {
    /** @type {!HardwareVerificationStatusObserverRemote} */
    const observer =
        /** @type {!HardwareVerificationStatusObserverRemote} */ ({
          /**
           * Implements
           * HardwareVerificationStatusObserverRemote.
           *      onHardwareVerificationResult()
           * @param {boolean} is_compliant
           * @param {string} error_message
           */
          onHardwareVerificationResult(is_compliant, error_message) {
            assertEquals(true, is_compliant);
            assertEquals('ok', error_message);
          }
        });
    service.observeHardwareVerificationStatus(observer);
    return service.triggerHardwareVerificationStatusObserver(true, 'ok', 0);
  });

  test('ObserveFinalizationStatus', () => {
    /** @type {!FinalizationObserverRemote} */
    const finalizationObserver =
        /** @type {!FinalizationObserverRemote} */ ({
          /**
           * Implements
           * FinalizationObserverRemote.onFinalizationUpdated()
           * @param {!FinalizationStatus} status
           * @param {number} progress
           */
          onFinalizationUpdated(status, progress) {
            assertEquals(FinalizationStatus.kInProgress, status);
            assertEquals(0.5, progress);
          }
        });
    service.observeFinalizationStatus(finalizationObserver);
    return service.triggerFinalizationObserver(
        FinalizationStatus.kInProgress, 0.5, 0);
  });
}
