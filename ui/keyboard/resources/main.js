// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
(function(exports) {
  /**
   * Alignment options for a keyset.
   * @param {Object=} opt_keyset The keyset to calculate the dimensions for.
   *    Defaults to the current active keyset.
   */
  var AlignmentOptions = function(opt_keyset) {
    var keyboard = document.getElementById('keyboard');
    var keyset = opt_keyset || keyboard.activeKeyset;
    this.calculate(keyset);
  }

  AlignmentOptions.prototype = {
    /**
     * The width of a regular key in logical pixels.
     * @type {number}
     */
    keyWidth: 0,

    /**
     * The horizontal space between two keys in logical pixels.
     * @type {number}
     */
    pitchX: 0,

    /**
     * The vertical space between two keys in logical pixels.
     * @type {number}
     */
    pitchY: 0,

    /**
     * The width in logical pixels the row should expand within.
     * @type {number}
     */
    availableWidth: 0,

    /**
     * The x-coordinate in logical pixels of the left most edge of the keyset.
     * @type {number}
     */
    offsetLeft: 0,

    /**
     * The x-coordinate of the right most edge in logical pixels of the keyset.
     * @type {number}
     */
    offsetRight: 0,

    /**
     * The height in logical pixels of all keys.
     * @type {number}
     */
    keyHeight: 0,

    /**
     * The height in logical pixels the keyset should stretch to fit.
     * @type {number}
     */
    availableHeight: 0,

    /**
     * The y-coordinate in logical pixels of the top most edge of the keyset.
     * @type {number}
     */
    offsetTop: 0,

    /**
     * The y-coordinate in logical pixels of the bottom most edge of the keyset.
     * @type {number}
     */
    offsetBottom: 0,

    /**
     * Recalculates the alignment options for a specific keyset.
     * @param {Object} keyset The keyset to align.
     */
    calculate: function (keyset) {
      var rows = keyset.querySelectorAll('kb-row').array();
      // Pick candidate row. This is the row with the most keys.
      var row = rows[0];
      var candidateLength = rows[0].childElementCount;
      for (var i = 1; i < rows.length; i++) {
        if (rows[i].childElementCount > candidateLength) {
          row = rows[i];
          candidateLength = rows[i].childElementCount;
        }
      }
      var allKeys = row.children;

      // Calculates widths first.
      // Weight of a single interspace.
      var pitches = keyset.pitch.split();
      var pitchWeightX;
      var pitchWeightY;
      pitchWeightX = parseFloat(pitches[0]);
      pitchWeightY = pitches.length < 2 ? pitchWeightX : parseFloat(pitch[1]);

      // Sum of all keys in the current row.
      var keyWeightSumX = 0;
      for (var i=0; i< allKeys.length; i++) {
        keyWeightSumX += allKeys[i].weight;
      }

      var interspaceWeightSumX = (allKeys.length -1) * pitchWeightX;
      // Total weight of the row in X.
      var totalWeightX = keyWeightSumX + interspaceWeightSumX +
          keyset.weightLeft + keyset.weightRight;

      var totalWeightY = (DEFAULT_KEY_WEIGHT_Y * rows.length) +
                        (pitchWeightY * (rows.length - 1)) +
                        keyset.weightTop +
                        keyset.weightBottom;

      // Calculate width and height of the window.
      var bounds = exports.getKeyboardBounds();

      var width = bounds.width;
      var height = bounds.height;
      var pixelPerWeightX = bounds.width/totalWeightX;
      var pixelPerWeightY = bounds.height/totalWeightY;

      if (keyset.align == LayoutAlignment.CENTER) {
        if (totalWeightX/bounds.width < totalWeightY/bounds.height) {
          pixelPerWeightY = bounds.height/totalWeightY;
          pixelPerWeightX = pixelPerWeightY;
          width = Math.floor(pixelPerWeightX * totalWeightX)
        } else {
          pixelPerWeightX = bounds.width/totalWeightX;
          pixelPerWeightY = pixelPerWeightX;
          height = Math.floor(pixelPerWeightY * totalWeightY);
        }
      }
      // Calculate pitch.
      this.pitchX = Math.floor(pitchWeightX * pixelPerWeightX);
      this.pitchY = Math.floor(pitchWeightY * pixelPerWeightY);

      // Convert weight to pixels on x axis.
      this.keyWidth = Math.floor(DEFAULT_KEY_WEIGHT_X * pixelPerWeightX);
      var offsetLeft = Math.floor(keyset.weightLeft * pixelPerWeightX);
      var offsetRight = Math.floor(keyset.weightRight * pixelPerWeightX);
      this.availableWidth = width - offsetLeft - offsetRight;

      // Calculates weight to pixels on the y axis.
      this.keyHeight = Math.floor(DEFAULT_KEY_WEIGHT_Y * pixelPerWeightY);
      var offsetTop = Math.floor(keyset.weightTop * pixelPerWeightY);
      var offsetBottom = Math.floor(keyset.weightBottom * pixelPerWeightY);
      this.availableHeight = height - offsetTop - offsetBottom;

      var dX = bounds.width - width;
      this.offsetLeft = offsetLeft + Math.floor(dX/2);
      this.offsetRight = offsetRight + Math.ceil(dX/2)

      var dY = bounds.height - height;
      this.offsetBottom = offsetBottom + dY;
      this.offsetTop = offsetTop;
    },
  };

  /**
   * Calculate width and height of the window.
   * @private
   * @return {Array.<String, number>} The bounds of the keyboard container.
   */
  function getKeyboardBounds_() {
    return {
      "width": window.innerWidth,
      "height": window.innerHeight,
    };
  }
  /**
   * Callback function for when the window is resized.
   */
  var onResize = function() {
    var bounds = exports.getKeyboardBounds();
    var height =  (bounds.width > ASPECT_RATIO * bounds.height) ?
        bounds.height : Math.floor(bounds.width / ASPECT_RATIO);
    var keyboard = $('keyboard');
    keyboard.style.fontSize = (height / FONT_SIZE_RATIO / ROW_LENGTH) + 'px';
    keyboard.stale = true;
    var keyset = keyboard.activeKeyset;
    if (keyset)
      realignAll();
  };

  /**
   * Keeps track of number of loaded keysets.
   * @param {number} n The number of keysets.
   * @param {function()} fn Callback function on completion.
   */
  var Counter = function(n, fn) {
    this.count = 0;
    this.nKeysets = n;
    this.callback = fn;
  }

  Counter.prototype = {
    tick: function() {
      this.count++;
      if (this.count == this.nKeysets)
        this.callback();
    }
  }

  /**
   * Keeps track of keysets loaded and triggers a realign when all are ready.
   * @type {Counter}
   */
  var alignmentCounter = undefined;

  /**
   * Request realignment for a new keyset that was just loaded.
   */
  function requestRealign () {
    var keyboard = $('keyboard');
    if (!keyboard.stale)
      return;
    if (!alignmentCounter) {
      var layout = keyboard.layout;
      var length =
          keyboard.querySelectorAll('kb-keyset[id^=' + layout + ']').length;
      alignmentCounter = new Counter(length, function(){
        realign(false);
        alignmentCounter = undefined;
      });
    }
    alignmentCounter.tick();
  }

  /**
   * Updates a specific key to the position specified.
   * @param {Object} key The key to update.
   * @param {number} width The new width of the key.
   * @param {number} height The new height of the key.
   * @param {number} left The left corner of the key.
   * @param {number} top The top corner of the key.
   */
  function updateKey(key, width, height, left, top) {
    key.style.position = 'absolute';
    key.style.width = width + 'px';
    key.style.height = (height - KEY_PADDING_TOP - KEY_PADDING_BOTTOM) + 'px';
    key.style.left = left + 'px';
    key.style.top = (top + KEY_PADDING_TOP) + 'px';
  }

  /**
   * Returns the key closest to given x-coordinate
   * @param {Array.<kb-key>} allKeys Sorted array of all possible key
   *     candidates.
   * @param {number} x The x-coordinate.
   * @param {number} pitch The pitch of the row.
   * @param {boolean} alignLeft whether to search with respect to the left or
   *   or right edge.
   */
  function findClosestKey(allKeys, x, pitch, alignLeft) {
    var n = allKeys.length;
    // Simple binary search.
    var binarySearch = function (start, end, testFn) {
      if (start >= end) {
        console.error("Unable to find key.");
        return;
      }
      var mid = Math.floor((start+end)/2);
      var result = testFn(mid);
      if (result == 0)
        return allKeys[mid];
      if (result < 0)
        return binarySearch(start, mid, testFn);
      else
        return binarySearch(mid + 1, end, testFn);
    }
    // Test function.
    var testFn = function(i) {
      var ERROR_THRESH = 1;
      var key = allKeys[i];
      var left = parseFloat(key.style.left);
      if (!alignLeft)
        left += parseFloat(key.style.width);
      var deltaRight = 0.5*(parseFloat(key.style.width) + pitch)
      deltaLeft = 0.5 * pitch;
      if (i > 0)
        deltaLeft += 0.5*parseFloat(allKeys[i-1].style.width);
      var high = Math.ceil(left + deltaRight) + ERROR_THRESH;
      var low = Math.floor(left - deltaLeft) - ERROR_THRESH;
      if (x <= high && x >= low)
        return 0;
      return x >= high? 1 : -1;
    }

    return binarySearch(0, allKeys.length -1, testFn);
  }

  /**
   * Redistributes the total width amongst the keys in the range provided.
   * @param {Array.<kb-key>} allKeys Ordered list of keys to stretch.
   * @param {number} pitch The space between two keys.
   * @param {number} xOffset The x-coordinate of the key who's index is start.
   * @param {number} width The total extraneous width to distribute.
   * @param {number} keyHeight The height of each key.
   * @param {number} yOffset The y-coordinate of the top edge of the row.
   */
  function redistribute(allKeys, pitch, xOffset, width, keyHeight, yOffset) {
    var weight = 0;
    for (var i = 0; i < allKeys.length; i++) {
      weight += allKeys[i].weight;
    }
    var availableWidth = width - (allKeys.length - 1) * pitch;
    var pixelsPerWeight = availableWidth / weight;
    for (var i = 0; i < allKeys.length; i++) {
      var keyWidth = Math.floor(allKeys[i].weight * pixelsPerWeight);
      if (i == allKeys.length -1) {
        keyWidth =  availableWidth;
      }
      updateKey(allKeys[i], keyWidth, keyHeight, xOffset, yOffset)
      availableWidth -= keyWidth;
      xOffset += keyWidth + pitch;
    }
  }

  /**
   * Aligns a row such that the spacebar is perfectly aligned with the row above
   * it. A precondition is that all keys in this row can be stretched as needed.
   * @param {!kb-row} row The current row to be aligned.
   * @param {!kb-row} prevRow The row above the current row.
   * @param {!AlignmentOptions} params The parameters used to align the keyset.
   * @param {number} heightOffset The height offset caused by the rows above.
   */
  function realignSpacebarRow(row, prevRow, params, heightOffset) {
    var allKeys = row.children;
    var stretchWeightBeforeSpace = 0;
    var stretchBefore = 0;
    var stretchWeightAfterSpace = 0;
    var stretchAfter = 0;
    var spaceIndex = -1;

    for (var i=0; i< allKeys.length; i++) {
      if (spaceIndex == -1) {
        if (allKeys[i].classList.contains('space')) {
          spaceIndex = i;
          continue;
        } else {
          stretchWeightBeforeSpace += allKeys[i].weight;
          stretchBefore++;
        }
      } else {
        stretchWeightAfterSpace += allKeys[i].weight;
        stretchAfter++;
      }
    }
    if (spaceIndex == -1) {
      console.error("No spacebar found in this row.");
      return;
    }
    var totalWeight = stretchWeightBeforeSpace +
                      stretchWeightAfterSpace +
                      allKeys[spaceIndex].weight;
    var widthForKeys = params.availableWidth -
                       (params.pitchX * (allKeys.length - 1 ))
    // Number of pixels to assign per unit weight.
    var pixelsPerWeight = widthForKeys/totalWeight;
    // Predicted left edge of the space bar.
    var spacePredictedLeft = params.offsetLeft +
                          (spaceIndex * params.pitchX) +
                          (stretchWeightBeforeSpace * pixelsPerWeight);
    var prevRowKeys = prevRow.children;
    // Find closest keys to the spacebar in order to align it to them.
    var leftKey =
        findClosestKey(prevRowKeys, spacePredictedLeft, params.pitchX, true);

    var spacePredictedRight = spacePredictedLeft +
        allKeys[spaceIndex].weight * (params.keyWidth/100);

    var rightKey =
        findClosestKey(prevRowKeys, spacePredictedRight, params.pitchX, false);

    var yOffset = params.offsetTop + heightOffset;
    // Fix left side.
    var leftEdge = parseFloat(leftKey.style.left);
    var leftWidth = leftEdge - params.offsetLeft - params.pitchX;
    var leftKeys = allKeys.array().slice(0, spaceIndex);
    redistribute(leftKeys,
                 params.pitchX,
                 params.offsetLeft,
                 leftWidth,
                 params.keyHeight,
                 yOffset);
    // Fix right side.
    var rightEdge = parseFloat(rightKey.style.left) +
        parseFloat(rightKey.style.width);
    var spacebarWidth = rightEdge - leftEdge;
    updateKey(allKeys[spaceIndex],
              spacebarWidth,
              params.keyHeight,
              leftEdge,
              yOffset);
    var rightWidth =
        params.availableWidth - (rightEdge - params.offsetLeft + params.pitchX);
    var rightKeys = allKeys.array().slice(spaceIndex + 1);
    redistribute(rightKeys,
                 params.pitchX,
                 rightEdge + params.pitchX,//xOffset.
                 rightWidth,
                 params.keyHeight,
                 yOffset);
  }

  /**
   * Realigns a given row based on the parameters provided.
   * @param {!kb-row} row The row to realign.
   * @param {!AlignmentOptions} params The parameters used to align the keyset.
   * @param {number} heightOffset The offset caused by rows above it.
   */
  function realignRow(row, params, heightOffset) {
    var all = row.children;
    var nStretch = 0;
    var stretchWeightSum = 0;
    var allSum = 0;
    // Keeps track of where to distribute pixels caused by round off errors.
    var deltaWidth = [];
    for (var i = 0; i < all.length; i++) {
      deltaWidth.push(0)
      var key = all[i];
      if (key.weight == DEFAULT_KEY_WEIGHT_X){
        allSum += params.keyWidth;
        continue;
      }
      nStretch++;
      var width =
          Math.floor((params.keyWidth/DEFAULT_KEY_WEIGHT_X) * key.weight);
      allSum += width;
      stretchWeightSum += key.weight;
    }
    var nRegular = all.length - nStretch;
    // Extra space.
    var extra = params.availableWidth -
                allSum -
                (params.pitchX * (all.length -1));
    var xOffset = params.offsetLeft;

    var alignment = row.align;
    switch (alignment) {
      case RowAlignment.STRETCH:
        var extraPerWeight = extra/stretchWeightSum;
        for (var i = 0; i < all.length; i++) {
          if (all[i].weight == DEFAULT_KEY_WEIGHT_X)
            continue;
          var delta = Math.floor(all[i].weight * extraPerWeight);
          extra -= delta;
          deltaWidth[i] = delta;
          // All left-over pixels assigned to right most key.
          nStretch--;
          if (nStretch == 0)
            deltaWidth[i] += extra;
        }
        break;
      case RowAlignment.CENTER:
        xOffset += Math.floor(extra/2)
        break;
      case RowAlignment.RIGHT:
        xOffset += extra;
        break;
      case RowAlignment.STRETCHRIGHT:
        deltaWidth[all.length - 1] = extra;
      default:
        break;
    };

    var yOffset = params.offsetTop + heightOffset;
    var left = xOffset;
    for (var i = 0; i < all.length; i++) {
      var key = all[i];
      var width = params.keyWidth;
      if (key.weight != DEFAULT_KEY_WEIGHT_X)
        width = Math.floor((params.keyWidth/DEFAULT_KEY_WEIGHT_X) * key.weight)
      width += deltaWidth[i];
      updateKey(key, width, params.keyHeight, left, yOffset)
      left += (width + params.pitchX);
    }
  }

  /**
   * Realigns the keysets in all layouts of the keyboard.
   */
  function realignAll() {
    var keyboard = $('keyboard');
    var layoutParams = {};

    var idToLayout = function(id) {
      var parts = id.split('-');
      parts.pop();
      return parts.join('-');
    }

    var keysets = keyboard.querySelectorAll('kb-keyset').array();
    for (var i=0; i< keysets.length; i++) {
      var keyset = keysets[i];
      var layout = idToLayout(keyset.id);
      // Caches the layouts size parameters since all keysets in the same layout
      // will have the same specs.
      if (!(layout in layoutParams))
        layoutParams[layout] = new AlignmentOptions(keyset);
      realignKeyset(keyset, layoutParams[layout]);
    }
  }

  /**
   * Realigns the keysets in the current layout of the keyboard.
   */
  function realign() {
    var keyboard = $('keyboard');
    var params = new AlignmentOptions();
    var layout = keyboard.layout;
    var keysets =
        keyboard.querySelectorAll('kb-keyset[id^=' + layout + ']').array();
    for (var i = 0; i<keysets.length ; i++) {
      realignKeyset(keysets[i], params);
    }
    keyboard.stale = false;
  }

  /*
   * Realigns a given keyset.
   * @param {Object} keyset The keyset to realign.
   * @param {!AlignmentOptions} params The parameters used to align the keyset.
   */
  function realignKeyset(keyset, params) {
    var rows = keyset.querySelectorAll('kb-row').array();
    var maxSize = getKeyboardBounds();
    var height =  (maxSize.width > ASPECT_RATIO * maxSize.height) ?
      maxSize.height : Math.floor(maxSize.width / ASPECT_RATIO);
    keyset.style.fontSize = (height / FONT_SIZE_RATIO / rows.length) + 'px';

    var heightOffset  = 0;
    for (var i = 0; i < rows.length; i++) {
      var row = rows[i];
      if (row.querySelector('.space') && (i > 1)) {
        realignSpacebarRow(row, rows[i-1], params, heightOffset)
      } else {
        realignRow(row, params, heightOffset);
      }
      heightOffset += (params.keyHeight + params.pitchY);
    }
  }
  window.addEventListener('realign', requestRealign);

  addEventListener('resize', onResize);
  addEventListener('load', onResize);

  exports.getKeyboardBounds = getKeyboardBounds_;
})(this);

/**
 * Recursively replace all kb-key-import elements with imported documents.
 * @param {!Document} content Document to process.
 */
function importHTML(content) {
  var dom = content.querySelector('template').createInstance();
  var keyImports = dom.querySelectorAll('kb-key-import');
  if (keyImports.length != 0) {
    keyImports.array().forEach(function(element) {
      if (element.importDoc(content)) {
        var generatedDom = importHTML(element.importDoc(content));
        element.parentNode.replaceChild(generatedDom, element);
      }
    });
  }
  return dom;
}

/**
 * Replace all kb-key-sequence elements with generated kb-key elements.
 * @param {!DocumentFragment} importedContent The imported dom structure.
 */
function expandHTML(importedContent) {
  var keySequences = importedContent.querySelectorAll('kb-key-sequence');
  if (keySequences.length != 0) {
    keySequences.array().forEach(function(element) {
      var generatedDom = element.generateDom();
      element.parentNode.replaceChild(generatedDom, element);
    });
  }
}

/**
  * Flatten the keysets which represents a keyboard layout. It has two steps:
  * 1) Replace all kb-key-import elements with imported document that associated
  *   with linkid.
  * 2) Replace all kb-key-sequence elements with generated DOM structures.
  * @param {!Document} content Document to process.
  */
function flattenKeysets(content) {
  var importedContent = importHTML(content);
  expandHTML(importedContent);
  return importedContent;
}

// Prevents all default actions of touch. Keyboard should use its own gesture
// recognizer.
addEventListener('touchstart', function(e) { e.preventDefault() });
addEventListener('touchend', function(e) { e.preventDefault() });
addEventListener('touchmove', function(e) { e.preventDefault() });
