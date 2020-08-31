// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import {assert} from 'chrome://resources/js/assert.m.js';
import {loadTimeData} from 'chrome://resources/js/load_time_data.m.js';
import {createEmptySearchBubble, highlight, Range, stripDiacritics} from 'chrome://resources/js/search_highlight_utils.m.js';

/**
 * @param {!HTMLElement} element The element to update. Element should have a
 *     shadow root.
 * @param {?RegExp} query The current search query
 * @param {!Map<!Node, number>} bubbles A map of bubbles created / results found
 *     so far.
 * @return {!Array<!Node>} The highlight wrappers that were created.
 */
export function updateHighlights(element, query, bubbles) {
  const highlights = [];
  if (!query) {
    return highlights;
  }

  assert(query.global);

  element.shadowRoot.querySelectorAll('.searchable').forEach(childElement => {
    childElement.childNodes.forEach(node => {
      if (node.nodeType !== Node.TEXT_NODE) {
        return;
      }

      const textContent = node.nodeValue;
      if (textContent.trim().length === 0) {
        return;
      }

      const strippedText = stripDiacritics(textContent);
      /** @type {!Array<!Range>} */
      const ranges = [];
      for (let match; match = query.exec(strippedText);) {
        ranges.push({start: match.index, length: match[0].length});
      }

      if (ranges.length > 0) {
        // Don't highlight <select> nodes, yellow rectangles can't be
        // displayed within an <option>.
        if (node.parentNode.nodeName === 'OPTION') {
          // The bubble should be parented by the select node's parent.
          // Note: The bubble's ::after element, a yellow arrow, will not
          // appear correctly in print preview without SPv175 enabled. See
          // https://crbug.com/817058.
          // TODO(crbug.com/1038464): turn on horizontallyCenter when we fix
          // incorrect positioning caused by scrollbar width changing after
          // search finishes.
          const bubble = createEmptySearchBubble(
              /** @type {!Node} */ (assert(node.parentNode.parentNode)),
              /* horizontallyCenter= */ false);
          const numHits = ranges.length + (bubbles.get(bubble) || 0);
          bubbles.set(bubble, numHits);
          const msgName = numHits === 1 ? 'searchResultBubbleText' :
                                          'searchResultsBubbleText';
          bubble.firstChild.textContent =
              loadTimeData.getStringF(msgName, numHits);
        } else {
          highlights.push(highlight(node, ranges));
        }
      }
    });
  });

  return highlights;
}
