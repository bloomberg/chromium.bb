// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

ColorPicker.ContrastInfo = class {
  constructor() {
    /** @type {?Common.Color} */
    this._fgColor = null;

    /** @type {?Common.Color} */
    this._bgColor = null;

    /** @type {?Array<!Common.Color>} */
    this._gradient = null;

    /** @type {?number} */
    this._contrastRatio = null;

    /** @type {?Object<string, number>} */
    this._contrastRatioThresholds = null;

    /** @type {string} */
    this._colorString = '';
  }

  /**
   * @param {?SDK.CSSModel.ContrastInfo} contrastInfo
   */
  setContrastInfo(contrastInfo) {
    this._contrastRatio = null;
    this._contrastRatioThresholds = null;
    this._bgColor = null;
    this._gradient = null;

    if (contrastInfo.computedFontSize && contrastInfo.computedFontWeight && contrastInfo.computedBodyFontSize) {
      var isLargeFont = ColorPicker.ContrastInfo.computeIsLargeFont(
          contrastInfo.computedFontSize, contrastInfo.computedFontWeight, contrastInfo.computedBodyFontSize);

      this._contrastRatioThresholds =
          ColorPicker.ContrastInfo._ContrastThresholds[(isLargeFont ? 'largeFont' : 'normalFont')];
    }

    if (!contrastInfo.backgroundColors || !contrastInfo.backgroundColors.length)
      return;

    if (contrastInfo.backgroundColors.length > 1) {
      this._gradient = [];
      for (var bgColorText of contrastInfo.backgroundColors) {
        var bgColor = Common.Color.parse(bgColorText);
        if (bgColor)
          this._gradient.push(bgColor);
      }
      if (this._fgColor)
        this._updateBgColorFromGradient();
    } else {
      var bgColorText = contrastInfo.backgroundColors[0];
      var bgColor = Common.Color.parse(bgColorText);
      if (bgColor)
        this.setBgColor(bgColor);
    }
  }

  /**
   * @param {!Array<number>} hsva
   * @param {string} colorString
   */
  setColor(hsva, colorString) {
    this._fgColor = Common.Color.fromHSVA(hsva);
    this._colorString = colorString;
    if (this._gradient)
      this._updateBgColorFromGradient();
    else
      this._updateContrastRatio();
  }

  /**
   * @return {?number}
   */
  contrastRatio() {
    return this._contrastRatio;
  }

  /**
   * @return {string}
   */
  colorString() {
    return this._colorString;
  }

  /**
   * @return {?Array<number>}
   */
  hsva() {
    return this._fgColor.hsva();
  }

  /**
   * @param {!Common.Color} bgColor
   */
  setBgColor(bgColor) {
    this._bgColor = bgColor;

    if (!this._fgColor)
      return;

    var fgRGBA = this._fgColor.rgba();

    // If we have a semi-transparent background color over an unknown
    // background, draw the line for the "worst case" scenario: where
    // the unknown background is the same color as the text.
    if (bgColor.hasAlpha) {
      var blendedRGBA = [];
      Common.Color.blendColors(bgColor.rgba(), fgRGBA, blendedRGBA);
      this._bgColor = new Common.Color(blendedRGBA, Common.Color.Format.RGBA);
    }

    this._contrastRatio = Common.Color.calculateContrastRatio(fgRGBA, this._bgColor.rgba());
  }

  /**
   * @return {?Common.Color}
   */
  bgColor() {
    return this._bgColor;
  }

  /**
   * @return {?Array<!Common.Color>}
   */
  gradient() {
    return this._gradient;
  }

  _updateContrastRatio() {
    if (!this._bgColor || !this._fgColor)
      return;
    this._contrastRatio = Common.Color.calculateContrastRatio(this._fgColor.rgba(), this._bgColor.rgba());
  }

  /**
   * Choose the gradient stop closest in luminance to the foreground color.
   */
  _updateBgColorFromGradient() {
    if (!this._gradient || !this._fgColor)
      return;
    var fgLuminance = Common.Color.luminance(this._fgColor.rgba());
    var luminanceDiff = 2;  // Max luminance == 1
    var closestBgColor = null;
    for (var bgColor of this._gradient) {
      var bgLuminance = Common.Color.luminance(bgColor.rgba());
      var currentLuminanceDiff = Math.abs(fgLuminance - bgLuminance);
      if (currentLuminanceDiff < luminanceDiff) {
        closestBgColor = bgColor;
        luminanceDiff = currentLuminanceDiff;
      }
    }
    if (closestBgColor)
      this.setBgColor(closestBgColor);
  }

  /**
   * @param {string} level
   * @return {?number}
   */
  contrastRatioThreshold(level) {
    if (!this._contrastRatioThresholds)
      return null;
    return this._contrastRatioThresholds[level];
  }

  /**
   * @param {string} fontSize
   * @param {string} fontWeight
   * @param {?string} bodyFontSize
   * @return {boolean}
   */
  static computeIsLargeFont(fontSize, fontWeight, bodyFontSize) {
    const boldWeights = ['bold', 'bolder', '600', '700', '800', '900'];

    var fontSizePx = parseFloat(fontSize.replace('px', ''));
    var isBold = (boldWeights.indexOf(fontWeight) !== -1);

    if (bodyFontSize) {
      var bodyFontSizePx = parseFloat(bodyFontSize.replace('px', ''));
      if (isBold) {
        if (fontSizePx >= (bodyFontSizePx * 1.2))
          return true;
      } else if (fontSizePx >= (bodyFontSizePx * 1.5)) {
        return true;
      }
      return false;
    }

    var fontSizePt = Math.ceil(fontSizePx * 72 / 96);
    if (isBold)
      return fontSizePt >= 14;
    else
      return fontSizePt >= 18;
  }
};

ColorPicker.ContrastInfo._ContrastThresholds = {
  largeFont: {AA: 3.0, AAA: 4.5},
  normalFont: {AA: 4.5, AAA: 7.0}
};

ColorPicker.ContrastOverlay = class {
  /**
   * @param {!Element} colorElement
   * @param {!Element} contentElement
   * @param {function(boolean=, !Common.Event=)} toggleMainColorPickerCallback
   */
  constructor(colorElement, contentElement, toggleMainColorPickerCallback) {
    this._contrastInfo = new ColorPicker.ContrastInfo();

    var contrastRatioSVG = colorElement.createSVGChild('svg', 'spectrum-contrast-container fill');
    this._contrastRatioLine = contrastRatioSVG.createSVGChild('path', 'spectrum-contrast-line');

    this._contrastValueBubble = colorElement.createChild('button', 'spectrum-contrast-info');
    this._contrastValueBubble.classList.add('force-white-icons');
    UI.ARIAUtils.setExpanded(this._contrastValueBubble, false);
    this._contrastValueBubble.createChild('span', 'low-contrast').textContent = Common.UIString('Low contrast');
    this._contrastValue = this._contrastValueBubble.createChild('span', 'value');
    this._contrastValueBubble.appendChild(UI.Icon.create('smallicon-contrast-ratio'));
    this._contrastValueBubble.title = Common.UIString('Click to toggle contrast ratio details');
    this._contrastValueBubble.addEventListener('mousedown', this._toggleContrastDetails.bind(this), true);
    this._contrastValueBubble.addEventListener('click', this._onToggleClick.bind(this), true);

    /** @type {!AnchorBox} */
    this._contrastValueBubbleBoxInWindow = new AnchorBox(0, 0, 0, 0);

    this._contrastDetails = new ColorPicker.ContrastDetails(
        this._contrastInfo, contentElement, toggleMainColorPickerCallback, this._update.bind(this));
    UI.ARIAUtils.setControls(this._contrastValueBubble, this._contrastDetails.element());
    this._width = 0;
    this._height = 0;

    this._contrastRatioLineThrottler = new Common.Throttler(0);
    this._drawContrastRatioLineBound = this._drawContrastRatioLine.bind(this);

    /** @type {?number} */
    this._hueForCurrentLine = null;
    /** @type {?number} */
    this._alphaForCurrentLine = null;
    /** @type {?string} */
    this._bgColorForCurrentLine = null;
  }

  /**
   * @param {?SDK.CSSModel.ContrastInfo} contrastInfo
   */
  setContrastInfo(contrastInfo) {
    this._contrastInfo.setContrastInfo(contrastInfo);
    this._update();
  }

  /**
   * @param {!Array<number>} hsva
   * @param {string} colorString
   */
  setColor(hsva, colorString) {
    this._contrastInfo.setColor(hsva, colorString);
    this._update();
  }

  /**
   * @param {number} x
   * @param {number} y
   */
  moveAwayFrom(x, y) {
    if (!this._contrastValueBubbleBoxInWindow.width || !this._contrastValueBubbleBoxInWindow.height ||
        !this._contrastValueBubbleBoxInWindow.contains(x, y))
      return;

    var bubble = this._contrastValueBubble;
    if (bubble.offsetWidth > ((bubble.offsetParent.offsetWidth / 2) - 10))
      bubble.classList.toggle('contrast-info-top');
    else
      bubble.classList.toggle('contrast-info-left');
  }

  _update() {
    UI.ARIAUtils.setExpanded(this._contrastValueBubble, this._contrastDetails.visible());
    var AA = this._contrastInfo.contrastRatioThreshold('AA');
    if (!AA)
      return;

    this._contrastValue.textContent = '';
    if (this._contrastInfo.contrastRatio() !== null) {
      this._contrastValue.textContent = this._contrastInfo.contrastRatio().toFixed(2);
      this._contrastRatioLineThrottler.schedule(this._drawContrastRatioLineBound);
      var passesAA = this._contrastInfo.contrastRatio() >= AA;
      this._contrastValueBubble.classList.toggle('contrast-fail', !passesAA);
      this._contrastValueBubble.classList.remove('contrast-unknown');
    } else {
      this._contrastValueBubble.classList.remove('contrast-fail');
      this._contrastValueBubble.classList.add('contrast-unknown');
    }

    this._contrastValueBubbleBoxInWindow = this._contrastValueBubble.boxInWindow();
    this._contrastDetails.update();
  }

  /**
   * @param {number} width
   * @param {number} height
   * @param {number} dragX
   * @param {number} dragY
   */
  show(width, height, dragX, dragY) {
    if (this._contrastInfo.contrastRatioThreshold('AA') === null) {
      this.hide();
      return;
    }

    this._width = width;
    this._height = height;
    this._update();

    this._contrastValueBubble.classList.remove('hidden');
    this.moveAwayFrom(dragX, dragY);
  }

  hide() {
    this._contrastValueBubble.classList.add('hidden');
  }

  /**
   * @param {!Event} event
   */
  _toggleContrastDetails(event) {
    if ('button' in event && event.button !== 0)
      return;
    event.consume();
    this._contrastDetails.toggleVisible();
    UI.ARIAUtils.setExpanded(this._contrastValueBubble, this._contrastDetails.visible());
  }

  /**
   * @param {!Event} event
   */
  _onToggleClick(event) {
    // Handle a synthetic click via keyboard, which doesn't trigger a mousedown.
    if (event.screenX || event.screenY)
      return;
    event.consume();
    this._toggleContrastDetails(event);
  }

  /**
   * @return {!Promise}
   */
  _drawContrastRatioLine() {
    var width = this._width;
    var height = this._height;
    var requiredContrast = this._contrastInfo.contrastRatioThreshold('AA');
    if (!width || !height || !requiredContrast)
      return Promise.resolve();

    const dS = 0.02;
    const epsilon = 0.0002;
    const H = 0;
    const S = 1;
    const V = 2;
    const A = 3;

    var hsva = this._contrastInfo.hsva();
    var bgColor = this._contrastInfo.bgColor();
    if (!hsva || !bgColor)
      return Promise.resolve();
    var bgColorString = bgColor.asString(Common.Color.Format.RGBA);
    if (hsva[H] === this._hueForCurrentLine && hsva[A] === this._alphaForCurrentLine &&
        bgColorString === this._bgColorForCurrentLine)
      return Promise.resolve();

    var fgRGBA = [];
    Common.Color.hsva2rgba(hsva, fgRGBA);
    var bgRGBA = bgColor.rgba();
    var bgLuminance = Common.Color.luminance(bgRGBA);
    var blendedRGBA = [];
    Common.Color.blendColors(fgRGBA, bgRGBA, blendedRGBA);
    var fgLuminance = Common.Color.luminance(blendedRGBA);
    this._contrastValueBubble.classList.toggle('light', fgLuminance > 0.5);
    var fgIsLighter = fgLuminance > bgLuminance;
    var desiredLuminance = Common.Color.desiredLuminance(bgLuminance, requiredContrast, fgIsLighter);

    var lastV = hsva[V];
    var currentSlope = 0;
    var candidateHSVA = [hsva[H], 0, 0, hsva[A]];
    var pathBuilder = [];
    var candidateRGBA = [];
    Common.Color.hsva2rgba(candidateHSVA, candidateRGBA);
    Common.Color.blendColors(candidateRGBA, bgRGBA, blendedRGBA);

    /**
     * @param {number} index
     * @param {number} x
     */
    function updateCandidateAndComputeDelta(index, x) {
      candidateHSVA[index] = x;
      Common.Color.hsva2rgba(candidateHSVA, candidateRGBA);
      Common.Color.blendColors(candidateRGBA, bgRGBA, blendedRGBA);
      return Common.Color.luminance(blendedRGBA) - desiredLuminance;
    }

    /**
     * Approach a value of the given component of `candidateHSVA` such that the
     * calculated luminance of `candidateHSVA` approximates `desiredLuminance`.
     * @param {number} index The component of `candidateHSVA` to modify.
     * @return {?number} The new value for the modified component, or `null` if
     *     no suitable value exists.
     */
    function approach(index) {
      var x = candidateHSVA[index];
      var multiplier = 1;
      var dLuminance = updateCandidateAndComputeDelta(index, x);
      var previousSign = Math.sign(dLuminance);

      for (var guard = 100; guard; guard--) {
        if (Math.abs(dLuminance) < epsilon)
          return x;

        var sign = Math.sign(dLuminance);
        if (sign !== previousSign) {
          // If `x` overshoots the correct value, halve the step size.
          multiplier /= 2;
          previousSign = sign;
        } else if (x < 0 || x > 1) {
          // If there is no overshoot and `x` is out of bounds, there is no
          // acceptable value for `x`.
          return null;
        }

        // Adjust `x` by a multiple of `dLuminance` to decrease step size as
        // the computed luminance converges on `desiredLuminance`.
        x += multiplier * (index === V ? -dLuminance : dLuminance);

        dLuminance = updateCandidateAndComputeDelta(index, x);
      }
      // The loop should always converge or go out of bounds on its own.
      console.error('Loop exited unexpectedly');
      return null;
    }

    // Plot V for values of S such that the computed luminance approximates
    // `desiredLuminance`, until no suitable value for V can be found, or the
    // current value of S goes of out bounds.
    for (var s = 0; s < 1 + dS; s += dS) {
      s = Math.min(1, s);
      candidateHSVA[S] = s;

      // Extrapolate the approximate next value for `v` using the approximate
      // gradient of the curve.
      candidateHSVA[V] = lastV + currentSlope * dS;

      var v = approach(V);
      if (v === null)
        break;

      // Approximate the current gradient of the curve.
      currentSlope = s === 0 ? 0 : (v - lastV) / dS;
      lastV = v;

      pathBuilder.push(pathBuilder.length ? 'L' : 'M');
      pathBuilder.push((s * width).toFixed(2));
      pathBuilder.push(((1 - v) * height).toFixed(2));
    }

    // If no suitable V value for an in-bounds S value was found, find the value
    // of S such that V === 1 and add that to the path.
    if (s < 1 + dS) {
      s -= dS;
      candidateHSVA[V] = 1;
      s = approach(S);
      if (s !== null)
        pathBuilder = pathBuilder.concat(['L', (s * width).toFixed(2), '-0.1']);
    }

    this._contrastRatioLine.setAttribute('d', pathBuilder.join(' '));
    this._bgColorForCurrentLine = bgColorString;
    this._hueForCurrentLine = hsva[H];
    this._alphaForCurrentLine = hsva[A];

    return Promise.resolve();
  }
};

ColorPicker.ContrastDetails = class {
  /**
   * @param {!ColorPicker.ContrastInfo} contrastInfo
   * @param {!Element} contentElement
   * @param {function(boolean=, !Common.Event=)} toggleMainColorPickerCallback
   * @param {function()} updateContrastOverlayCallback
   */
  constructor(contrastInfo, contentElement, toggleMainColorPickerCallback, updateContrastOverlayCallback) {
    /** @type {!ColorPicker.ContrastInfo} */
    this._contrastInfo = contrastInfo;

    /** @type {function(boolean=, !Common.Event=)} */
    this._toggleMainColorPicker = toggleMainColorPickerCallback;

    /** @type {function()} */
    this._updateContrastOverlayCallback = updateContrastOverlayCallback;

    this._visible = false;

    this._contrastDetails = contentElement.createChild('div', 'spectrum-contrast-details');
    this._contrastDetails.id = 'contrast-ratio-details';
    var contrastValueRow = this._contrastDetails.createChild('div');
    contrastValueRow.createTextChild(Common.UIString('Contrast Ratio'));
    contrastValueRow.createTextChild(' ');

    var linkName = Common.UIString('Color and contrast on Web Fundamentals');

    var contrastLink = UI.createExternalLink(
        'https://developers.google.com/web/fundamentals/accessibility/accessible-styles#color_and_contrast', linkName,
        'contrast-link');
    contrastLink.textContent = '';
    contrastValueRow.appendChild(contrastLink);
    UI.ARIAUtils.setAccessibleName(contrastLink, linkName);
    contrastLink.appendChild(UI.Icon.create('mediumicon-info'));

    this._contrastValueBubble = contrastValueRow.createChild('span', 'contrast-details-value force-white-icons');
    this._contrastValue = this._contrastValueBubble.createChild('span');
    this._contrastValueBubbleIcons = [];
    this._contrastValueBubbleIcons.push(
        this._contrastValueBubble.appendChild(UI.Icon.create('smallicon-checkmark-square')));
    this._contrastValueBubbleIcons.push(
        this._contrastValueBubble.appendChild(UI.Icon.create('smallicon-checkmark-behind')));
    this._contrastValueBubbleIcons.push(this._contrastValueBubble.appendChild(UI.Icon.create('smallicon-no')));
    this._contrastValueBubble.addEventListener('mouseenter', this._toggleContrastValueHovered.bind(this));
    this._contrastValueBubble.addEventListener('mouseleave', this._toggleContrastValueHovered.bind(this));

    var toolbar = new UI.Toolbar('', contrastValueRow);
    var closeButton = new UI.ToolbarButton('Hide contrast ratio details', 'largeicon-delete');
    closeButton.addEventListener(UI.ToolbarButton.Events.Click, this.hide.bind(this));
    toolbar.appendToolbarItem(closeButton);

    this._chooseBgColor = this._contrastDetails.createChild('div', 'contrast-choose-bg-color');
    this._chooseBgColor.textContent = Common.UIString('Please select background color to compute contrast ratio.');

    this._contrastThresholds = this._contrastDetails.createChild('div', 'contrast-thresholds');
    this._contrastAA = this._contrastThresholds.createChild('div', 'contrast-threshold');
    this._contrastAA.appendChild(UI.Icon.create('smallicon-checkmark-square'));
    this._contrastAA.appendChild(UI.Icon.create('smallicon-no'));
    this._contrastPassFailAA = this._contrastAA.createChild('span', 'contrast-pass-fail');

    this._contrastAAA = this._contrastThresholds.createChild('div', 'contrast-threshold');
    this._contrastAAA.appendChild(UI.Icon.create('smallicon-checkmark-square'));
    this._contrastAAA.appendChild(UI.Icon.create('smallicon-no'));
    this._contrastPassFailAAA = this._contrastAAA.createChild('span', 'contrast-pass-fail');

    var bgColorRow = this._contrastDetails.createChild('div');
    bgColorRow.createTextChild(Common.UIString('Background color:'));
    this._bgColorSwatch = new ColorPicker.ContrastDetails.Swatch(bgColorRow);

    this._bgColorPicker = bgColorRow.createChild('button', 'background-color-picker');
    this._bgColorPicker.appendChild(UI.Icon.create('largeicon-eyedropper'));
    this._bgColorPicker.addEventListener('click', this._toggleBackgroundColorPicker.bind(this, undefined));
    this._bgColorPickedBound = this._bgColorPicked.bind(this);
  }

  update() {
    var AA = this._contrastInfo.contrastRatioThreshold('AA');
    var AAA = this._contrastInfo.contrastRatioThreshold('AAA');
    if (!AA)
      return;

    var contrastRatio = this._contrastInfo.contrastRatio();
    var bgColor = this._contrastInfo.bgColor();
    if (!contrastRatio || !bgColor) {
      this._contrastValue.textContent = '?';
      this._contrastValueBubble.classList.add('contrast-unknown');
      this._chooseBgColor.classList.remove('hidden');
      this._contrastThresholds.classList.add('hidden');
      return;
    }

    this._chooseBgColor.classList.add('hidden');
    this._contrastThresholds.classList.remove('hidden');
    this._contrastValueBubble.classList.remove('contrast-unknown');
    this._contrastValue.textContent = contrastRatio.toFixed(2);

    var gradient = this._contrastInfo.gradient();
    if (gradient && gradient.length) {
      var darkest = null;
      var lightness = null;
      for (var color of gradient) {
        var hsla = color.hsla();
        if (!darkest || hsla[2] < lightness) {
          darkest = color;
          lightness = hsla[2];
        }
      }
      var gradientStrings = gradient.map(color => color.asString(Common.Color.Format.RGBA));
      var gradientString = String.sprintf('linear-gradient(90deg, %s)', gradientStrings.join(', '));
      this._bgColorSwatch.setColor(/** @type {!Common.Color} */ (darkest), gradientString);
    } else {
      this._bgColorSwatch.setColor(bgColor);
    }

    var passesAA = this._contrastInfo.contrastRatio() >= AA;
    this._contrastPassFailAA.textContent = '';
    this._contrastPassFailAA.createTextChild(passesAA ? Common.UIString('Passes ') : Common.UIString('Fails '));
    this._contrastPassFailAA.createChild('strong').textContent = Common.UIString('AA (%s)', AA.toFixed(1));
    this._contrastAA.classList.toggle('pass', passesAA);
    this._contrastAA.classList.toggle('fail', !passesAA);

    var passesAAA = this._contrastInfo.contrastRatio() >= AAA;
    this._contrastPassFailAAA.textContent = '';
    this._contrastPassFailAAA.createTextChild(passesAAA ? Common.UIString('Passes ') : Common.UIString('Fails '));
    this._contrastPassFailAAA.createChild('strong').textContent = Common.UIString('AAA (%s)', AAA.toFixed(1));
    this._contrastAAA.classList.toggle('pass', passesAAA);
    this._contrastAAA.classList.toggle('fail', !passesAAA);

    this._contrastValueBubble.classList.toggle('contrast-fail', !passesAA);
    this._contrastValueBubble.classList.toggle('contrast-aa', passesAA);
    this._contrastValueBubble.classList.toggle('contrast-aaa', passesAAA);
    this._contrastValueBubble.style.color = this._contrastInfo.colorString();
    for (var i = 0; i < this._contrastValueBubbleIcons.length; i++)
      this._contrastValueBubbleIcons[i].style.setProperty('background', this._contrastInfo.colorString(), 'important');

    var isWhite = (this._contrastInfo.bgColor().hsla()[2] > 0.9);
    this._contrastValueBubble.style.background =
        /** @type {string} */ (this._contrastInfo.bgColor().asString(Common.Color.Format.RGBA));
    this._contrastValueBubble.classList.toggle('contrast-color-white', isWhite);

    if (isWhite) {
      this._contrastValueBubble.style.removeProperty('border-color');
    } else {
      this._contrastValueBubble.style.borderColor =
          /** @type {string} */ (this._contrastInfo.bgColor().asString(Common.Color.Format.RGBA));
    }
  }

  toggleVisible() {
    this._visible = !this._visible;
    this._contrastDetails.classList.toggle('visible', this._visible);
    if (this._visible)
      this._toggleMainColorPicker(false);
    else
      this._toggleBackgroundColorPicker(false);
  }

  hide() {
    this._contrastDetails.classList.remove('visible');
    this._toggleBackgroundColorPicker(false);
    this._updateContrastOverlayCallback();
  }

  /**
   * @return {!Element}
   */
  element() {
    return this._contrastDetails;
  }

  /**
   * @return {boolean}
   */
  visible() {
    return this._visible;
  }

  /**
   * @param {boolean=} enabled
   */
  _toggleBackgroundColorPicker(enabled) {
    if (enabled === undefined) {
      this._bgColorPicker.classList.toggle('active');
      enabled = this._bgColorPicker.classList.contains('active');
    } else {
      this._bgColorPicker.classList.toggle('active', enabled);
    }
    UI.ARIAUtils.setPressed(this._bgColorPicker, enabled);

    InspectorFrontendHost.setEyeDropperActive(enabled);
    if (enabled) {
      InspectorFrontendHost.events.addEventListener(
          InspectorFrontendHostAPI.Events.EyeDropperPickedColor, this._bgColorPickedBound);
    } else {
      InspectorFrontendHost.events.removeEventListener(
          InspectorFrontendHostAPI.Events.EyeDropperPickedColor, this._bgColorPickedBound);
    }
  }

  /**
   * @param {!Common.Event} event
   */
  _bgColorPicked(event) {
    var rgbColor = /** @type {!{r: number, g: number, b: number, a: number}} */ (event.data);
    var rgba = [rgbColor.r, rgbColor.g, rgbColor.b, (rgbColor.a / 2.55 | 0) / 100];
    var color = Common.Color.fromRGBA(rgba);
    this._contrastInfo.setBgColor(color);
    this.update();
    this._updateContrastOverlayCallback();
    InspectorFrontendHost.bringToFront();
  }

  /**
   * @param {!Event} event
   */
  _toggleContrastValueHovered(event) {
    if (!this._contrastValueBubble.classList.contains('contrast-fail'))
      return;

    if (event.type === 'mouseenter') {
      this._contrastValueBubble.classList.add('hover');
      for (var i = 0; i < this._contrastValueBubbleIcons.length; i++)
        this._contrastValueBubbleIcons[i].style.setProperty('background', '#333', 'important');
    } else {
      this._contrastValueBubble.classList.remove('hover');
      for (var i = 0; i < this._contrastValueBubbleIcons.length; i++) {
        this._contrastValueBubbleIcons[i].style.setProperty(
            'background', this._contrastInfo.colorString(), 'important');
      }
    }
  }
};

ColorPicker.ContrastDetails.Swatch = class {
  /**
   * @param {!Element} parentElement
   */
  constructor(parentElement) {
    this._parentElement = parentElement;
    this._swatchElement = parentElement.createChild('span', 'swatch contrast');
    this._swatchInnerElement = this._swatchElement.createChild('span', 'swatch-inner');
  }

  /**
   * @param {!Common.Color} color
   * @param {string=} colorString
   */
  setColor(color, colorString) {
    if (!colorString)
      colorString = /** @type {string} */ (color.asString(Common.Color.Format.RGBA));
    this._swatchInnerElement.style.background = colorString;
    UI.ARIAUtils.setAccessibleName(this._swatchElement, colorString);

    // Show border if the swatch is white.
    this._swatchElement.classList.toggle('swatch-inner-white', color.hsla()[2] > 0.9);
  }
};
