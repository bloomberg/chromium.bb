/*
 * Copyright 2010, Google Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/**
 * Manages a sequence of bubble objects.
 *
 * @constructor
 * @param {!Array<!Bubble.BubbleType>} requestedSequence
 * @param {o3d.Pack} pack
 */
var BubbleManager = function(requestedSequence, pack) {
  this.bubbleQueue_ = [];
  this.nextIndex_ = 0;
  this.pack_ = pack;

  this.container_ = $('#bubble-bubbles');
  this.container_.html(" "); // reset
  this.counter_ = $('#bubble-count-span');

  for (var i = 0; i < requestedSequence.length; i++) {
    var bubble = new Bubble(requestedSequence[i], this.pack_);
    var el = g_paramArray.createParam(i, 'ParamFloat4');
    el.bind(bubble.param);

    this.bubbleQueue_.push(bubble);
    var img = new Image();
    var imageSrc;
    var type = requestedSequence[i];
    switch (type) {
      case Bubble.SMALL:
        imageSrc = "resources/bubble3.png";
        break;
      case Bubble.MEDIUM:
        imageSrc = "resources/bubble2.png";
        break;
      case Bubble.LARGE:
        imageSrc = "resources/bubble1.png";
        break;
    }
    img.src = imageSrc;
    img.alt = "Bubble " + i;
    this.container_.append(img);
    this.counter_.text("" + requestedSequence.length);
  }
};

/**
 * The bubbles queued to be released.
 * @type {!Array<!Bubble>}
 * @private
 */
BubbleManager.prototype.bubbleQueue_ = [];

/**
 * The DOM container to show the queue of bubbles.
 * @type {DOM.Element}
 * @private
 */
BubbleManager.prototype.container_ = null;

/**
 * The DOM container that keeps the count of bubbles remaining.
 * @type {DOM.Element}
 * @private
 */
BubbleManager.prototype.counter_ = null;

/**
 * A pointer to the current position in the bubble queue.
 * @type {number}
 * @private
 */
BubbleManager.prototype.nextIndex_ = 0;

/**
 * A pack contains all the transforms, geometry and shaders of the bubbles it
 * manages.
 * @type {o3d.Pack}
 * @private
 */
BubbleManager.prototype.pack_ = 0;

/**
 * Moves a bubble from the head of the queue to the tail of the bubblesInPlay
 * list and returns that bubble.
 *
 * If the queue is empty, returns null.
 *
 * @return {Bubble}
 */
BubbleManager.prototype.pop = function() {
  if (this.nextIndex_ == this.bubbleQueue_.length) {
    return null;
  }
  var children = this.container_.children();
  var img = children[this.nextIndex_];
  img.className = "used";
  $(img).slideUp();
  this.counter_.text(this.bubbleQueue_.length - this.nextIndex_ - 1) + "";
  return this.bubbleQueue_[this.nextIndex_++];
};

/**
 * Returns the bubble at the head of the queue, but does not pop it.
 * If the queue is empty, returns null.
 *
 * @return {Bubble}
 */
BubbleManager.prototype.peek = function() {
  return (this.bubbleQueue_.length == this.nextIndex_) ? null :
      this.bubbleQueue_[this.nextIndex_];
};

/**
 * Advances each bubble in the active queue by one timestep.
 */
BubbleManager.prototype.step = function() {
  for (var i = 0; i < this.nextIndex_; i++) {
    var bubble = this.bubbleQueue_[i];
    bubble.step();
  }
};
