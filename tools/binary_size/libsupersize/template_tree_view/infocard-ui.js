// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// @ts-check
'use strict';

/**
 * @fileoverview
 * UI classes and methods for the info cards that display informations about
 * symbols as the user hovers or focuses on them.
 */

{
  // Infocard elements
  const _containerInfocard = document.getElementById('infocard-container');
  const _symbolInfocard = document.getElementById('infocard-symbol');

  const _tableBody = _containerInfocard.querySelector('tbody');

  /** @type {CanvasRenderingContext2D} Pie chart canvas from infocard */
  const _ctx = _containerInfocard
    .querySelector('.type-pie-info')
    .getContext('2d');

  /**
   * @type {{[type:string]: HTMLTableRowElement}} Rows in the container infocard
   * that represent a particular symbol type.
   */
  const _INFO_ROWS = {
    b: _containerInfocard.querySelector('.bss-info'),
    d: _containerInfocard.querySelector('.data-info'),
    r: _containerInfocard.querySelector('.rodata-info'),
    t: _containerInfocard.querySelector('.text-info'),
    v: _containerInfocard.querySelector('.vtable-info'),
    '*': _containerInfocard.querySelector('.gen-info'),
    x: _containerInfocard.querySelector('.dexnon-info'),
    m: _containerInfocard.querySelector('.dex-info'),
    p: _containerInfocard.querySelector('.pak-info'),
    P: _containerInfocard.querySelector('.paknon-info'),
    o: _containerInfocard.querySelector('.other-info'),
  };

  const _CANVAS_RADIUS = 40;

  /** Update the DPI of the canvas for zoomed in and high density screens. */
  function _updateCanvasDpi() {
    _ctx.canvas.height = _CANVAS_RADIUS * 2 * devicePixelRatio;
    _ctx.canvas.width = _CANVAS_RADIUS * 2 * devicePixelRatio;
    _ctx.scale(devicePixelRatio, devicePixelRatio);
  }
  _updateCanvasDpi();
  window.addEventListener('resize', _updateCanvasDpi);

  /**
   * Draw a slice of a pie chart.
   * @param {CanvasRenderingContext2D} ctx
   * @param {number} angleStart Starting angle, in radians.
   * @param {number} percentage Percentage of circle to draw.
   * @param {string} color Color of the pie slice.
   * @returns {number} Ending angle, in radians.
   */
  function _drawSlice(ctx, angleStart, percentage, color) {
    const arcLength = percentage * 2 * Math.PI;
    const angleEnd = angleStart + arcLength;
    if (arcLength === 0) return angleEnd;

    // Update DOM
    ctx.fillStyle = color;
    // Move cursor to center, where line will start
    ctx.beginPath();
    ctx.moveTo(40, 40);
    // Move cursor to start of arc then draw arc
    ctx.arc(40, 40, _CANVAS_RADIUS, angleStart, angleEnd);
    // Move cursor back to center
    ctx.closePath();
    ctx.fill();

    return angleEnd;
  }

  /**
   * Update a row in the breakdown table with the given values.
   * @param {HTMLTableRowElement} row
   * @param {number} size Total size of the symbols of a given type in a
   * container.
   * @param {number} percentage How much the size represents in relation to the
   * total size of the symbols in the container.
   */
  function _updateBreakdownRow(row, size, percentage) {
    if (size === 0) {
      if (row.parentElement != null) {
        _tableBody.removeChild(row);
      }
      return;
    }

    const sizeColumn = row.querySelector('.size');
    const percentColumn = row.querySelector('.percent');

    const sizeString = size.toLocaleString(undefined, {
      minimumFractionDigits: 2,
      maximumFractionDigits: 2,
      useGrouping: true,
    });
    const percentString = percentage.toLocaleString(undefined, {
      style: 'percent',
      minimumFractionDigits: 2,
      maximumFractionDigits: 2,
    });

    // Update DOM
    sizeColumn.textContent = sizeString;
    percentColumn.textContent = percentString;
    _tableBody.appendChild(row);
  }

  /**
   * @param {TreeNode} symbol
   * @param {GetSize} getSizeLabel
   * @returns {[DocumentFragment, DocumentFragment]}
   */
  function _buildHeaderFragments(symbol, getSizeLabel) {
    // Create size info fragment, such as "1,234 bytes (1.23 KiB)"
    const {title, element} = getSizeLabel(
      symbol.size,
      state.get('byteunit', {default: 'MiB', valid: _BYTE_UNITS_SET})
    );
    const sizeInfoFragment = dom.createFragment([
      document.createTextNode(`${title} (`),
      element,
      document.createTextNode(')'),
    ]);

    // Create path fragment where the shortName is highlighted.
    const path = symbol.idPath.slice(0, symbol.shortNameIndex);
    const boldShortName = dom.textElement(
      'span',
      shortName(symbol),
      'symbol-name-info'
    );
    const pathInfoFragment = dom.createFragment([
      document.createTextNode(path),
      boldShortName,
    ]);

    return [sizeInfoFragment, pathInfoFragment];
  }

  function _insertHeaderFragments(
    infocard,
    symbol,
    [sizeInfoFragment, pathInfoFragment]
  ) {
    const sizeInfo = infocard.querySelector('.size-info');
    // Update DOM
    infocard.removeAttribute('hidden');
    if (symbol.size < 0) {
      sizeInfo.classList.add('negative');
    } else {
      sizeInfo.classList.remove('negative');
    }

    dom.replace(sizeInfo, sizeInfoFragment);
    dom.replace(infocard.querySelector('.path-info'), pathInfoFragment);
  }

  /**
   * Update DOM for the container infocard
   * @param {TreeNode} container
   * @param {Array<[string, number]>} sizeEntries
   * @param {{[type: string]: HTMLTableRowElement}} extraRows
   * @param {[DocumentFragment, DocumentFragment]} headerFragments
   */
  function _updateContainerInfocard(
    container,
    sizeEntries,
    extraRows,
    headerFragments
  ) {
    let angleStart = 0;
    for (const [type, size] of sizeEntries) {
      delete extraRows[type];
      const {color} = getIconStyle(type);
      const percentage = size / container.size;

      // Update DOM
      angleStart = _drawSlice(_ctx, angleStart, percentage, color);
      _updateBreakdownRow(_INFO_ROWS[type], size, percentage);
    }

    // Hide unused types
    for (const row of Object.values(extraRows)) {
      // Update DOM
      _updateBreakdownRow(row, 0, 0);
    }

    // Update DOM
    _insertHeaderFragments(_containerInfocard, container, headerFragments);
    _symbolInfocard.setAttribute('hidden', '');
  }

  /**
   * Update DOM for the symbol infocard
   * @param {TreeNode} symbol
   * @param {SVGSVGElement} icon
   * @param {HTMLDivElement} iconInfo
   * @param {[DocumentFragment, DocumentFragment]} headerFragments
   */
  function _updateSymbolInfocard(symbol, icon, iconInfo, headerFragments) {
    const typeInfo = icon.querySelector('title').textContent;
    // Update DOM
    iconInfo.style.backgroundColor = icon.getAttribute('fill');
    icon.setAttribute('fill', '#fff');
    dom.replace(iconInfo, icon);

    _symbolInfocard.querySelector('.type-info').textContent = typeInfo;
    _insertHeaderFragments(_symbolInfocard, symbol, headerFragments);
    _containerInfocard.setAttribute('hidden', '');
  }

  let _pendingFrame;

  /**
   * Displays an infocard for the given symbol on the next frame.
   * @param {TreeNode} symbol
   * @param {GetSize} getSizeLabel
   */
  function displayInfocard(symbol, getSizeLabel) {
    cancelAnimationFrame(_pendingFrame);
    const headerFragments = _buildHeaderFragments(symbol, getSizeLabel);

    if (_CONTAINER_TYPE_SET.has(symbol.type[0])) {
      const extraRows = {..._INFO_ROWS};
      const sizeEntries = Object.entries(symbol.childSizes).sort(
        (a, b) => b[1] - a[1]
      );

      _pendingFrame = requestAnimationFrame(() =>
        _updateContainerInfocard(
          symbol,
          sizeEntries,
          extraRows,
          headerFragments
        )
      );
    } else {
      const icon = getIconTemplate(symbol.type[0]);
      /** @type {HTMLDivElement} */
      const iconInfo = _symbolInfocard.querySelector('.icon-info');

      _pendingFrame = requestAnimationFrame(() =>
        _updateSymbolInfocard(symbol, icon, iconInfo, headerFragments)
      );
    }
  }

  self.displayInfocard = displayInfocard;
}
