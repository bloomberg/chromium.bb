// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is the shared code for the new (Chrome 37) security interstitials. It is
// used for both SSL interstitials and Safe Browsing interstitials.

let expandedDetails = false;
let keyPressState = 0;

/**
 * This allows errors to be skippped by typing a secret phrase into the page.
 * @param {string} e The key that was just pressed.
 */
function handleKeypress(e) {
  // HTTPS errors are serious and should not be ignored. For testing purposes,
  // other approaches are both safer and have fewer side-effects.
  // See https://goo.gl/ZcZixP for more details.
  const BYPASS_SEQUENCE = window.atob('dGhpc2lzdW5zYWZl');
  if (BYPASS_SEQUENCE.charCodeAt(keyPressState) === e.keyCode) {
    keyPressState++;
    if (keyPressState === BYPASS_SEQUENCE.length) {
      sendCommand(SecurityInterstitialCommandId.CMD_PROCEED);
      keyPressState = 0;
    }
  } else {
    keyPressState = 0;
  }
}

/**
 * This appends a piece of debugging information to the end of the warning.
 * When complete, the caller must also make the debugging div
 * (error-debugging-info) visible.
 * @param {string} title  The name of this debugging field.
 * @param {string} value  The value of the debugging field.
 * @param {boolean=} fixedWidth If true, the value field is displayed fixed
 *                              width.
 */
function appendDebuggingField(title, value, fixedWidth) {
  // The values input here are not trusted. Never use innerHTML on these
  // values!
  const spanTitle = document.createElement('span');
  spanTitle.classList.add('debugging-title');
  spanTitle.innerText = title + ': ';

  const spanValue = document.createElement('span');
  spanValue.classList.add('debugging-content');
  if (fixedWidth) {
    spanValue.classList.add('debugging-content-fixed-width');
  }
  spanValue.innerText = value;

  const pElem = document.createElement('p');
  pElem.classList.add('debugging-content');
  pElem.appendChild(spanTitle);
  pElem.appendChild(spanValue);
  $('error-debugging-info').appendChild(pElem);
}

function toggleDebuggingInfo() {
  $('error-debugging-info').classList.toggle(HIDDEN_CLASS);
}

function setupEvents() {
  const overridable = loadTimeData.getBoolean('overridable');
  const interstitialType = loadTimeData.getString('type');
  const ssl = interstitialType === 'SSL';
  const captivePortal = interstitialType === 'CAPTIVE_PORTAL';
  const badClock = ssl && loadTimeData.getBoolean('bad_clock');
  const lookalike = interstitialType === 'LOOKALIKE';
  const billing =
      interstitialType === 'SAFEBROWSING' && loadTimeData.getBoolean('billing');
  const originPolicy = interstitialType === 'ORIGIN_POLICY';
  const blockedInterception = interstitialType === 'BLOCKED_INTERCEPTION';
  const legacyTls = interstitialType == 'LEGACY_TLS';
  const hidePrimaryButton = loadTimeData.getBoolean('hide_primary_button');
  const showRecurrentErrorParagraph = loadTimeData.getBoolean(
    'show_recurrent_error_paragraph');

  if (ssl || originPolicy || blockedInterception || legacyTls) {
    $('body').classList.add(badClock ? 'bad-clock' : 'ssl');
    $('error-code').textContent = loadTimeData.getString('errorCode');
    $('error-code').classList.remove(HIDDEN_CLASS);
  } else if (captivePortal) {
    $('body').classList.add('captive-portal');
  } else if (billing) {
    $('body').classList.add('safe-browsing-billing');
  } else if (lookalike) {
    $('body').classList.add('lookalike-url');
  } else {
    $('body').classList.add('safe-browsing');
    // Override the default theme color.
    document.querySelector('meta[name=theme-color]').setAttribute('content',
      'rgb(206, 52, 38)');
  }

  $('icon').classList.add('icon');

  if (legacyTls) {
    $('icon').classList.add('legacy-tls');
  }

  if (hidePrimaryButton) {
    $('primary-button').classList.add(HIDDEN_CLASS);
  } else {
    $('primary-button').addEventListener('click', function() {
      switch (interstitialType) {
        case 'CAPTIVE_PORTAL':
          sendCommand(SecurityInterstitialCommandId.CMD_OPEN_LOGIN);
          break;

        case 'SSL':
        case 'LEGACY_TLS':
          if (badClock) {
            sendCommand(SecurityInterstitialCommandId.CMD_OPEN_DATE_SETTINGS);
          } else if (overridable) {
            sendCommand(SecurityInterstitialCommandId.CMD_DONT_PROCEED);
          } else {
            sendCommand(SecurityInterstitialCommandId.CMD_RELOAD);
          }
          break;

        case 'SAFEBROWSING':
        case 'ORIGIN_POLICY':
          sendCommand(SecurityInterstitialCommandId.CMD_DONT_PROCEED);
          break;

        case 'LOOKALIKE':
          sendCommand(SecurityInterstitialCommandId.CMD_DONT_PROCEED);
          break;

        default:
          throw 'Invalid interstitial type';
      }
    });
  }

  if (lookalike) {
    const proceedButton = 'proceed-button';
    const dontProceedLink = 'dont-proceed-link';
    $(proceedButton).classList.remove(HIDDEN_CLASS);

    $(proceedButton).textContent = loadTimeData.getString('proceedButtonText');

    $(proceedButton).addEventListener('click', function(event) {
      sendCommand(SecurityInterstitialCommandId.CMD_PROCEED);
    });

    $(dontProceedLink).addEventListener('click', function(event) {
      sendCommand(SecurityInterstitialCommandId.CMD_DONT_PROCEED);
    });
  }

  if (overridable) {
    const overrideElement = billing ? 'proceed-button' : 'proceed-link';
    // Captive portal page isn't overridable.
    $(overrideElement).addEventListener('click', function(event) {
      sendCommand(SecurityInterstitialCommandId.CMD_PROCEED);
    });

    if (ssl) {
      $(overrideElement).classList.add('small-link');
    } else if (billing) {
      $(overrideElement).classList.remove(HIDDEN_CLASS);
      $(overrideElement).textContent =
          loadTimeData.getString('proceedButtonText');
    }
  } else if (!ssl) {
    $('final-paragraph').classList.add(HIDDEN_CLASS);
  }


  if (!ssl || !showRecurrentErrorParagraph) {
    $('recurrent-error-message').classList.add(HIDDEN_CLASS);
  } else {
    $('body').classList.add('showing-recurrent-error-message');
  }

  if ($('diagnostic-link')) {
    $('diagnostic-link').addEventListener('click', function(event) {
      sendCommand(SecurityInterstitialCommandId.CMD_OPEN_DIAGNOSTIC);
    });
  }

  if ($('learn-more-link')) {
    $('learn-more-link').addEventListener('click', function(event) {
      sendCommand(SecurityInterstitialCommandId.CMD_OPEN_HELP_CENTER);
    });
  }

  if (captivePortal || billing || lookalike) {
    // Captive portal, billing and lookalike pages don't have details buttons.
    $('details-button').classList.add('hidden');
  } else {
    $('details-button').addEventListener('click', function(event) {
      const hiddenDetails = $('details').classList.toggle(HIDDEN_CLASS);

      if (mobileNav) {
        // Details appear over the main content on small screens.
        $('main-content').classList.toggle(HIDDEN_CLASS, !hiddenDetails);
      } else {
        $('main-content').classList.remove(HIDDEN_CLASS);
      }

      $('details-button').innerText = hiddenDetails ?
          loadTimeData.getString('openDetails') :
          loadTimeData.getString('closeDetails');
      if (!expandedDetails) {
        // Record a histogram entry only the first time that details is opened.
        sendCommand(SecurityInterstitialCommandId.CMD_SHOW_MORE_SECTION);
        expandedDetails = true;
      }
    });
  }

  if ($('report-error-link')) {
    $('report-error-link').addEventListener('click', function(event) {
      sendCommand(SecurityInterstitialCommandId.CMD_REPORT_PHISHING_ERROR);
    });
  }

  preventDefaultOnPoundLinkClicks();
  setupExtendedReportingCheckbox();
  setupSSLDebuggingInfo();
  document.addEventListener('keypress', handleKeypress);
}

document.addEventListener('DOMContentLoaded', setupEvents);
