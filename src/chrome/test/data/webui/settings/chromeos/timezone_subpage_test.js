// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {CrSettingsPrefs, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {assert} from 'chrome://resources/js/assert.m.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {flush} from 'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {waitAfterNextRender} from 'chrome://test/test_util.js';

import {assertEquals} from '../../chai_assert.js';

suite('TimezoneSubpageTests', function() {
  /** @type {TimezoneSubpage} */
  let timezoneSubpage = null;

  setup(function() {
    PolymerTest.clearBody();

    const prefElement = document.createElement('settings-prefs');
    document.body.appendChild(prefElement);

    return CrSettingsPrefs.initialized.then(function() {
      timezoneSubpage = document.createElement('timezone-subpage');
      timezoneSubpage.prefs = prefElement.prefs;
      document.body.appendChild(timezoneSubpage);
    });
  });

  teardown(function() {
    timezoneSubpage.remove();
    CrSettingsPrefs.resetForTesting();
    Router.getInstance().resetRouteForTesting();
  });

  test('Timezone autodetect by geolocation radio', async () => {
    const timezoneRadioGroup =
        assert(timezoneSubpage.$$('#timeZoneRadioGroup'));

    // Resolve timezone by geolocation is on.
    timezoneSubpage.setPrefValue(
        'generated.resolve_timezone_by_geolocation_on_off', true);
    flush();
    assertEquals('true', timezoneRadioGroup.selected);

    // Resolve timezone by geolocation is off.
    timezoneSubpage.setPrefValue(
        'generated.resolve_timezone_by_geolocation_on_off', false);
    flush();
    assertEquals('false', timezoneRadioGroup.selected);

    // Set timezone autodetect on by clicking the 'on' radio.
    const timezoneAutodetectOn =
        assert(timezoneSubpage.$$('#timeZoneAutoDetectOn'));
    timezoneAutodetectOn.click();
    assertTrue(timezoneSubpage
                   .getPref('generated.resolve_timezone_by_geolocation_on_off')
                   .value);

    // Turn timezone autodetect off by clicking the 'off' radio.
    const timezoneAutodetectOff =
        assert(timezoneSubpage.$$('#timeZoneAutoDetectOff'));
    timezoneAutodetectOff.click();
    assertFalse(timezoneSubpage
                    .getPref('generated.resolve_timezone_by_geolocation_on_off')
                    .value);
  });

  test('Deep link to time zone setter on subpage', async () => {
    // Resolve timezone by geolocation is on.
    timezoneSubpage.setPrefValue(
        'generated.resolve_timezone_by_geolocation_on_off', true);

    const params = new URLSearchParams();
    params.append('settingId', '1001');
    Router.getInstance().navigateTo(routes.DATETIME_TIMEZONE_SUBPAGE, params);

    const deepLinkElement =
        timezoneSubpage.$$('#timeZoneAutoDetectOn').$$('#button');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Auto set time zone toggle should be focused for settingId=1001.');
  });
});
