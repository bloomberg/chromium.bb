// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

Audits2.Audits2StatusView = class {
  constructor() {
    this._statusView = null;
    this._progressWrapper = null;
    this._progressBar = null;
    this._statusText = null;

    this._textChangedAt = 0;
    this._fastFactsQueued = Audits2.Audits2StatusView.FastFacts.slice();
    this._currentPhase = null;
    this._scheduledTextChangeTimeout = null;
    this._scheduledFastFactTimeout = null;
  }

  /**
   * @param {!Element} parentElement
   */
  render(parentElement) {
    this.reset();

    this._statusView = parentElement.createChild('div', 'audits2-status vbox hidden');
    this._progressWrapper = this._statusView.createChild('div', 'audits2-progress-wrapper');
    this._progressBar = this._progressWrapper.createChild('div', 'audits2-progress-bar');
    this._statusText = this._statusView.createChild('div', 'audits2-status-text');

    this.updateStatus(Common.UIString('Loading...'));
  }

  reset() {
    this._resetProgressBarClasses();
    clearTimeout(this._scheduledFastFactTimeout);

    this._textChangedAt = 0;
    this._fastFactsQueued = Audits2.Audits2StatusView.FastFacts.slice();
    this._currentPhase = null;
    this._scheduledTextChangeTimeout = null;
    this._scheduledFastFactTimeout = null;
  }

  /**
   * @param {boolean} isVisible
   */
  setVisible(isVisible) {
    this._statusView.classList.toggle('hidden', !isVisible);

    if (!isVisible)
      clearTimeout(this._scheduledFastFactTimeout);
  }

  /**
   * @param {?string} message
   */
  updateStatus(message) {
    if (!message || !this._statusText)
      return;

    if (message.startsWith('Cancel')) {
      this._commitTextChange(Common.UIString('Cancelling\u2026'));
      clearTimeout(this._scheduledFastFactTimeout);
      return;
    }

    const nextPhase = this._getPhaseForMessage(message);
    if (!nextPhase && !this._currentPhase) {
      this._commitTextChange(Common.UIString('Lighthouse is warming up\u2026'));
      clearTimeout(this._scheduledFastFactTimeout);
    } else if (nextPhase && (!this._currentPhase || this._currentPhase.order < nextPhase.order)) {
      this._currentPhase = nextPhase;
      this._scheduleTextChange(Common.UIString(nextPhase.message));
      this._scheduleFastFactCheck();
      this._resetProgressBarClasses();
      this._progressBar.classList.add(nextPhase.progressBarClass);
    }
  }

  /**
   * @param {string} message
   * @return {?Audits2.Audits2StatusView.StatusPhases}
   */
  _getPhaseForMessage(message) {
    return Audits2.Audits2StatusView.StatusPhases.find(phase => message.startsWith(phase.statusMessagePrefix));
  }

  _resetProgressBarClasses() {
    if (!this._progressBar)
      return;

    this._progressBar.className = 'audits2-progress-bar';
  }

  _scheduleFastFactCheck() {
    if (!this._currentPhase || this._scheduledFastFactTimeout)
      return;

    this._scheduledFastFactTimeout = setTimeout(() => {
      this._updateFastFactIfNecessary();
      this._scheduledFastFactTimeout = null;

      this._scheduleFastFactCheck();
    }, 100);
  }

  _updateFastFactIfNecessary() {
    const now = performance.now();
    if (now - this._textChangedAt < Audits2.Audits2StatusView.fastFactRotationInterval)
      return;
    if (!this._fastFactsQueued.length)
      return;

    const fastFactIndex = Math.floor(Math.random() * this._fastFactsQueued.length);
    this._scheduleTextChange(ls`\ud83d\udca1 ${this._fastFactsQueued[fastFactIndex]}`);
    this._fastFactsQueued.splice(fastFactIndex, 1);
  }

  /**
   * @param {string} text
   */
  _commitTextChange(text) {
    if (!this._statusText)
      return;
    this._textChangedAt = performance.now();
    this._statusText.textContent = text;
  }

  /**
   * @param {string} text
   */
  _scheduleTextChange(text) {
    if (this._scheduledTextChangeTimeout)
      clearTimeout(this._scheduledTextChangeTimeout);

    const msSinceLastChange = performance.now() - this._textChangedAt;
    const msToTextChange = Audits2.Audits2StatusView.minimumTextVisibilityDuration - msSinceLastChange;

    this._scheduledTextChangeTimeout = setTimeout(() => {
      this._commitTextChange(text);
    }, Math.max(msToTextChange, 0));
  }

  /**
   * @param {!Error} err
   * @param {string} auditURL
   */
  renderBugReport(err, auditURL) {
    console.error(err);
    clearTimeout(this._scheduledFastFactTimeout);
    clearTimeout(this._scheduledTextChangeTimeout);
    this._resetProgressBarClasses();
    this._progressBar.classList.add('errored');

    this._commitTextChange('');
    this._statusText.createTextChild(Common.UIString('Ah, sorry! We ran into an error: '));
    this._statusText.createChild('em').createTextChild(err.message);
    if (Audits2.Audits2StatusView.KnownBugPatterns.some(pattern => pattern.test(err.message))) {
      const message = Common.UIString(
          'Try to navigate to the URL in a fresh Chrome profile without any other tabs or ' +
          'extensions open and try again.');
      this._statusText.createChild('p').createTextChild(message);
    } else {
      this._renderBugReportLink(err, auditURL);
    }
  }

  /**
   * @param {!Error} err
   * @param {string} auditURL
   */
  _renderBugReportLink(err, auditURL) {
    const baseURI = 'https://github.com/GoogleChrome/lighthouse/issues/new?';
    const title = encodeURI('title=DevTools Error: ' + err.message.substring(0, 60));

    const issueBody = `
**Initial URL**: ${auditURL}
**Chrome Version**: ${navigator.userAgent.match(/Chrome\/(\S+)/)[1]}
**Error Message**: ${err.message}
**Stack Trace**:
\`\`\`
${err.stack}
\`\`\`
    `;
    const body = '&body=' + encodeURIComponent(issueBody.trim());
    const reportErrorEl = UI.XLink.create(
        baseURI + title + body, Common.UIString('Report this bug'), 'audits2-link audits2-report-error');
    this._statusText.appendChild(reportErrorEl);
  }
};


/** @type {!Array.<!RegExp>} */
Audits2.Audits2StatusView.KnownBugPatterns = [
  /Parsing problem/,
  /Read failed/,
  /Tracing.*already started/,
  /Unable to load.*page/,
  /^You must provide a url to the runner/,
  /^You probably have multiple tabs open/,
];

/** @typedef {{message: string, progressBarClass: string, order: number}} */
Audits2.Audits2StatusView.StatusPhases = [
  {
    progressBarClass: 'loading',
    message: 'Lighthouse is loading your page with throttling to measure performance on a mobile device on 3G.',
    statusMessagePrefix: 'Loading page',
    order: 10,
  },
  {
    progressBarClass: 'gathering',
    message: 'Lighthouse is gathering information about the page to compute your score.',
    statusMessagePrefix: 'Retrieving',
    order: 20,
  },
  {
    progressBarClass: 'auditing',
    message: 'Almost there! Lighthouse is now generating your own special pretty report!',
    statusMessagePrefix: 'Evaluating',
    order: 30,
  }
];

Audits2.Audits2StatusView.FastFacts = [
  '1MB takes a minimum of 5 seconds to download on a typical 3G connection [Source: WebPageTest and DevTools 3G definition].',
  'Rebuilding Pinterest pages for performance increased conversion rates by 15% [Source: WPO Stats]',
  'BBC has seen a loss of 10% of their users for every extra second of page load [Source: WPO Stats]',
  'By reducing the response size of JSON needed for displaying comments, Instagram saw increased impressions [Source: WPO Stats]',
  'Walmart saw a 1% increase in revenue for every 100ms improvement in page load [Source: WPO Stats]',
  'If a site takes >1 second to become interactive, users lose attention, and their perception of completing the page task is broken [Source: Google Developers Blog]',
  '75% of global mobile users in 2016 were on 2G or 3G [Source: GSMA Mobile]',
  'The average user device costs less than 200 USD. [Source: International Data Corporation]',
  '53% of all site visits are abandoned if page load takes more than 3 seconds [Source: Google DoubleClick blog]',
  '19 seconds is the average time a mobile web page takes to load on a 3G connection [Source: Google DoubleClick blog]',
  '14 seconds is the average time a mobile web page takes to load on a 4G connection [Source: Google DoubleClick blog]',
  '70% of mobile pages take nearly 7 seconds for the visual content above the fold to display on the screen. [Source: Think with Google]',
  'As page load time increases from one second to seven seconds, the probability of a mobile site visitor bouncing increases 113%. [Source: Think with Google]',
  'As the number of elements on a page increases from 400 to 6,000, the probability of conversion drops 95%. [Source: Think with Google]',
  '70% of mobile pages weigh over 1MB, 36% over 2MB, and 12% over 4MB. [Source: Think with Google]',
  'Lighthouse only simulates mobile performance; to measure performance on a real device, try WebPageTest.org [Source: Lighthouse team]',
];

/** @const */
Audits2.Audits2StatusView.fastFactRotationInterval = 6000;
/** @const */
Audits2.Audits2StatusView.minimumTextVisibilityDuration = 3000;
