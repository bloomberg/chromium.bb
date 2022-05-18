// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {$} from 'chrome://resources/js/util.m.js';

import {FEEDBACK_LANDING_PAGE, FEEDBACK_LANDING_PAGE_TECHSTOP, FEEDBACK_LEGAL_HELP_URL, FEEDBACK_PRIVACY_POLICY_URL, FEEDBACK_TERM_OF_SERVICE_URL, openUrlInAppWindow} from './feedback_util.js';
import {domainQuestions} from './questionnaire.js';
import {questionnaireBegin} from './questionnaire.js';
import {questionnaireNotification} from './questionnaire.js';
import {takeScreenshot} from './take_screenshot.js';

const formOpenTime: number = new Date().getTime();

const dialogArgs: string = chrome.getVariableValue('dialogArguments');

/**
 * The object will be manipulated by feedbackHelper
 */
let feedbackInfo: chrome.feedbackPrivate.FeedbackInfo = {
  assistantDebugInfoAllowed: false,
  attachedFile: undefined,
  attachedFileBlobUuid: undefined,
  categoryTag: undefined,
  description: '...',
  descriptionPlaceholder: undefined,
  email: undefined,
  flow: chrome.feedbackPrivate.FeedbackFlow.REGULAR,
  fromAssistant: false,
  includeBluetoothLogs: false,
  pageUrl: undefined,
  sendHistograms: undefined,
  systemInformation: [],
  useSystemWindowFrame: false,
};


class FeedbackHelper {
  getSystemInformation(): Promise<chrome.feedbackPrivate.SystemInformation[]> {
    return new Promise(
        resolve => chrome.feedbackPrivate.getSystemInformation(resolve));
  }

  getUserEmail(): Promise<string> {
    return new Promise(resolve => chrome.feedbackPrivate.getUserEmail(resolve));
  }

  sendFeedbackReport(useSystemInfo: boolean) {
    const ID = Math.round(Date.now() / 1000);
    const FLOW = feedbackInfo.flow;

    chrome.feedbackPrivate.sendFeedback(
        feedbackInfo, useSystemInfo, formOpenTime,
        function(result, landingPageType) {
          if (result === chrome.feedbackPrivate.Status.SUCCESS) {
            if (FLOW !== chrome.feedbackPrivate.FeedbackFlow.LOGIN &&
                landingPageType !==
                    chrome.feedbackPrivate.LandingPageType.NO_LANDING_PAGE) {
              const landingPage = landingPageType ===
                      chrome.feedbackPrivate.LandingPageType.NORMAL ?
                  FEEDBACK_LANDING_PAGE :
                  FEEDBACK_LANDING_PAGE_TECHSTOP;
              window.open(landingPage, '_blank');
            }
          } else {
            console.warn(
                'Feedback: Report for request with ID ' + ID +
                ' will be sent later.');
          }
          scheduleWindowClose();
        });
  }

  // Send a message to show the WebDialog
  showDialog() {
    chrome.send('showDialog');
  }

  // Send a message to close the WebDialog
  closeDialog() {
    chrome.send('dialogClose');
  }

  // <if expr="chromeos_ash">
  showAssistantLogsInfo() {
    chrome.send('showAssistantLogsInfo');
  }

  showBluetoothLogsInfo() {
    chrome.send('showBluetoothLogsInfo');
  }
  // </if>

  showSystemInfo() {
    chrome.send('showSystemInfo');
  }

  showMetrics() {
    chrome.send('showMetrics');
  }
}

const feedbackHelper: FeedbackHelper = new FeedbackHelper();

const MAX_ATTACH_FILE_SIZE: number = 3 * 1024 * 1024;

const MAX_SCREENSHOT_WIDTH: number = 100;

let attachedFileBlob: Blob|null = null;

/**
 * Which questions have been appended to the issue description text area.
 */
const appendedQuestions: {[key: string]: boolean} = {};

/**
 * Builds a RegExp that matches one of the given words. Each word has to match
 * at word boundary and is not at the end of the tested string. For example,
 * the word "SIM" would match the string "I have a sim card issue" but not
 * "I have a simple issue" nor "I have a sim" (because the user might not have
 * finished typing yet).
 * @param words The words to match.
 */
function buildWordMatcher(words: string[]): RegExp {
  return new RegExp(
      words.map((word) => '\\b' + word + '\\b[^$]').join('|'), 'i');
}

/**
 * Regular expression to check for all variants of blu[e]toot[h] with or without
 * space between the words; for BT when used as an individual word, or as two
 * individual characters, and for BLE, BlueZ, and Floss when used as an
 * individual word. Case insensitive matching.
 */
const btRegEx: RegExp = new RegExp(
    'blu[e]?[ ]?toot[h]?|\\bb[ ]?t\\b|\\bble\\b|\\bfloss\\b|\\bbluez\\b', 'i');

/**
 * Regular expression to check for wifi-related keywords.
 */
const wifiRegEx: RegExp =
    buildWordMatcher(['wifi', 'wi-fi', 'internet', 'network', 'hotspot']);

/**
 * Regular expression to check for cellular-related keywords.
 */
const cellularRegEx: RegExp = buildWordMatcher([
  '2G',     '3G',      '4G',  '5G',   'LTE',  'UMTS',     'SIM',     'eSIM',
  'mmWave', 'mobile',  'APN', 'IMEI', 'IMSI', 'eUICC',    'carrier', 'T.Mobile',
  'TMO',    'Verizon', 'VZW', 'AT&T', 'MVNO', 'pin.lock', 'cellular'
]);

/**
 * Regular expression to check for all strings indicating that a user can't
 * connect to a HID or Audio device. This is also a likely indication of a
 * Bluetooth related issue.
 * Sample strings this will match:
 * "I can't connect the speaker!",
 * "The keyboard has connection problem."
 */
const cantConnectRegEx: RegExp = new RegExp(
    '((headphone|keyboard|mouse|speaker)((?!(connect|pair)).*)(connect|pair))' +
        '|((connect|pair).*(headphone|keyboard|mouse|speaker))',
    'i');

/**
 * Regular expression to check for "tether" or "tethering". Case insensitive
 * matching.
 */
const tetherRegEx: RegExp = new RegExp('tether(ing)?', 'i');

/**
 * Regular expression to check for "Smart (Un)lock" or "Easy (Un)lock" with or
 * without space between the words. Case insensitive matching.
 */
const smartLockRegEx: RegExp = new RegExp('(smart|easy)[ ]?(un)?lock', 'i');

/**
 * Regular expression to check for keywords related to Nearby Share like
 * "nearby (share)" or "phone (hub)".
 * Case insensitive matching.
 */
const nearbyShareRegEx: RegExp = new RegExp('nearby|phone', 'i');

/**
 * Regular expression to check for keywords related to Fast Pair like
 * "fast pair".
 * Case insensitive matching.
 */
const fastPairRegEx: RegExp = new RegExp('fast[ ]?pair', 'i');

/**
 * Regular expression to check for Bluetooth device specific keywords.
 */
const btDeviceRegEx =
    buildWordMatcher(['apple', 'allegro', 'pixelbud', 'microsoft', 'sony']);

/**
 * Reads the selected file when the user selects a file.
 * @param fileSelectedEvent The onChanged event for the file input box.
 */
function onFileSelected(fileSelectedEvent: Event) {
  // <if expr="chromeos_ash">
  // This is needed on CrOS. Otherwise, the feedback window will stay behind
  // the Chrome window.
  feedbackHelper.showDialog();
  // </if>

  const file = (fileSelectedEvent.target as HTMLInputElement).files![0];
  if (!file) {
    // User canceled file selection.
    attachedFileBlob = null;
    return;
  }

  if (file.size > MAX_ATTACH_FILE_SIZE) {
    $('attach-error').hidden = false;

    // Clear our selected file.
    ($('attach-file') as HTMLInputElement).value = '';
    attachedFileBlob = null;
    return;
  }

  attachedFileBlob = file.slice();
}

/**
 * Called when user opens the file dialog. Hide $('attach-error') before file
 * dialog is open to prevent a11y bug https://crbug.com/1020047
 */
function onOpenFileDialog() {
  $('attach-error').hidden = true;
}

/**
 * Clears the file that was attached to the report with the initial request.
 * Instead we will now show the attach file button in case the user wants to
 * attach another file.
 */
function clearAttachedFile() {
  $('custom-file-container').hidden = true;
  attachedFileBlob = null;
  feedbackInfo.attachedFile = undefined;
  $('attach-file').hidden = false;
}

/**
 * Sets up the event handlers for the given |anchorElement|.
 * @param anchorElement The <a> html element.
 * @param url The destination URL for the link.
 * @param useAppWindow true if the URL should be opened inside a new App Window,
 *     false if it should be opened in a new tab.
 */
function setupLinkHandlers(
    anchorElement: HTMLElement, url: string, useAppWindow: boolean) {
  anchorElement.onclick = function(e) {
    e.preventDefault();
    if (useAppWindow) {
      openUrlInAppWindow(url);
    } else {
      window.open(url, '_blank');
    }
  };

  anchorElement.onauxclick = function(e) {
    e.preventDefault();
  };
}

// <if expr="chromeos_ash">
/**
 * Opens a new window with chrome://slow_trace, downloading performance data.
 */
function openSlowTraceWindow() {
  window.open('chrome://slow_trace/tracing.zip#' + feedbackInfo.traceId);
}
// </if>

/**
 * Checks if any keywords related to bluetooth have been typed. If they are,
 * we show the bluetooth logs option, otherwise hide it.
 * @param inputEvent The input event for the description textarea.
 */
function checkForSendBluetoothLogs(inputEvent: Event) {
  const value = (inputEvent.target as HTMLInputElement).value;
  const isRelatedToBluetooth = btRegEx.test(value) ||
      cantConnectRegEx.test(value) || tetherRegEx.test(value) ||
      smartLockRegEx.test(value) || nearbyShareRegEx.test(value) ||
      fastPairRegEx.test(value) || btDeviceRegEx.test(value);
  $('bluetooth-checkbox-container').hidden = !isRelatedToBluetooth;
}

/**
 * Checks if any keywords have associated questionnaire in a domain. If so,
 * we append the questionnaire in $('description-text').
 * @param inputEvent The input event for the description textarea.
 */
function checkForShowQuestionnaire(inputEvent: Event) {
  const toAppend = [];

  // Match user-entered description before the questionnaire to reduce false
  // positives due to matching the questionnaire questions and answers.
  const value = (inputEvent.target as HTMLInputElement).value;
  const questionnaireBeginPos = value.indexOf(questionnaireBegin);
  const matchedText = questionnaireBeginPos >= 0 ?
      value.substring(0, questionnaireBeginPos) :
      value;

  if (btRegEx.test(matchedText)) {
    toAppend.push(...domainQuestions['bluetooth']);
  }

  if (wifiRegEx.test(matchedText)) {
    toAppend.push(...domainQuestions['wifi']);
  }

  if (cellularRegEx.test(matchedText)) {
    toAppend.push(...domainQuestions['cellular']);
  }

  if (toAppend.length === 0) {
    return;
  }

  const textarea = $('description-text') as HTMLTextAreaElement;
  const savedCursor = textarea.selectionStart;
  if (Object.keys(appendedQuestions).length === 0) {
    textarea.value += '\n\n' + questionnaireBegin + '\n';
    $('questionnaire-notification').textContent = questionnaireNotification;
  }

  for (const question of toAppend) {
    if (question in appendedQuestions) {
      continue;
    }

    textarea.value += '* ' + question + ' \n';
    appendedQuestions[question] = true;
  }

  // After appending text, the web engine automatically moves the cursor to the
  // end of the appended text, so we need to move the cursor back to where the
  // user was typing before.
  textarea.selectionEnd = savedCursor;
}

/**
 * Updates the description-text box based on whether it was valid.
 * If invalid, indicate an error to the user. If valid, remove indication of the
 * error.
 */
function updateDescription(wasValid: boolean) {
  // Set visibility of the alert text for users who don't use a screen
  // reader.
  $('description-empty-error').hidden = wasValid;

  // Change the textarea's aria-labelled by to ensure the screen reader does
  // (or doesn't) read the error, as appropriate.
  // If it does read the error, it should do so _before_ it reads the normal
  // description.
  const description = $('description-text');
  description.setAttribute(
      'aria-labelledby',
      (wasValid ? '' : 'description-empty-error ') + 'free-form-text');
  // Indicate whether input is valid.
  description.setAttribute('aria-invalid', String(!wasValid));
  if (!wasValid) {
    // Return focus to field so user can correct error.
    description.focus();
  }

  // We may have added or removed a line of text, so make sure the app window
  // is the right size.
  resizeAppWindow();
}

/**
 * Sends the report; after the report is sent, we need to be redirected to
 * the landing page, but we shouldn't be able to navigate back, hence
 * we open the landing page in a new tab and sendReport closes this tab.
 * @return Whether the report was sent.
 */
function sendReport(): boolean {
  const textarea = $('description-text') as HTMLTextAreaElement;
  if (textarea.value.length === 0) {
    updateDescription(false);
    return false;
  }
  // This isn't strictly necessary, since if we get past this point we'll
  // succeed, but for future-compatibility (and in case we later add more
  // failure cases after this), re-hide the alert and reset the aria label.
  updateDescription(true);

  // Prevent double clicking from sending additional reports.
  ($('send-report-button') as HTMLButtonElement).disabled = true;
  if (!feedbackInfo.attachedFile && attachedFileBlob) {
    feedbackInfo.attachedFile = {
      name: ($('attach-file') as HTMLInputElement).value,
      data: attachedFileBlob,
    };
  }

  const consentCheckboxValue: boolean =
      ($('consent-checkbox') as HTMLInputElement).checked;
  feedbackInfo.systemInformation = [
    {
      key: 'feedbackUserCtlConsent',
      value: String(consentCheckboxValue),
    },
  ];

  feedbackInfo.description = textarea.value;
  feedbackInfo.pageUrl = ($('page-url-text') as HTMLInputElement).value;
  feedbackInfo.email = ($('user-email-drop-down') as HTMLSelectElement).value;

  let useSystemInfo = false;
  let useHistograms = false;
  const checkbox = $('sys-info-checkbox') as HTMLInputElement | null;
  if (checkbox != null && checkbox.checked) {
    // Send histograms along with system info.
    useHistograms = true;
    useSystemInfo = true;
  }

  // <if expr="chromeos_ash">
  const assistantCheckbox =
      $('assistant-info-checkbox') as HTMLInputElement | null;
  if (assistantCheckbox != null && assistantCheckbox.checked &&
      !$('assistant-checkbox-container').hidden) {
    // User consent to link Assistant debug info on Assistant server.
    feedbackInfo.assistantDebugInfoAllowed = true;
  }

  const bluetoothCheckbox =
      $('bluetooth-logs-checkbox') as HTMLInputElement | null;
  if (bluetoothCheckbox != null && bluetoothCheckbox.checked &&
      !$('bluetooth-checkbox-container').hidden) {
    feedbackInfo.sendBluetoothLogs = true;
    feedbackInfo.categoryTag = 'BluetoothReportWithLogs';
  }

  const performanceCheckbox =
      $('performance-info-checkbox') as HTMLInputElement | null;
  if (performanceCheckbox == null || !performanceCheckbox.checked) {
    feedbackInfo.traceId = undefined;
  }
  // </if>

  feedbackInfo.sendHistograms = useHistograms;

  if (($('screenshot-checkbox') as HTMLInputElement).checked) {
    // The user is okay with sending the screenshot and tab titles.
    feedbackInfo.sendTabTitles = true;
  } else {
    // The user doesn't want to send the screenshot, so clear it.
    feedbackInfo.screenshot = undefined;
  }

  let productId: number|undefined = parseInt('' + feedbackInfo.productId, 10);
  if (isNaN(productId)) {
    // For apps that still use a string value as the |productId|, we must clear
    // that value since the API uses an integer value, and a conflict in data
    // types will cause the report to fail to be sent.
    productId = undefined;
  }
  feedbackInfo.productId = productId;

  // Request sending the report, show the landing page (if allowed)
  feedbackHelper.sendFeedbackReport(useSystemInfo);

  return true;
}

/**
 * Click listener for the cancel button.
 */
function cancel(e: Event) {
  e.preventDefault();
  scheduleWindowClose();
}

// <if expr="chromeos_ash">
/**
 * Update the page when performance feedback state is changed.
 */
function performanceFeedbackChanged() {
  const screenshotCheckbox = $('screenshot-checkbox') as HTMLInputElement;
  const fileInput = $('attach-file') as HTMLInputElement;

  if (($('performance-info-checkbox') as HTMLInputElement).checked) {
    fileInput.disabled = true;
    fileInput.checked = false;

    screenshotCheckbox.disabled = true;
    screenshotCheckbox.checked = false;
  } else {
    fileInput.disabled = false;
    screenshotCheckbox.disabled = false;
  }
}
// </if>

function resizeAppWindow() {
  // TODO(crbug.com/1167223): The UI is now controlled by a WebDialog delegate
  // which is set to not resizable for now. If needed, a message handler can
  // be added to respond to resize request.
}

/**
 * Close the window after 100ms delay.
 */
function scheduleWindowClose() {
  setTimeout(function() {
    feedbackHelper.closeDialog();
  }, 100);
}

/**
 * Initializes our page.
 * Flow:
 * .) DOMContent Loaded        -> . Request feedbackInfo object
 *                                . Setup page event handlers
 * .) Feedback Object Received -> . take screenshot
 *                                . request email
 *                                . request System info
 *                                . request i18n strings
 * .) Screenshot taken         -> . Show Feedback window.
 */
function initialize() {
  // apply received feedback info object.
  function applyData(feedbackInfo: chrome.feedbackPrivate.FeedbackInfo) {
    if (feedbackInfo.includeBluetoothLogs) {
      assert(
          feedbackInfo.flow ===
          chrome.feedbackPrivate.FeedbackFlow.GOOGLE_INTERNAL);
      $('description-text')
          .addEventListener('input', checkForSendBluetoothLogs);
    }

    if (feedbackInfo.showQuestionnaire) {
      assert(
          feedbackInfo.flow ===
          chrome.feedbackPrivate.FeedbackFlow.GOOGLE_INTERNAL);
      $('description-text')
          .addEventListener('input', checkForShowQuestionnaire);
    }

    if ($('assistant-checkbox-container') != null &&
        feedbackInfo.flow ===
            chrome.feedbackPrivate.FeedbackFlow.GOOGLE_INTERNAL &&
        feedbackInfo.fromAssistant) {
      $('assistant-checkbox-container').hidden = false;
    }

    $('description-text').textContent = feedbackInfo.description;
    if (feedbackInfo.descriptionPlaceholder) {
      ($('description-text') as HTMLTextAreaElement).placeholder =
          feedbackInfo.descriptionPlaceholder;
    }
    if (feedbackInfo.pageUrl) {
      ($('page-url-text') as HTMLInputElement).value = feedbackInfo.pageUrl;
    }

    takeScreenshot(function(screenshotCanvas) {
      // We've taken our screenshot, show the feedback page without any
      // further delay.
      window.requestAnimationFrame(function() {
        resizeAppWindow();
      });

      feedbackHelper.showDialog();

      // Allow feedback to be sent even if the screenshot failed.
      if (!screenshotCanvas) {
        const checkbox = $('screenshot-checkbox') as HTMLInputElement;
        checkbox.disabled = true;
        checkbox.checked = false;
        return;
      }

      screenshotCanvas.toBlob(function(blob) {
        const image = $('screenshot-image') as HTMLImageElement;
        image.src = URL.createObjectURL(blob!);
        // Only set the alt text when the src url is available, otherwise we'd
        // get a broken image picture instead. crbug.com/773985.
        image.alt = 'screenshot';
        image.classList.toggle(
            'wide-screen', image.width > MAX_SCREENSHOT_WIDTH);
        feedbackInfo.screenshot = blob!;
      });
    });

    feedbackHelper.getUserEmail().then(function(email) {
      // Never add an empty option.
      if (!email) {
        return;
      }
      const optionElement = document.createElement('option');
      optionElement.value = email;
      optionElement.text = email;
      optionElement.selected = true;
      // Make sure the "Report anonymously" option comes last.
      $('user-email-drop-down')
          .insertBefore(optionElement, $('anonymous-user-option'));

      // Now we can unhide the user email section:
      $('user-email').hidden = false;
    });

    // An extension called us with an attached file.
    if (feedbackInfo.attachedFile) {
      $('attached-filename-text').textContent = feedbackInfo.attachedFile.name;
      attachedFileBlob = feedbackInfo.attachedFile.data!;
      $('custom-file-container').hidden = false;
      $('attach-file').hidden = true;
    }

    // No URL, file attachment for login screen feedback.
    if (feedbackInfo.flow === chrome.feedbackPrivate.FeedbackFlow.LOGIN) {
      $('page-url').hidden = true;
      $('attach-file-container').hidden = true;
      $('attach-file-note').hidden = true;
    }

    // <if expr="chromeos_ash">
    if (feedbackInfo.traceId && ($('performance-info-area'))) {
      $('performance-info-area').hidden = false;
      ($('performance-info-checkbox') as HTMLInputElement).checked = true;
      performanceFeedbackChanged();
      $('performance-info-link').onclick = openSlowTraceWindow;
    }
    // </if>

    const sysInfoUrlElement = $('sys-info-url');
    if (sysInfoUrlElement) {
      // Opens a new window showing the full anonymized system+app
      // information.
      sysInfoUrlElement.onclick = function(e) {
        e.preventDefault();

        feedbackHelper.showSystemInfo();
      };

      sysInfoUrlElement.onauxclick = function(e) {
        e.preventDefault();
      };
    }

    const histogramUrlElement = $('histograms-url');
    if (histogramUrlElement) {
      histogramUrlElement.onclick = function(e) {
        e.preventDefault();

        feedbackHelper.showMetrics();
      };

      histogramUrlElement.onauxclick = function(e) {
        e.preventDefault();
      };
    }

    // The following URLs don't open on login screen, so hide them.
    // TODO(crbug.com/1116383): Find a solution to display them properly.
    // Update: the bluetooth and assistant logs links will work on login
    // screen now. But to limit the scope of this CL, they are still hidden.
    if (feedbackInfo.flow !== chrome.feedbackPrivate.FeedbackFlow.LOGIN) {
      const legalHelpPageUrlElement = $('legal-help-page-url');
      if (legalHelpPageUrlElement) {
        setupLinkHandlers(
            legalHelpPageUrlElement, FEEDBACK_LEGAL_HELP_URL,
            false /* useAppWindow */);
      }

      const privacyPolicyUrlElement = $('privacy-policy-url');
      if (privacyPolicyUrlElement) {
        setupLinkHandlers(
            privacyPolicyUrlElement, FEEDBACK_PRIVACY_POLICY_URL,
            false /* useAppWindow */);
      }

      const termsOfServiceUrlElement = $('terms-of-service-url');
      if (termsOfServiceUrlElement) {
        setupLinkHandlers(
            termsOfServiceUrlElement, FEEDBACK_TERM_OF_SERVICE_URL,
            false /* useAppWindow */);
      }

      // <if expr="chromeos_ash">
      const bluetoothLogsInfoLinkElement = $('bluetooth-logs-info-link');
      if (bluetoothLogsInfoLinkElement) {
        bluetoothLogsInfoLinkElement.onclick = function(e) {
          e.preventDefault();

          feedbackHelper.showBluetoothLogsInfo();

          bluetoothLogsInfoLinkElement.onauxclick = function(e) {
            e.preventDefault();
          };
        };
      }

      const assistantLogsInfoLinkElement = $('assistant-logs-info-link');
      if (assistantLogsInfoLinkElement) {
        assistantLogsInfoLinkElement.onclick = function(e) {
          e.preventDefault();

          feedbackHelper.showAssistantLogsInfo();

          assistantLogsInfoLinkElement.onauxclick = function(e) {
            e.preventDefault();
          };
        };
      }
      // </if>
    }

    // Make sure our focus starts on the description field.
    $('description-text').focus();
  }

  window.addEventListener('DOMContentLoaded', function() {
    if (dialogArgs) {
      feedbackInfo = JSON.parse(dialogArgs);
    }
    applyData(feedbackInfo);

    Object.assign(window, {feedbackInfo, feedbackHelper});

    // Setup our event handlers.
    $('attach-file').addEventListener('change', onFileSelected);
    $('attach-file').addEventListener('click', onOpenFileDialog);
    $('send-report-button').onclick = sendReport;
    $('cancel-button').onclick = cancel;
    $('remove-attached-file').onclick = clearAttachedFile;
    // <if expr="chromeos_ash">
    $('performance-info-checkbox')
        .addEventListener('change', performanceFeedbackChanged);
    // </if>
  });
}

initialize();
