// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Root from '../../core/root/root.js';

function disableLoggingForTest(): void {
  console.log = (): void => undefined;  // eslint-disable-line no-console
}

/**
 * Any message that comes back from Lighthouse has to go via a so-called "port".
 * This class holds the relevant callbacks that Lighthouse provides and that
 * can be called in the onmessage callback of the worker, so that the frontend
 * can communicate to Lighthouse. Lighthouse itself communicates to the frontend
 * via status updates defined below.
 */
class LighthousePort {
  onMessage?: (message: string) => void;
  onClose?: () => void;
  on(eventName: string, callback: (arg?: string) => void): void {
    if (eventName === 'message') {
      this.onMessage = callback;
    } else if (eventName === 'close') {
      this.onClose = callback;
    }
  }

  send(message: string): void {
    notifyFrontendViaWorkerMessage('sendProtocolMessage', {message});
  }
  close(): void {
  }
}

const port = new LighthousePort();

// eslint-disable-next-line @typescript-eslint/no-explicit-any
async function start(params: any): Promise<unknown> {
  if (Root.Runtime.Runtime.queryParam('isUnderTest')) {
    disableLoggingForTest();
    params.flags.maxWaitForLoad = 2 * 1000;
  }

  // @ts-expect-error https://github.com/GoogleChrome/lighthouse/issues/11628
  self.listenForStatus(message => {
    notifyFrontendViaWorkerMessage('statusUpdate', {message: message[1]});
  });

  try {
    const locale = await fetchLocaleData(params.locales);
    const flags = params.flags;
    flags.logLevel = flags.logLevel || 'info';
    flags.channel = 'devtools';
    flags.locale = locale;

    // @ts-expect-error https://github.com/GoogleChrome/lighthouse/issues/11628
    const connection = self.setUpWorkerConnection(port);
    // @ts-expect-error https://github.com/GoogleChrome/lighthouse/issues/11628
    const config = self.createConfig(params.categoryIDs, flags.emulatedFormFactor);
    const url = params.url;

    // @ts-expect-error https://github.com/GoogleChrome/lighthouse/issues/11628
    return await self.runLighthouse(url, flags, config, connection);
  } catch (err) {
    return ({
      fatal: true,
      message: err.message,
      stack: err.stack,
    });
  }
}

/**
 * Finds a locale supported by Lighthouse from the user's system locales.
 * If no matching locale is found, or if fetching locale data fails, this function returns nothing
 * and Lighthouse will use `en-US` by default.
 */
async function fetchLocaleData(locales: string[]): Promise<string|void> {
  // @ts-expect-error https://github.com/GoogleChrome/lighthouse/issues/11628
  const locale = self.lookupLocale(locales);

  // If the locale is en-US, no need to fetch locale data.
  if (locale === 'en-US' || locale === 'en') {
    return;
  }

  // Try to load the locale data.
  try {
    let localeDataTextPromise;
    const remoteBase = Root.Runtime.getRemoteBase();
    if (remoteBase && remoteBase.base) {
      const localeUrl = `${remoteBase.base}third_party/lighthouse/locales/${locale}.json`;
      localeDataTextPromise = Root.Runtime.loadResourcePromise(localeUrl);
    } else {
      const localeUrl = new URL(`../../third_party/lighthouse/locales/${locale}.json`, import.meta.url);
      localeDataTextPromise = Root.Runtime.loadResourcePromise(localeUrl.toString());
    }

    const timeoutPromise = new Promise<string>(
        (resolve, reject) => setTimeout(() => reject(new Error('timed out fetching locale')), 5000));
    const localeDataText = await Promise.race([timeoutPromise, localeDataTextPromise]);
    const localeData = JSON.parse(localeDataText);
    // @ts-expect-error https://github.com/GoogleChrome/lighthouse/issues/11628
    self.registerLocaleData(locale, localeData);
    return locale;
  } catch (err) {
    console.error(err);
  }

  return;
}

// eslint-disable-next-line @typescript-eslint/no-explicit-any
function notifyFrontendViaWorkerMessage(method: string, params: any): void {
  self.postMessage(JSON.stringify({method, params}));
}

self.onmessage = async(event: MessageEvent): Promise<void> => {
  const messageFromFrontend = JSON.parse(event.data);
  if (messageFromFrontend.method === 'start') {
    const result = await start(messageFromFrontend.params);
    self.postMessage(JSON.stringify({id: messageFromFrontend.id, result}));
  } else if (messageFromFrontend.method === 'dispatchProtocolMessage') {
    if (port.onMessage) {
      port.onMessage(messageFromFrontend.params.message);
    }
  } else {
    throw new Error(`Unknown event: ${event.data}`);
  }
};

// Make lighthouse and traceviewer happy.
// @ts-expect-error https://github.com/GoogleChrome/lighthouse/issues/11628
globalThis.global = self;
// @ts-expect-error https://github.com/GoogleChrome/lighthouse/issues/11628
globalThis.global.isVinn = true;
// @ts-expect-error https://github.com/GoogleChrome/lighthouse/issues/11628
globalThis.global.document = {};
// @ts-expect-error https://github.com/GoogleChrome/lighthouse/issues/11628
globalThis.global.document.documentElement = {};
// @ts-expect-error https://github.com/GoogleChrome/lighthouse/issues/11628
globalThis.global.document.documentElement.style = {
  WebkitAppearance: 'WebkitAppearance',
};
