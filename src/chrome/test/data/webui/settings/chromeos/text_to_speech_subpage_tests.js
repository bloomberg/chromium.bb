// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://os-settings/chromeos/lazy_load.js';

import {TtsSubpageBrowserProxyImpl, Router, routes} from 'chrome://os-settings/chromeos/os_settings.js';
import {flush} from'chrome://resources/polymer/v3_0/polymer/polymer_bundled.min.js';
import {assertEquals} from '../../chai_assert.js';
import {waitAfterNextRender} from 'chrome://test/test_util.js';
import {getDeepActiveElement} from 'chrome://resources/js/util.m.js';
import {TestBrowserProxy} from '../../test_browser_proxy.js';

/**
 * @implements {TtsSubpageBrowserProxy}
 */
class TestTtsSubpageBrowserProxy extends TestBrowserProxy {
  constructor() {
    super([
      'getAllTtsVoiceData',
      'getTtsExtensions',
      'previewTtsVoice',
      'wakeTtsEngine',
    ]);
  }

  /** @override */
  getAllTtsVoiceData() {
    this.methodCalled('getAllTtsVoiceData');
  }

  /** @override */
  getTtsExtensions() {
    this.methodCalled('getTtsExtensions');
  }

  /** @override */
  previewTtsVoice(previewText, previewVoice) {
    this.methodCalled('previewTtsVoice', [previewText, previewVoice]);
  }

  /** @override */
  wakeTtsEngine() {
    this.methodCalled('wakeTtsEngine');
  }
}

suite('TextToSpeechSubpageTests', function() {
  /** @type {SettingsTtsSubpageElement} */
  let ttsPage = null;

  /** @type {?TestTtsSubpageBrowserProxy} */
  let browserProxy = null;

  function getDefaultPrefs() {
    return {
      intl: {
        accept_languages: {
          key: 'intl.accept_languages',
          type: chrome.settingsPrivate.PrefType.STRING,
          value: '',
        },
      },
      settings: {
        tts: {
          lang_to_voice_name: {
            key: 'prefs.settings.tts.lang_to_voice_name',
            type: chrome.settingsPrivate.PrefType.DICTIONARY,
            value: {},
          },
        },
      },
    };
  }

  setup(function() {
    browserProxy = new TestTtsSubpageBrowserProxy();
    TtsSubpageBrowserProxyImpl.instance_ = browserProxy;

    PolymerTest.clearBody();
    ttsPage = document.createElement('settings-tts-subpage');
    ttsPage.prefs = getDefaultPrefs();
    document.body.appendChild(ttsPage);
    flush();
  });

  teardown(function() {
    ttsPage.remove();
    Router.getInstance().resetRouteForTesting();
  });

  test('Deep link to text to speech rate', async () => {
    const params = new URLSearchParams();
    params.append('settingId', '1503');
    Router.getInstance().navigateTo(
        routes.MANAGE_TTS_SETTINGS, params);

    flush();

    const deepLinkElement =
        ttsPage.$$('#textToSpeechRate').shadowRoot.querySelector('cr-slider');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Text to speech rate slider should be focused for settingId=1503.');
  });

  test('Deep link to text to speech engines', async () => {
    ttsPage.extensions = [{
      name: 'extension1',
      extensionId: 'extension1_id',
      optionsPage: 'extension1_page'
    }];
    flush();

    const params = new URLSearchParams();
    params.append('settingId', '1507');
    Router.getInstance().navigateTo(
        routes.MANAGE_TTS_SETTINGS, params);

    const deepLinkElement = ttsPage.$$('#extensionOptionsButton_0');
    await waitAfterNextRender(deepLinkElement);
    assertEquals(
        deepLinkElement, getDeepActiveElement(),
        'Text to speech engine options should be focused for settingId=1507.');
  });
});
