// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A panel to display a collection of PanelItem.
 * @extends HTMLElement
 */
class DisplayPanel extends HTMLElement {
  constructor() {
    DisplayPanel.createElement_.call(super());

    /** @private {?Element} */
    this.summary_ = this.shadowRoot.querySelector('#summary');

    /** @private {?Element} */
    this.separator_ = this.shadowRoot.querySelector('#separator');

    /** @private {?Element} */
    this.panels_ = this.shadowRoot.querySelector('#panels');

    /** @private {!function(!Event)} */
    this.listener_;

    /**
     * True if the panel is not visible.
     * @type {boolean}
     * @private
     */
    this.hidden_ = true;

    /**
     * True if the panel is collapsed to summary view.
     * @type {boolean}
     * @private
     */
    this.collapsed_ = false;

    /**
     * Collection of PanelItems hosted in this DisplayPanel.
     * @type {!Array<PanelItem>}
     * @private
     */
    this.items_ = [];
  }

  /**
   * Creates an instance of DisplayPanel, attaching the template clone.
   * @private
   */
  static createElement_() {
    const template = document.createElement('template');
    template.innerHTML = DisplayPanel.html_();
    const fragment = template.content.cloneNode(true);
    this.attachShadow({mode: 'open'}).appendChild(fragment);
  }

  /**
   * Get the custom element template string.
   * @private
   * @return {string}
   */
  static html_() {
    return `<style>
              :host {
                max-width: 400px;
              }
              #container {
                  align-items: stretch;
                  background-color: #FFF;
                  box-shadow: -1px -1px rgba(60, 64, 67, 0.15),
                              0px 2px rgba(60, 64, 67, 0.3),
                              2px 0px rgba(60, 64, 67, 0.15);
                  border-radius: 4px;
                  display: flex;
                  flex-direction: column;
                  max-width: min-content;
                  z-index: 100;
              }
              #separator {
                background-color: rgba(60, 64, 67, 0.15);
                height: 1px;
              }
              /* Limit to 3 visible progress panels before scroll. */
              #panels {
                  max-height: calc(192px + 28px);
                  overflow-y: auto;
              }
              xf-panel-item:not(:only-child) {
                --progress-height: 64px;
              }
              xf-panel-item:not(:only-child):first-child {
                --progress-padding-top: 14px;
              }
              xf-panel-item:not(:only-child):last-child {
                --progress-padding-bottom: 14px;
              }
              xf-panel-item:only-child {
                --progress-height: 68px;
              }
              @keyframes setcollapse {
                0% {
                  max-height: 0;
                  max-width: 0;
                  opacity: 0;
                }
                75% {
                  max-height: calc(192px + 28px);
                  width: 400px;
                  opacity: 0;
                }
                100% {
                  max-height: calc(192px + 28px);
                  width: 400px;
                  opacity: 1;
                }
              }

              @keyframes setexpand {
                0% {
                  max-height: calc(192px + 28px);
                  max-width: 400px;
                  opacity: 1;
                }
                25% {
                  max-height: calc(192px + 28px);
                  max-width: 400px;
                  opacity: 0;
                }
                100% {
                  max-height: 0;
                  max-width: 0;
                  opacity: 0;
                }
              }
              .expanded {
                animation: setcollapse 200ms forwards;
                width: 400px;
              }
              .collapsed {
                animation: setexpand 200ms forwards;
              }
              .expanding {
                overflow: hidden;
              }
              .expandfinished {
                overflow-y: auto;
              }
              xf-panel-item:not(:only-child) {
                --multi-progress-height: 92px;
              }
            </style>
            <div id="container">
              <div id="summary"></div>
              <div id="separator" hidden></div>
              <div id="panels"></div>
            </div>`;
  }

  /**
   * Re-enable scrollbar visibility after expand/contract animation.
   * @param {!Event} event
   */
  panelExpandFinished(event) {
    this.classList.remove('expanding');
    this.classList.add('expandfinished');
    this.removeEventListener('animationend', this.listener_);
  }

  /**
   * Hides the active panel items at end of collapse animation.
   * @param {!Event} event
   */
  panelCollapseFinished(event) {
    this.hidden = true;
    this.classList.remove('expanding');
    this.classList.add('expandfinished');
    this.removeEventListener('animationend', this.listener_);
  }

  /**
   * Event handler to toggle the visible state of panel items.
   * @private
   */
  toggleSummary(event) {
    const panel = event.target.parent;
    const summaryPanel = panel.summary_.querySelector('xf-panel-item');
    const expandButton =
        summaryPanel.shadowRoot.querySelector('#primary-action');
    // TODO(crbug.com/989322) i18n for this string.
    const fbWindow = ' Files feedback panels';
    if (panel.collapsed_) {
      panel.collapsed_ = false;
      expandButton.setAttribute('data-category', 'collapse');
      // TODO(crbug.com/989322) create a i18n{COLLAPSE_LABEL} to replace this..
      expandButton.setAttribute('aria-label', '$i18n{CLOSE_LABEL}' + fbWindow);
      expandButton.setAttribute('aria-expanded', 'true');
      panel.panels_.hidden = false;
      panel.separator_.hidden = false;
      panel.panels_.listener_ = panel.panelExpandFinished;
      panel.panels_.addEventListener('animationend', panel.panelExpandFinished);
      panel.panels_.setAttribute('class', 'expanded expanding');
    } else {
      panel.collapsed_ = true;
      expandButton.setAttribute('data-category', 'expand');
      expandButton.setAttribute('aria-label', '$i18n{EXPAND_LABEL}' + fbWindow);
      expandButton.setAttribute('aria-expanded', 'false');
      panel.separator_.hidden = true;
      panel.panels_.listener_ = panel.panelCollapseFinished;
      panel.panels_.addEventListener(
          'animationend', panel.panelCollapseFinished);
      panel.panels_.setAttribute('class', 'collapsed expanding');
    }
  }

  /**
   * Update the summary panel item progress indicator.
   * @public
   */
  updateProgress() {
    let total = 0;

    if (this.items_.length == 0) {
      return;
    }
    for (let i = 0; i < this.items_.length; ++i) {
      total += Number(this.items_[i].progress);
    }
    total /= this.items_.length;
    const summaryPanel = this.summary_.querySelector('xf-panel-item');
    if (summaryPanel) {
      // TODO(crbug.com/947388) i18n this string (add setter).
      summaryPanel.primaryText = total.toFixed(0) + '% complete';
      summaryPanel.progress = total;
    }
  }

  /**
   * Update the summary panel.
   * @public
   */
  updateSummaryPanel() {
    let summaryHost = this.shadowRoot.querySelector('#summary');
    let summaryPanel = summaryHost.querySelector('#summary-panel');

    // Work out how many progress panels are being shown.
    let count = 0;
    for (let i = 0; i < this.items_.length; ++i) {
      if (this.items_[i].panelType == this.items_[i].panelTypeProgress) {
        count++;
      }
    }
    // If there's only one progress panel item active, no need for summary.
    if (count <= 1 && summaryPanel) {
      const button = summaryPanel.primaryButton;
      if (button) {
        button.removeEventListener('click', this.toggleSummary);
      }
      summaryPanel.remove();
      this.panels_.hidden = false;
      this.separator_.hidden = true;
      this.panels_.classList.remove('collapsed');
      return;
    }
    // Show summary panel if there are more than 1 progress panels.
    if (count > 1 && !summaryPanel) {
      summaryPanel = document.createElement('xf-panel-item');
      summaryPanel.setAttribute('panel-type', 1);
      summaryPanel.id = 'summary-panel';
      const button = summaryPanel.primaryButton;
      if (button) {
        button.parent = this;
        button.addEventListener('click', this.toggleSummary);
      }
      summaryHost.appendChild(summaryPanel);
      this.panels_.hidden = true;
      this.collapsed_ = true;
    }
    if (summaryPanel) {
      summaryPanel.setAttribute('count', count);
      this.updateProgress();
    }
  }

  /**
   * Add a panel entry element inside our display panel.
   * @param {string} id The identifier attached to this panel.
   * @return {PanelItem}
   * @public
   */
  addPanelItem(id) {
    const panel = document.createElement('xf-panel-item');
    panel.id = id;
    // Set the containing parent so the child panel can
    // trigger updates in the parent (e.g. progress summary %).
    panel.parent = this;
    panel.setAttribute('indicator', 'progress');
    this.panels_.appendChild(panel);
    this.items_.push(/** @type {!PanelItem} */ (panel));
    this.updateSummaryPanel();
    return /** @type {!PanelItem} */ (panel);
  }

  /**
   * Remove a panel from this display panel.
   * @param {PanelItem} item The PanelItem to remove.
   * @public
   */
  removePanelItem(item) {
    const index = this.items_.indexOf(item);
    if (index === -1) {
      return;
    }
    item.remove();
    this.items_.splice(index, 1);
    this.updateSummaryPanel();
  }

  /**
   * Find a panel with given 'id'.
   * @public
   */
  findPanelItemById(id) {
    const panel = this.shadowRoot.querySelector('xf-panel-item[id=' + id + ']');
    return panel || null;
  }
}

window.customElements.define('xf-display-panel', DisplayPanel);
