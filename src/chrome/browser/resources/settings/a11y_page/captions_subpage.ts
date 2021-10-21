// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview 'settings-captions' is a component for showing captions
 * settings subpage (chrome://settings/captions, and
 * chrome://os-settings/manageAccessibility/captions on Chrome OS).
 */

import '//resources/cr_elements/shared_style_css.m.js';
import '../controls/settings_slider.js';
import '../settings_shared_css.js';
import './live_caption_section.js';

import {html, PolymerElement} from '//resources/polymer/v3_0/polymer/polymer_bundled.min.js';

import {FontsBrowserProxy, FontsBrowserProxyImpl, FontsData} from '../appearance_page/fonts_browser_proxy.js';
import {DropdownMenuOptionList} from '../controls/settings_dropdown_menu.js';
import {SettingsToggleButtonElement} from '../controls/settings_toggle_button.js';
import {loadTimeData} from '../i18n_setup.js';
import {PrefsMixin} from '../prefs/prefs_mixin.js';

const SettingsCaptionsElementBase = PrefsMixin(PolymerElement);

class SettingsCaptionsElement extends SettingsCaptionsElementBase {
  static get is() {
    return 'settings-captions';
  }

  static get template() {
    return html`{__html_template__}`;
  }

  static get properties() {
    return {
      prefs: {
        type: Object,
        notify: true,
      },

      /**
       * List of options for the background opacity drop-down menu.
       */
      backgroundOpacityOptions_: {
        readOnly: true,
        type: Array,
        value() {
          return [
            {
              value: 100,  // Default
              name: loadTimeData.getString('captionsOpacityOpaque')
            },
            {
              value: 50,
              name: loadTimeData.getString('captionsOpacitySemiTransparent')
            },
            {
              value: 0,
              name: loadTimeData.getString('captionsOpacityTransparent')
            },
          ];
        },
      },

      /**
       * List of options for the color drop-down menu.
       */
      colorOptions_: {
        readOnly: true,
        type: Array,
        value() {
          return [
            {value: '', name: loadTimeData.getString('captionsDefaultSetting')},
            {
              value: '0,0,0',
              name: loadTimeData.getString('captionsColorBlack')
            },
            {
              value: '255,255,255',
              name: loadTimeData.getString('captionsColorWhite')
            },
            {
              value: '255,0,0',
              name: loadTimeData.getString('captionsColorRed')
            },
            {
              value: '0,255,0',
              name: loadTimeData.getString('captionsColorGreen')
            },
            {
              value: '0,0,255',
              name: loadTimeData.getString('captionsColorBlue')
            },
            {
              value: '255,255,0',
              name: loadTimeData.getString('captionsColorYellow')
            },
            {
              value: '0,255,255',
              name: loadTimeData.getString('captionsColorCyan')
            },
            {
              value: '255,0,255',
              name: loadTimeData.getString('captionsColorMagenta')
            },
          ];
        },
      },

      /**
       * List of fonts populated by the fonts browser proxy.
       */
      textFontOptions_: Object,

      /**
       * List of options for the text opacity drop-down menu.
       */
      textOpacityOptions_: {
        readOnly: true,
        type: Array,
        value() {
          return [
            {
              value: 100,  // Default
              name: loadTimeData.getString('captionsOpacityOpaque')
            },
            {
              value: 50,
              name: loadTimeData.getString('captionsOpacitySemiTransparent')
            },
            {
              value: 10,
              name: loadTimeData.getString('captionsOpacityTransparent')
            },
          ];
        },
      },

      /**
       * List of options for the text shadow drop-down menu.
       */
      textShadowOptions_: {
        readOnly: true,
        type: Array,
        value() {
          return [
            {value: '', name: loadTimeData.getString('captionsTextShadowNone')},
            {
              value: '-2px -2px 4px rgba(0, 0, 0, 0.5)',
              name: loadTimeData.getString('captionsTextShadowRaised')
            },
            {
              value: '2px 2px 4px rgba(0, 0, 0, 0.5)',
              name: loadTimeData.getString('captionsTextShadowDepressed')
            },
            {
              value: '-1px 0px 0px black, ' +
                  '0px -1px 0px black, 1px 0px 0px black, 0px  1px 0px black',
              name: loadTimeData.getString('captionsTextShadowUniform')
            },
            {
              value: '0px 0px 2px rgba(0, 0, 0, 0.5), 2px 2px 2px black',
              name: loadTimeData.getString('captionsTextShadowDropShadow')
            },
          ];
        },
      },

      /**
       * List of options for the text size drop-down menu.
       */
      textSizeOptions_: {
        readOnly: true,
        type: Array,
        value() {
          return [
            {value: '25%', name: loadTimeData.getString('verySmall')},
            {value: '50%', name: loadTimeData.getString('small')},
            {
              value: '',
              name: loadTimeData.getString('medium')
            },  // Default = 100%
            {value: '150%', name: loadTimeData.getString('large')},
            {value: '200%', name: loadTimeData.getString('veryLarge')},
          ];
        },
      },

      enableLiveCaption_: {
        type: Boolean,
        value: function() {
          return loadTimeData.getBoolean('enableLiveCaption');
        },
      },
    };
  }

  private readonly backgroundOpacityOptions_: DropdownMenuOptionList;
  private readonly colorOptions_: DropdownMenuOptionList;
  private textFontOptions_: DropdownMenuOptionList;
  private readonly textOpacityOptions_: DropdownMenuOptionList;
  private readonly textShadowOptions_: DropdownMenuOptionList;
  private readonly textSizeOptions_: DropdownMenuOptionList;
  private enableLiveCaption_: boolean;

  ready() {
    super.ready();
    FontsBrowserProxyImpl.getInstance().fetchFontsData().then(
        (response: FontsData) => this.setFontsData_(response));
  }

  /**
   * @return the Live Caption toggle element.
   */
  getLiveCaptionToggle(): SettingsToggleButtonElement|null {
    const liveCaptionSection =
        this.shadowRoot!.querySelector('settings-live-caption');
    return liveCaptionSection ? liveCaptionSection.getLiveCaptionToggle() :
                                null;
  }

  /**
   * @param response A list of fonts.
   */
  private setFontsData_(response: FontsData) {
    const fontMenuOptions =
        [{value: '', name: loadTimeData.getString('captionsDefaultSetting')}];
    for (const fontData of response.fontList) {
      fontMenuOptions.push({value: fontData[0], name: fontData[1]});
    }
    this.textFontOptions_ = fontMenuOptions;
  }

  /**
   * @return the font family as a CSS property value.
   */
  private getFontFamily_(): string {
    const fontFamily = this.getPref('accessibility.captions.text_font').value;

    // Return the preference value or the default font family for
    // video::-webkit-media-text-track-container defined in mediaControls.css.
    return fontFamily || 'sans-serif';
  }

  /**
   * @return the background color as a RGBA string.
   */
  private computeBackgroundColor_(): string {
    const backgroundColor = this.formatRGAString_(
        'accessibility.captions.background_color',
        'accessibility.captions.background_opacity');

    // Return the preference value or the default background color for
    // video::cue defined in mediaControls.css.
    return backgroundColor || 'rgba(0, 0, 0, 0.8)';
  }

  /**
   * @return the text color as a RGBA string.
   */
  private computeTextColor_(): string {
    const textColor = this.formatRGAString_(
        'accessibility.captions.text_color',
        'accessibility.captions.text_opacity');

    // Return the preference value or the default text color for
    // video::-webkit-media-text-track-container defined in mediaControls.css.
    return textColor || 'rgba(255, 255, 255, 1)';
  }

  /**
   * Formats the color as an RGBA string.
   * @param colorPreference The name of the preference containing the RGB values
   *     as a comma-separated string.
   * @param opacityPreference The name of the preference containing the opacity
   *     value as a percentage.
   * @return The formatted RGBA string.
   */
  private formatRGAString_(colorPreference: string, opacityPreference: string):
      string {
    const color = this.getPref(colorPreference).value;

    if (!color) {
      return '';
    }

    return 'rgba(' + color + ',' +
        parseInt(this.getPref(opacityPreference).value, 10) / 100.0 + ')';
  }

  /**
   * @param size The font size of the captions text as a percentage.
   * @return The padding around the captions text as a percentage.
   */
  private computePadding_(size: string): string {
    if (size === '') {
      return '1%';
    }

    return `${+ size.slice(0, -1) / 100}%`;
  }
}

customElements.define(SettingsCaptionsElement.is, SettingsCaptionsElement);
