// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Script that runs on the background page.
 */

goog.provide('ChromeVoxBackground');

goog.require('ChromeVoxState');
goog.require('ConsoleTts');
goog.require('EventStreamLogger');
goog.require('LogStore');
goog.require('Msgs');
goog.require('constants');
goog.require('AbstractEarcons');
goog.require('BrailleBackground');
goog.require('BrailleCaptionsBackground');
goog.require('ChromeVox');
goog.require('ChromeVoxEditableTextBase');
goog.require('ChromeVoxPrefs');
goog.require('CompositeTts');
goog.require('ExtensionBridge');
goog.require('InjectedScriptLoader');
goog.require('NavBraille');
goog.require('QueueMode');
goog.require('TabsApiHandler');
goog.require('TtsBackground');


/**
 * This is the legacy ChromeVox background object.
 */
ChromeVoxBackground = class {
  constructor() {
    ChromeVoxBackground.readPrefs();

    const consoleTts = ConsoleTts.getInstance();
    consoleTts.setEnabled(
        ChromeVoxPrefs.instance.getPrefs()['enableSpeechLogging'] == 'true');

    LogStore.getInstance();

    /**
     * Chrome's actual TTS which knows and cares about pitch, volume, etc.
     * @type {TtsBackground}
     * @private
     */
    this.backgroundTts_ = new TtsBackground();

    /**
     * @type {TtsInterface}
     */
    this.tts = new CompositeTts().add(this.backgroundTts_).add(consoleTts);

    this.addBridgeListener();

    /**
     * The actual Braille service.
     * @type {BrailleBackground}
     * @private
     */
    this.backgroundBraille_ = BrailleBackground.getInstance();

    this.tabsApiHandler_ = new TabsApiHandler();

    // Export globals on ChromeVox.
    ChromeVox.tts = this.tts;
    ChromeVox.braille = this.backgroundBraille_;

    chrome.accessibilityPrivate.onIntroduceChromeVox.addListener(
        this.onIntroduceChromeVox);

    // Set up a message passing system for goog.provide() calls from
    // within the content scripts.
    chrome.extension.onMessage.addListener(function(request, sender, callback) {
      if (request['srcFile']) {
        const srcFile = request['srcFile'];
        InjectedScriptLoader.fetchCode([srcFile], function(code) {
          callback({'code': code[srcFile]});
        });
      }
      return true;
    });

    // Build a regexp to match all allowed urls.
    let matches = [];
    try {
      matches = chrome.runtime.getManifest()['content_scripts'][0]['matches'];
    } catch (e) {
      throw new Error(
          'Unable to find content script matches entry in manifest.');
    }

    // Build one large regexp.
    const matchesRe = new RegExp(matches.join('|'));

    // Inject the content script into all running tabs allowed by the
    // manifest. This block is still necessary because the extension system
    // doesn't re-inject content scripts into already running tabs.
    chrome.windows.getAll({'populate': true}, (windows) => {
      for (let i = 0; i < windows.length; i++) {
        const tabs = windows[i].tabs.filter((tab) => matchesRe.test(tab.url));
        this.injectChromeVoxIntoTabs(tabs);
      }
    });
  }

  /**
   * @param {string} pref
   * @param {*} value
   * @param {boolean} announce
   */
  static setPref(pref, value, announce) {
    if (pref == 'earcons') {
      AbstractEarcons.enabled = !!value;
    } else if (pref == 'sticky' && announce) {
      if (typeof (value) != 'boolean') {
        throw new Error('Unexpected sticky mode value ' + value);
      }
      chrome.accessibilityPrivate.setKeyboardListener(true, !!value);
      new Output()
          .withInitialSpeechProperties(AbstractTts.PERSONALITY_ANNOTATION)
          .withString(
              value ? Msgs.getMsg('sticky_mode_enabled') :
                      Msgs.getMsg('sticky_mode_disabled'))
          .go();
    } else if (pref == 'typingEcho' && announce) {
      let announceStr = '';
      switch (value) {
        case TypingEcho.CHARACTER:
          announceStr = Msgs.getMsg('character_echo');
          break;
        case TypingEcho.WORD:
          announceStr = Msgs.getMsg('word_echo');
          break;
        case TypingEcho.CHARACTER_AND_WORD:
          announceStr = Msgs.getMsg('character_and_word_echo');
          break;
        case TypingEcho.NONE:
          announceStr = Msgs.getMsg('none_echo');
          break;
        default:
          break;
      }
      if (announceStr) {
        new Output()
            .withInitialSpeechProperties(AbstractTts.PERSONALITY_ANNOTATION)
            .withString(announceStr)
            .go();
      }
    } else if (pref == 'brailleCaptions') {
      BrailleCaptionsBackground.setActive(!!value);
    } else if (pref == 'position') {
      ChromeVox.position =
          /** @type {Object<string, constants.Point>} */ (JSON.parse(
              /** @type {string} */ (value)));
    }
    window['prefs'].setPref(pref, value);
    ChromeVoxBackground.readPrefs();
  }

  /**
   * Read and apply preferences that affect the background context.
   */
  static readPrefs() {
    const prefs = ChromeVoxPrefs.instance.getPrefs();
    ChromeVoxEditableTextBase.useIBeamCursor =
        (prefs['useIBeamCursor'] == 'true');
    ChromeVox.isStickyPrefOn = (prefs['sticky'] == 'true');
  }

  /**
   * Inject ChromeVox into a tab.
   * @param {Array<Tab>} tabs The tab where ChromeVox scripts should be
   *     injected.
   */
  injectChromeVoxIntoTabs(tabs) {
    let listOfFiles;

    // These lists of files must match the content_scripts section in
    // the manifest files.
    if (COMPILED) {
      listOfFiles = ['chromeVoxChromePageScript.js'];
    } else {
      listOfFiles = [
        'closure/closure_preinit.js', 'closure/base.js', 'deps.js',
        'chromevox/injected/loader.js'
      ];
    }

    const stageTwo = function(code) {
      for (let i = 0, tab; tab = tabs[i]; i++) {
        window.console.log('Injecting into ' + tab.id, tab);
        let sawError = false;

        /**
         * A helper function which executes code.
         * @param {string} code The code to execute.
         */
        const executeScript = goog.bind(function(code) {
          chrome.tabs.executeScript(
              tab.id, {code, 'allFrames': true}, goog.bind(function() {
                if (!chrome.extension.lastError) {
                  return;
                }
                if (sawError) {
                  return;
                }
                sawError = true;
                console.error('Could not inject into tab', tab);
                this.tts.speak(
                    'Error starting ChromeVox for ' + tab.title + ', ' +
                        tab.url,
                    QueueMode.QUEUE);
              }, this));
        }, this);

        // There is a scenario where two copies of the content script can get
        // loaded into the same tab on browser startup - one automatically and
        // one because the background page injects the content script into every
        // tab on startup. To work around potential bugs resulting from this,
        // ChromeVox exports a global function called disableChromeVox() that
        // can be used here to disable any existing running instance before we
        // inject a new instance of the content script into this tab.
        //
        // It's harmless if there wasn't a copy of ChromeVox already running.
        //
        // Also, set some variables so that Closure deps work correctly and so
        // that ChromeVox knows not to announce feedback as if a page just
        // loaded.
        executeScript(
            'try { window.disableChromeVox(); } catch(e) { }\n' +
            'window.INJECTED_AFTER_LOAD = true;\n' +
            'window.CLOSURE_NO_DEPS = true\n');

        // Now inject the ChromeVox content script code into the tab.
        listOfFiles.forEach(function(file) {
          executeScript(code[file]);
        });
      }
    };

    // We use fetchCode instead of chrome.extensions.executeFile because
    // executeFile doesn't propagate the file name to the content script
    // which means that script is not visible in Dev Tools.
    InjectedScriptLoader.fetchCode(listOfFiles, stageTwo);
  }

  /**
   * Called when a TTS message is received from a page content script.
   * @param {Object} msg The TTS message.
   */
  onTtsMessage(msg) {
    if (msg['action'] == 'speak') {
      // The only caller sending this message is a ChromeVox Classic api client.
      // Disallow empty strings.
      if (msg['text'] == '') {
        return;
      }

      this.tts.speak(
          msg['text'],
          /** QueueMode */ msg['queueMode'], msg['properties']);
    } else if (msg['action'] == 'stop') {
      this.tts.stop();
    } else if (msg['action'] == 'increaseOrDecrease') {
      this.tts.increaseOrDecreaseProperty(msg['property'], msg['increase']);
      const property = msg['property'];
      const engine = this.backgroundTts_;
      const valueAsPercent =
          Math.round(this.backgroundTts_.propertyToPercentage(property) * 100);
      let announcement;
      switch (msg['property']) {
        case AbstractTts.RATE:
          announcement = Msgs.getMsg('announce_rate', [valueAsPercent]);
          break;
        case AbstractTts.PITCH:
          announcement = Msgs.getMsg('announce_pitch', [valueAsPercent]);
          break;
        case AbstractTts.VOLUME:
          announcement = Msgs.getMsg('announce_volume', [valueAsPercent]);
          break;
      }
      if (announcement) {
        this.tts.speak(
            announcement, QueueMode.FLUSH, AbstractTts.PERSONALITY_ANNOTATION);
      }
    } else if (msg['action'] == 'cyclePunctuationEcho') {
      this.tts.speak(
          Msgs.getMsg(this.backgroundTts_.cyclePunctuationEcho()),
          QueueMode.FLUSH);
    }
  }

  /**
   * Listen for connections from our content script bridges, and dispatch the
   * messages to the proper destination.
   */
  addBridgeListener() {
    ExtensionBridge.addMessageListener(goog.bind(function(msg, port) {
      const target = msg['target'];
      const action = msg['action'];

      switch (target) {
        case 'TTS':
          if (msg['startCallbackId'] != undefined) {
            msg['properties']['startCallback'] = function(opt_cleanupOnly) {
              port.postMessage({
                'message': 'TTS_CALLBACK',
                'cleanupOnly': opt_cleanupOnly,
                'id': msg['startCallbackId']
              });
            };
          }
          if (msg['endCallbackId'] != undefined) {
            msg['properties']['endCallback'] = function(opt_cleanupOnly) {
              port.postMessage({
                'message': 'TTS_CALLBACK',
                'cleanupOnly': opt_cleanupOnly,
                'id': msg['endCallbackId']
              });
            };
          }
          try {
            this.onTtsMessage(msg);
          } catch (err) {
            console.log(err);
          }
          break;
      }
    }, this));
  }

  /**
   * Handles the onIntroduceChromeVox event.
   */
  onIntroduceChromeVox() {
    ChromeVox.tts.speak(
        Msgs.getMsg('chromevox_intro'), QueueMode.QUEUE,
        {doNotInterrupt: true});
    ChromeVox.braille.write(NavBraille.fromText(Msgs.getMsg('intro_brl')));
  }

  /**
   * Gets the voice currently used by ChromeVox when calling tts.
   * @return {string}
   */
  getCurrentVoice() {
    return this.backgroundTts_.currentVoice;
  }

  /**
   * Initializes classic background object.
   */
  static init() {
    // Create the background page object and export a function window['speak']
    // so that other background pages can access it. Also export the prefs
    // object for access by the options page.
    const background = new ChromeVoxBackground();

    // TODO: this needs to be cleaned up (move to init?).
    window['speak'] = goog.bind(background.tts.speak, background.tts);
    ChromeVoxState.backgroundTts = background.backgroundTts_;
    // Export the prefs object for access by the options page.
    window['prefs'] = ChromeVoxPrefs.instance;
    // Export the braille translator manager for access by the options page.
    window['braille_translator_manager'] =
        background.backgroundBraille_.getTranslatorManager();
    window['getCurrentVoice'] = background.getCurrentVoice.bind(background);
  }
};
