// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// @ts-check
'use strict';

/**
 * @fileoverview
 * UI classes and methods for the Tree View in the
 * Binary Size Analysis HTML report.
 */

{
  /**
   * @enum {number} Various byte units and the corresponding amount of bytes
   * that one unit represents.
   */
  const _BYTE_UNITS = {
    GiB: 1024 ** 3,
    MiB: 1024 ** 2,
    KiB: 1024 ** 1,
    B: 1024 ** 0,
  };
  /** Set of all byte units */
  const _BYTE_UNITS_SET = new Set(Object.keys(_BYTE_UNITS));

  const _icons = document.getElementById('icons');
  /**
   * @type {{[type:string]: SVGSVGElement}} Icon elements
   * that correspond to each symbol type.
   */
  const SYMBOL_ICONS = {
    D: _icons.querySelector('.foldericon'),
    C: _icons.querySelector('.componenticon'),
    F: _icons.querySelector('.fileicon'),
    b: _icons.querySelector('.bssicon'),
    d: _icons.querySelector('.dataicon'),
    r: _icons.querySelector('.readonlyicon'),
    t: _icons.querySelector('.codeicon'),
    v: _icons.querySelector('.vtableicon'),
    '*': _icons.querySelector('.generatedicon'),
    x: _icons.querySelector('.dexicon'),
    m: _icons.querySelector('.dexmethodicon'),
    p: _icons.querySelector('.localpakicon'),
    P: _icons.querySelector('.nonlocalpakicon'),
    o: _icons.querySelector('.othericon'), // used as default icon
  };
  const _CONTAINER_TYPES = new Set('DCF');

  // Templates for tree nodes in the UI.
  /** @type {HTMLTemplateElement} Template for leaves in the tree */
  const _leafTemplate = document.getElementById('treeitem');
  /** @type {HTMLTemplateElement} Template for trees */
  const _treeTemplate = document.getElementById('treefolder');

  // Infocard elements
  const _containerInfocard = document.getElementById('infocardcontainer');
  const _symbolInfocard = document.getElementById('infocardsymbol');

  const _symbolTree = document.getElementById('symboltree');

  /**
   * @type {HTMLCollectionOf<HTMLAnchorElement | HTMLSpanElement>}
   * HTMLCollection of all nodes. Updates itself automatically.
   */
  const _liveNodeList = document.getElementsByClassName('node');
  /**
   * @type {HTMLCollectionOf<HTMLSpanElement>}
   * HTMLCollection of all size elements. Updates itself automatically.
   */
  const _liveSizeSpanList = document.getElementsByClassName('size');

  /** @type {CanvasRenderingContext2D} Pie chart canvas from infocard */
  const _ctx = _containerInfocard
    .querySelector('.type-pie-info')
    .getContext('2d');
  const _tableBody = _containerInfocard.querySelector('tbody');

  /**
   * @type {WeakMap<HTMLElement, Readonly<TreeNode>>}
   * Associates UI nodes with the corresponding tree data object
   * so that event listeners and other methods can
   * query the original data.
   */
  const _uiNodeData = new WeakMap();

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

    ctx.fillStyle = color;
    // Move cursor to center, where line will start
    ctx.beginPath();
    ctx.moveTo(40, 40);
    // Move cursor to start of arc then draw arc
    ctx.arc(40, 40, 40, angleStart, angleEnd);
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
    } else {
      _tableBody.appendChild(row);
    }

    const sizeColumn = row.querySelector('.size');
    const percentColumn = row.querySelector('.percent');

    sizeColumn.textContent = size.toLocaleString(undefined, {
      minimumFractionDigits: 2,
      maximumFractionDigits: 2,
      useGrouping: true,
    });
    percentColumn.textContent = percentage.toLocaleString(undefined, {
      style: 'percent',
      minimumFractionDigits: 2,
      maximumFractionDigits: 2,
    });
  }

  function _updateHeader(infocard, symbol) {
    infocard.removeAttribute('hidden');
    const _getSizeLabels = state.has('method_count')
      ? _getMethodCountContents
      : _getSizeContents;
    const {title, element} = _getSizeLabels(symbol.size);

    const sizeInfo = infocard.querySelector('.size-info');
    dom.replace(
      sizeInfo,
      dom.createFragment([
        document.createTextNode(title),
        document.createTextNode(' ('),
        element,
        document.createTextNode(')'),
      ])
    );

    if (symbol.size < 0) {
      sizeInfo.classList.add('negative');
    } else {
      sizeInfo.classList.remove('negative');
    }

    const [path] = symbol.idPath.split(symbol.shortName, 1);
    const boldShortName = document.createElement('span');
    boldShortName.className = 'symbol-name-info';
    boldShortName.textContent = symbol.shortName;
    dom.replace(
      infocard.querySelector('.path-info'),
      dom.createFragment([document.createTextNode(path), boldShortName])
    );
  }

  function _updateInfocard(symbol) {
    if (_CONTAINER_TYPES.has(symbol.type[0])) {
      _updateHeader(_containerInfocard, symbol);
      _symbolInfocard.setAttribute('hidden', '');

      let angleStart = 0;
      let otherSize = 0;
      const extraRows = {..._INFO_ROWS};
      const sizeEntries = Object.entries(symbol.childSizes).sort(
        (a, b) => b[1] - a[1]
      );
      for (const [type, size] of sizeEntries) {
        const icon = _getIconTemplate(type, false);
        if (type === 'o') {
          otherSize += size;
        } else {
          const color = icon.getAttribute('fill');
          const percentage = size / symbol.size;

          angleStart = _drawSlice(_ctx, angleStart, percentage, color);

          _updateBreakdownRow(_INFO_ROWS[type], size, percentage);
          delete extraRows[type];
        }
      }

      for (const row of Object.values(extraRows)) {
        _updateBreakdownRow(row, 0, 0);
      }
      const otherPercentage = otherSize / symbol.size;
      _updateBreakdownRow(_INFO_ROWS.o, otherSize, otherPercentage);
      _drawSlice(
        _ctx,
        angleStart,
        otherPercentage,
        SYMBOL_ICONS.o.getAttribute('fill')
      );
    } else {
      _updateHeader(_symbolInfocard, symbol);
      _containerInfocard.setAttribute('hidden', '');

      const icon = _getIconTemplate(symbol.type[0]);
      const iconInfo = _symbolInfocard.querySelector('.icon-info');
      iconInfo.style.backgroundColor = icon.getAttribute('fill');
      icon.setAttribute('fill', '#fff');
      dom.replace(iconInfo, icon);

      const title = icon.querySelector('title').textContent;
      _symbolInfocard.querySelector('.type-info').textContent = title;
    }
  }

  let _pendingFrame;
  function _showInfocardOnHover(event) {
    const symbol = _uiNodeData.get(event.currentTarget);
    cancelAnimationFrame(_pendingFrame);
    _pendingFrame = requestAnimationFrame(() => _updateInfocard(symbol));
  }
  function _showInfocardOnFocus(event) {
    const symbol = _uiNodeData.get(event.target);
    cancelAnimationFrame(_pendingFrame);
    _pendingFrame = requestAnimationFrame(() => _updateInfocard(symbol));
  }

  /**
   * Create the contents for the size element of a tree node..
   * If in method count mode, size instead represents the amount of methods in
   * the node. In this case, don't append a unit at the end.
   * @param {number} methodCount Number of methods to use for the count text
   * @returns {{title:string,element:Node}} Object with hover text title and
   * size element body. Can be consumed by `_applySizeFunc()`
   */
  function _getMethodCountContents(methodCount) {
    const methodStr = methodCount.toLocaleString(undefined, {
      useGrouping: true,
    });

    return {
      element: document.createTextNode(methodStr),
      title: `${methodStr} method${methodCount === 1 ? '' : 's'}`,
    };
  }

  /**
   * Create the contents for the size element of a tree node.
   * The unit to use is selected from the current state, and the original number
   * of bytes will be displayed as hover text over the element.
   * @param {number} bytes Number of bytes to use for the size text
   * @returns {{title:string,element:Node}} Object with hover text title and
   * size element body. Can be consumed by `_applySizeFunc()`
   */
  function _getSizeContents(bytes) {
    // Get unit from query string, will fallback for invalid query
    const suffix = state.get('byteunit', {
      default: 'MiB',
      valid: _BYTE_UNITS_SET,
    });
    const value = _BYTE_UNITS[suffix];

    // Format the bytes as a number with 2 digits after the decimal point
    const text = (bytes / value).toLocaleString(undefined, {
      minimumFractionDigits: 2,
      maximumFractionDigits: 2,
    });
    const textNode = document.createTextNode(`${text} `);

    // Display the suffix with a smaller font
    const suffixElement = document.createElement('small');
    suffixElement.textContent = suffix;

    return {
      element: dom.createFragment([textNode, suffixElement]),
      title: bytes.toLocaleString(undefined, {useGrouping: true}) + ' bytes',
    };
  }

  /**
   * Returns the SVG icon template element corresponding to the given type.
   * @param {string} type Symbol type character.
   * @param {boolean} clone Set to false if you don't want a copy of the
   * SVG template.
   * @returns {SVGSVGElement}
   */
  function _getIconTemplate(type, clone = true) {
    const iconTemplate = SYMBOL_ICONS[type] || SYMBOL_ICONS.o;
    return clone ? iconTemplate.cloneNode(true) : iconTemplate;
  }

  /**
   * Replace the contexts of the size element for a tree node, using a
   * predefined function which returns a title string and DOM element.
   * @param {(size: number) => ({title:string,element:Node})} sizeFunc
   * @param {HTMLElement} sizeElement Element that should display the size
   * @param {number} bytes Number of bytes to use for the size text
   */
  function _applySizeFunc(sizeFunc, sizeElement, bytes) {
    const {title, element} = sizeFunc(bytes);

    // Replace the contents of '.size' and change its title
    dom.replace(sizeElement, element);
    sizeElement.title = title;

    if (bytes < 0) {
      sizeElement.classList.add('negative');
    } else {
      sizeElement.classList.remove('negative');
    }
  }

  /**
   * Sets focus to a new tree element while updating the element that last had
   * focus. The tabindex property is used to avoid needing to tab through every
   * single tree item in the page to reach other areas.
   * @param {number | HTMLElement} el Index of tree node in `_liveNodeList`
   */
  function _focusTreeElement(el) {
    const lastFocused = document.activeElement;
    if (_uiNodeData.has(lastFocused)) {
      lastFocused.tabIndex = -1;
    }
    const element = typeof el === 'number' ? _liveNodeList[el] : el;
    if (element != null) {
      element.tabIndex = 0;
      element.focus();
    }
  }

  /**
   * Click event handler to expand or close the child group of a tree.
   * @param {Event} event
   */
  function _toggleTreeElement(event) {
    event.preventDefault();

    /** @type {HTMLAnchorElement | HTMLSpanElement} */
    const link = event.currentTarget;
    const element = link.parentElement;
    const group = link.nextElementSibling;

    const isExpanded = element.getAttribute('aria-expanded') === 'true';
    if (isExpanded) {
      element.setAttribute('aria-expanded', 'false');
      dom.replace(group, null);
    } else {
      const data = _uiNodeData.get(link);
      const newElements = data.children.map(child => newTreeElement(child));
      if (newElements.length === 1) {
        // Open the inner element if it only has a single child.
        // Ensures nodes like "java"->"com"->"google" are opened all at once.
        newElements[0].querySelector('.node').click();
      }

      group.appendChild(dom.createFragment(newElements));
      element.setAttribute('aria-expanded', 'true');
    }
  }

  /**
   * Keydown event handler to move focus for the given element
   * @param {KeyboardEvent} event
   */
  function _handleKeyNavigation(event) {
    /** @type {HTMLAnchorElement | HTMLSpanElement} */
    const link = event.target;
    const focusIndex = Array.prototype.indexOf.call(_liveNodeList, link);

    /** Focus the tree element immediately following this one */
    function _focusNext() {
      if (focusIndex > -1 && focusIndex < _liveNodeList.length - 1) {
        event.preventDefault();
        _focusTreeElement(focusIndex + 1);
      }
    }

    /** Open or close the tree element */
    function _toggle() {
      event.preventDefault();
      link.click();
    }

    /** Focus the tree element at `index` if it starts with `char` */
    function _focusIfStartsWith(char, index) {
      const data = _uiNodeData.get(_liveNodeList[index]);
      if (data.shortName.startsWith(char)) {
        event.preventDefault();
        _focusTreeElement(index);
        return true;
      } else {
        return false;
      }
    }

    switch (event.key) {
      // Space should act like clicking or pressing enter & toggle the tree.
      case ' ':
        _toggle();
        break;
      // Move to previous focusable node
      case 'ArrowUp':
        if (focusIndex > 0) {
          event.preventDefault();
          _focusTreeElement(focusIndex - 1);
        }
        break;
      // Move to next focusable node
      case 'ArrowDown':
        _focusNext();
        break;
      // If closed tree, open tree. Otherwise, move to first child.
      case 'ArrowRight':
        if (_uiNodeData.get(link).children.length !== 0) {
          const isExpanded =
            link.parentElement.getAttribute('aria-expanded') === 'true';
          if (isExpanded) {
            _focusNext();
          } else {
            _toggle();
          }
        }
        break;
      // If opened tree, close tree. Otherwise, move to parent.
      case 'ArrowLeft':
        {
          const isExpanded =
            link.parentElement.getAttribute('aria-expanded') === 'true';
          if (isExpanded) {
            _toggle();
          } else {
            const groupList = link.parentElement.parentElement;
            if (groupList.getAttribute('role') === 'group') {
              event.preventDefault();
              _focusTreeElement(groupList.previousElementSibling);
            }
          }
        }
        break;
      // Focus first node
      case 'Home':
        event.preventDefault();
        _focusTreeElement(0);
        break;
      // Focus last node on screen
      case 'End':
        event.preventDefault();
        _focusTreeElement(_liveNodeList.length - 1);
        break;
      // Expand all sibling nodes
      case '*':
        const groupList = link.parentElement.parentElement;
        if (groupList.getAttribute('role') === 'group') {
          event.preventDefault();
          for (const li of groupList.children) {
            if (li.getAttribute('aria-expanded') !== 'true') {
              li.querySelector('.node').click();
            }
          }
        }
        break;
      // If a letter was pressed, find a node starting with that character.
      default:
        if (event.key.length === 1 && event.key.match(/\S/)) {
          for (let i = focusIndex + 1; i < _liveNodeList.length; i++) {
            if (_focusIfStartsWith(event.key, i)) return;
          }
          for (let i = 0; i < focusIndex; i++) {
            if (_focusIfStartsWith(event.key, i)) return;
          }
        }
        break;
    }
  }

  /**
   * Inflate a template to create an element that represents one tree node.
   * The element will represent a tree or a leaf, depending on if the tree
   * node object has any children. Trees use a slightly different template
   * and have click event listeners attached.
   * @param {TreeNode} data Data to use for the UI.
   * @returns {DocumentFragment}
   */
  function newTreeElement(data) {
    const isLeaf = data.children.length === 0;
    const template = isLeaf ? _leafTemplate : _treeTemplate;
    const element = document.importNode(template.content, true);

    // Associate clickable node & tree data
    /** @type {HTMLAnchorElement | HTMLSpanElement} */
    const link = element.querySelector('.node');
    _uiNodeData.set(link, Object.freeze(data));

    // Icons are predefined in the HTML through hidden SVG elements
    const type = data.type[0];
    const icon = _getIconTemplate(type);
    if (_CONTAINER_TYPES.has(type)) {
      const symbolIcon = _getIconTemplate(data.type[1], false);
      const symbolColor = symbolIcon.getAttribute('fill');
      const symbolName = symbolIcon.querySelector('title').textContent;

      icon.setAttribute('fill', symbolColor);
      icon.querySelector('title').textContent += ` - Mostly ${symbolName}`;
    }
    // Insert an SVG icon at the start of the link to represent type
    link.insertBefore(icon, link.firstElementChild);

    // Set the symbol name and hover text
    /** @type {HTMLSpanElement} */
    const symbolName = element.querySelector('.symbol-name');
    symbolName.textContent = data.shortName;
    symbolName.title = data.idPath;

    // Set the byte size and hover text
    const _getSizeLabels = state.has('method_count')
      ? _getMethodCountContents
      : _getSizeContents;
    _applySizeFunc(_getSizeLabels, element.querySelector('.size'), data.size);

    link.addEventListener('mouseover', _showInfocardOnHover);
    if (!isLeaf) {
      link.addEventListener('click', _toggleTreeElement);
    }

    return element;
  }

  // When the `byteunit` state changes, update all .size elements in the page
  form.byteunit.addEventListener('change', event => {
    // Update existing size elements with the new unit
    for (const sizeElement of _liveSizeSpanList) {
      const data = _uiNodeData.get(sizeElement.parentElement);
      _applySizeFunc(_getSizeContents, sizeElement, data.size);
    }
  });

  _symbolTree.addEventListener('keydown', _handleKeyNavigation);
  _symbolTree.addEventListener('focusin', _showInfocardOnFocus);

  self.newTreeElement = newTreeElement;
  self._symbolTree = _symbolTree;
}

{
  const blob = new Blob([
    `
    --INSERT_WORKER_CODE--
    `,
  ]);
  // We use a worker to keep large tree creation logic off the UI thread
  const worker = new Worker(URL.createObjectURL(blob));

  /**
   * Displays the given data as a tree view
   * @param {{data:{root:TreeNode,meta:object}}} param0
   */
  worker.onmessage = ({data}) => {
    /** @type {DocumentFragment} */
    const root = newTreeElement(data);
    /** @type {HTMLAnchorElement} */
    const node = root.querySelector('.node');
    // Expand the root UI node
    node.click();
    node.tabIndex = 0;

    dom.replace(_symbolTree, root);
  };

  /**
   * Loads the tree data given on a worker thread and replaces the tree view in
   * the UI once complete. Uses query string as state for the options.
   * @param {string} treeData JSON string to be parsed on the worker thread.
   */
  function loadTree(treeData) {
    // Post as a JSON string for better performance
    worker.postMessage(
      `{"tree":${treeData}, "options":"${location.search.slice(1)}"}`
    );
  }

  form.addEventListener('change', () => loadTree(tree_data));
  form.addEventListener('submit', event => {
    event.preventDefault();
    loadTree(tree_data);
  });

  self.loadTree = loadTree;
}
