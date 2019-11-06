// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * A panel to display the status or progress of a file operation.
 */
class PanelItem extends HTMLElement {
  constructor() {
    super();
    const host = document.createElement('template');
    host.innerHTML = this.constructor.template_;
    this.attachShadow({mode: 'open'}).appendChild(host.content.cloneNode(true));

    /** @private {Element} */
    this.indicator_ = this.shadowRoot.querySelector('#indicator');
  }

  /**
   * Static getter for the custom element template.
   * @private
   * @return {string}
   */
  static get template_() {
    return `<link rel='stylesheet' href='files_visual_signals.css'>
                <div class='xf-panel-item'>
                    <div class='xf-panel-text'>
                        <span class='xf-panel-label-text'>
                                Copying File_With_a_long_name.jpg</span>
                        <span class='xf-panel-secondary-text'>
                                To SubFolder A1</span>
                    </div>
                    <div class='xf-padder-24'></div>
                    <button class='xf-button' src='pause-icon.png'
                            tabindex='-1'>
                    </button>
                    <div class='xf-padder-4'></div>
                    <button class='xf-button' src='cancel-icon.png'
                            tabindex='-1'>
                    </button>
                </div>`;
  }

  /**
   * Registers this instance to listen to these attribute changes.
   * @private
   */
  static get observedAttributes() {
    return ['indicator', 'progress'];
  }

  /**
   * Callback triggered by the browser when our attribute values change.
   * @param {string} name Attribute that's changed.
   * @param {string} oldValue Old value of the attribute.
   * @param {string} newValue New value of the attribute.
   * @private
   */
  attributeChangedCallback(name, oldValue, newValue) {
    let indicator;
    switch (name) {
      case 'indicator':
        if (oldValue !== newValue) {
          if (newValue === 'progress' || newValue === 'largeprogress') {
            indicator = document.createElement('xf-circular-progress');
            indicator.setAttribute('id', 'indicator');
            if (newValue === 'largeprogress') {
              indicator.setAttribute('radius', '14');
            }
            const itemRoot = this.shadowRoot.querySelector('.xf-panel-item');
            itemRoot.prepend(indicator);
          }
        }
        break;
      case 'progress':
        indicator = this.shadowRoot.querySelector('xf-circular-progress');
        if (indicator) {
          indicator.progress = Number(newValue);
        }
        break;
    }
  }
}

window.customElements.define('xf-panel-item', PanelItem);
