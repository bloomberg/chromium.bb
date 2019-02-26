// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * Javascript for omnibox.html, served from chrome://omnibox/
 * This is used to debug omnibox ranking.  The user enters some text
 * into a box, submits it, and then sees lots of debug information
 * from the autocompleter that shows what omnibox would do with that
 * input.
 *
 * The simple object defined in this javascript file listens for
 * certain events on omnibox.html, sends (when appropriate) the
 * input text to C++ code to start the omnibox autcomplete controller
 * working, and listens from callbacks from the C++ code saying that
 * results are available.  When results (possibly intermediate ones)
 * are available, the Javascript formats them and displays them.
 */

(function () {
  /**
   * @type {number} the value for cursor position we sent with the most
   *     recent request.  We need to remember this in order to display it
   *     in the output; otherwise it's hard or impossible to determine
   *     from screen captures or print-to-PDFs.
   */
  let cursorPosition = -1;

  class BrowserProxy {
    constructor() {
      /** @private {!mojom.OmniboxPageCallbackRouter} */
      this.callbackRouter_ = new mojom.OmniboxPageCallbackRouter;

      // TODO (manukh) rename method to handleNewAutocompleteResponse in order
      // to keep terminology consistent. Result refers to a single autocomplete
      // match. Response refers to the data returned from the C++
      // AutocompleteController.
      this.callbackRouter_.handleNewAutocompleteResult.addListener(
          result => omniboxOutput.addAutocompleteResponse(result));

      /** @private {!mojom.OmniboxPageHandlerProxy} */
      this.handler_ = mojom.OmniboxPageHandler.getProxy();
      this.handler_.setClientPage(this.callbackRouter_.createProxy());
    }

    /**
     * Extracts the input text from the text field and sends it to the
     * C++ portion of chrome to handle.  The C++ code will iteratively
     * call handleNewAutocompleteResult as results come in.
     */
    makeRequest(inputString,
                cursorPosition,
                preventInlineAutocomplete,
                preferKeyword,
                pageClassification) {
      // Then, call chrome with a five-element list:
      // - first element: the value in the text box
      // - second element: the location of the cursor in the text box
      // - third element: the value of prevent-inline-autocomplete
      // - forth element: the value of prefer-keyword
      // - fifth element: the value of page-classification
      this.handler_.startOmniboxQuery(
          inputString, cursorPosition, preventInlineAutocomplete, preferKeyword,
          pageClassification);
    }
  }

  /** @type {!BrowserProxy} */
  const browserProxy = new BrowserProxy();
  /** @type {!OmniboxInputs} */
  let omniboxInputs;
  /** @type {!omnibox_output.OmniboxOutput} */
  let omniboxOutput;

  document.addEventListener('DOMContentLoaded', () => {
    omniboxInputs = /** @type {!OmniboxInputs} */ ($('omnibox-inputs'));
    omniboxOutput =
        /** @type {!omnibox_output.OmniboxOutput} */ ($('omnibox-output'));

    omniboxInputs.addEventListener('query-inputs-changed', event => {
      omniboxOutput.clearAutocompleteResponses();
      omniboxOutput.updateQueryInputs(event.detail);
      browserProxy.makeRequest(
          event.detail.inputText,
          event.detail.cursorPosition,
          event.detail.preventInlineAutocomplete,
          event.detail.preferKeyword,
          event.detail.pageClassification);
    });
    omniboxInputs.addEventListener(
        'display-inputs-changed',
        event => omniboxOutput.updateDisplayInputs(event.detail));
    omniboxInputs.addEventListener(
        'copy-request',
        event => event.detail === 'text' ?
            omniboxOutput.copyDelegate.copyTextOutput() :
            omniboxOutput.copyDelegate.copyJsonOutput());
    omniboxInputs.addEventListener(
        'filter-input-changed',
        event => omniboxOutput.filterDelegate.filter(
            event.detail.filterText, event.detail.filterHide));
  });
})();
