// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

class CWSWidgetContainerErrorDialog extends cr.ui.dialogs.BaseDialog {
  /**
   * @param {HTMLElement} parentNode Node to be parent for this dialog.
   */
  constructor(parentNode) {
    super(parentNode);
  }

  /**
   * Whether the dialog is showm.
   * @return {boolean}
   */
  shown() {
    return this.container_.classList.contains('shown');
  }

  /**
   * One-time initialization of DOM.
   * @protected
   */
  initDom_() {
    super.initDom_();
    this.frame_.classList.add('cws-widget-error-dialog-frame');
    const img = this.document_.createElement('div');
    img.className = 'cws-widget-error-dialog-img';
    this.frame_.insertBefore(img, this.text_);

    this.title_.hidden = true;
    this.closeButton_.hidden = true;
    this.cancelButton_.hidden = true;
    this.text_.classList.add('cws-widget-error-dialog-text');

    // Don't allow OK button to lose focus, in order to prevent webview content
    // from stealing focus.
    // BaseDialog keeps focus by removing all other focusable elements from tab
    // order (by setting their tabIndex to -1). This doesn't work for webviews
    // because the webview embedder cannot access the webview DOM tree, and thus
    // fails to remove elements in the webview from tab order.
    this.okButton_.addEventListener('blur', this.refocusOkButton_.bind(this));
  }

  /**
   * Focuses OK button.
   * @private
   */
  refocusOkButton_() {
    if (this.shown()) {
      this.okButton_.focus();
    }
  }
}
