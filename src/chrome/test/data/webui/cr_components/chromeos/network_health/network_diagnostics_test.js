// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://connectivity-diagnostics/strings.m.js';
import 'chrome://resources/cr_components/chromeos/network_health/network_diagnostics_mojo.m.js';
import 'chrome://resources/cr_components/chromeos/network_health/network_diagnostics.m.js';

import {setNetworkDiagnosticsServiceForTesting} from 'chrome://resources/cr_components/chromeos/network_health/mojo_interface_provider.m.js';
import {Icons} from 'chrome://resources/cr_components/chromeos/network_health/network_diagnostics_types.m.js';

import {assertEquals, assertFalse, assertGT, assertNotReached, assertTrue} from '../../../chai_assert.js';
import {flushTasks, isVisible} from '../../../test_util.js';

import {FakeNetworkDiagnostics} from './fake_network_diagnostics_routines.js';
import {getIconFromSrc} from './network_health_test_utils.js';

suite('NetworkDiagnosticsTest', () => {
  /** @type {?NetworkDiagnosticsElement} */
  let networkDiagnostics = null;

  /** @type {?FakeNetworkDiagnostics} */
  let fakeNetworkDiagnostics_ = null;

  setup(() => {
    document.body.innerHTML = '';
    networkDiagnostics = /** @type {!NetworkDiagnosticsElement} */ (
        document.createElement('network-diagnostics'));
    document.body.appendChild(networkDiagnostics);
    fakeNetworkDiagnostics_ = new FakeNetworkDiagnostics();
    setNetworkDiagnosticsServiceForTesting(fakeNetworkDiagnostics_);
    flushTasks();
  });

  teardown(() => {
    networkDiagnostics.remove();
    networkDiagnostics = null;
  });

  /** @param {!chromeos.networkDiagnostics.mojom.RoutineVerdict} verdict */
  function setFakeVerdict(verdict) {
    fakeNetworkDiagnostics_.setFakeVerdict(verdict);
  }

  async function runRoutines() {
    networkDiagnostics.runAllRoutines();
    await flushTasks();
  }

  async function resolveRoutines() {
    fakeNetworkDiagnostics_.resolveRoutines();
    await flushTasks();
  }

  /**
   * Returns a list of the routine groups in the NetworkDiagnostics element.
   * @returns {!NodeList<!RoutineGroupElement>}
   */
  function getRoutineGroups() {
    return /** @type {!NodeList<!RoutineGroupElement>} */ (
        networkDiagnostics.shadowRoot.querySelectorAll('routine-group'));
  }

  /**
   * Checks that all the routine groups match the |running| param
   * @param {boolean} running
   */
  function checkRoutinesRunning(running) {
    for (const group of getRoutineGroups()) {
      const spinner = group.$$('paper-spinner-lite');
      assertEquals(isVisible(spinner), running);
    }
  }

  /**
   * Checks that all the routine groups' verdicts match the |verdict| param
   * @param {!chromeos.networkDiagnostics.mojom.RoutineVerdict} verdict
   */
  function checkRoutinesVerdict(verdict) {
    for (const group of getRoutineGroups()) {
      const icon = group.$$('.routine-icon');
      switch (verdict) {
        case chromeos.networkDiagnostics.mojom.RoutineVerdict.kNoProblem:
          assertEquals(getIconFromSrc(icon.src), Icons.TEST_PASSED);
          break;
        case chromeos.networkDiagnostics.mojom.RoutineVerdict.kNotRun:
          assertEquals(getIconFromSrc(icon.src), Icons.TEST_NOT_RUN);
          break;
        case chromeos.networkDiagnostics.mojom.RoutineVerdict.kProblem:
          assertEquals(getIconFromSrc(icon.src), Icons.TEST_FAILED);
          break;
        default:
          assertNotReached();
      }
    }
  }

  /**
   * Checks that all the routine groups' problems match the |verdict| param
   * @param {!chromeos.networkDiagnostics.mojom.RoutineVerdict} verdict
   */
  async function checkRoutinesProblems(verdict) {
    for (const group of getRoutineGroups()) {
      const container = group.$$('network-health-container');
      const header = container.$$('.container-header');
      header.click();
      await flushTasks();
      assertTrue(group.expanded.valueOf());
      for (const routine of container.querySelectorAll('.routine-container')) {
        const msg = routine.querySelector('#result-msg').textContent.trim();
        const parts = msg.split(': ');

        switch (verdict) {
          case chromeos.networkDiagnostics.mojom.RoutineVerdict.kNoProblem:
            assertEquals(parts.length, 1);
            assertEquals(
                parts[0], networkDiagnostics.i18n('NetworkDiagnosticsPassed'));
            break;
          case chromeos.networkDiagnostics.mojom.RoutineVerdict.kProblem:
            assertGT(parts.length, 0);
            assertEquals(
                parts[0], networkDiagnostics.i18n('NetworkDiagnosticsFailed'));
            // Routine has an associated problem string.
            if (parts.length === 2) {
              assertGT(parts[1].length, 0);
            }
            break;
          case chromeos.networkDiagnostics.mojom.RoutineVerdict.kNotRun:
            assertEquals(parts.length, 1);
            assertEquals(
                parts[0], networkDiagnostics.i18n('NetworkDiagnosticsNotRun'));
            break;
          default:
            assertNotReached();
        }
      }
    }
  }

  // Tests that the routines are not running by default when the element is
  // created.
  test('RoutinesShow', () => {
    checkRoutinesRunning(false);
    for (const group of getRoutineGroups()) {
      assertFalse(group.expanded.valueOf());
    }
  });

  // Tests running the routines with each expect result.
  [chromeos.networkDiagnostics.mojom.RoutineVerdict.kNoProblem,
   chromeos.networkDiagnostics.mojom.RoutineVerdict.kNotRun,
   chromeos.networkDiagnostics.mojom.RoutineVerdict.kProblem]
      .forEach(verdict => {
        test('RoutinesRun' + verdict, async () => {
          setFakeVerdict(verdict);
          await runRoutines();
          checkRoutinesRunning(true);

          await resolveRoutines();
          checkRoutinesRunning(false);
          checkRoutinesVerdict(verdict);
          await checkRoutinesProblems(verdict);
        });
      });
});
