// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

'use strict';

installClass('HTMLMarqueeElement', function(global, HTMLMarqueeElementPrototype) {

    var kDefaultScrollAmount = 6;
    var kDefaultScrollDelayMS = 85;
    var kMinimumScrollDelayMS = 60;

    var kDefaultLoopLimit = -1;

    var kBehaviorScroll = 'scroll';
    var kBehaviorSlide = 'slide';
    var kBehaviorAlternate = 'alternate';

    var kDirectionLeft = 'left';
    var kDirectionRight = 'right';
    var kDirectionUp = 'up';
    var kDirectionDown = 'down';

    var kPresentationalAttributes = [
        'bgcolor',
        'height',
        'hspace',
        'vspace',
        'width',
    ];

    var pixelLengthRegexp = /^\s*([\d.]+)\s*$/;
    var percentageLengthRegexp = /^\s*([\d.]+)\s*%\s*$/;

    function convertHTMLLengthToCSSLength(value) {
        var pixelMatch = value.match(pixelLengthRegexp);
        if (pixelMatch)
            return pixelMatch[1] + 'px';
        var percentageMatch = value.match(percentageLengthRegexp);
        if (percentageMatch)
            return percentageMatch[1] + '%';
        return null;
    }

    // FIXME: Consider moving these utility functions to PrivateScriptRunner.js.
    var kInt32Max = Math.pow(2, 31);

    function convertToLong(n) {
        // Using parseInt() is wrong but this aligns with the existing behavior of StringImpl::toInt().
        // FIXME: Implement a correct algorithm of the Web IDL value conversion.
        var value = parseInt(n);
        if (!isNaN(value) && -kInt32Max <= value && value < kInt32Max)
            return value;
        return NaN;
    }

    function reflectAttribute(prototype, attributeName, propertyName) {
        Object.defineProperty(prototype, propertyName, {
            get: function() {
                return this.getAttribute(attributeName) || '';
            },
            set: function(value) {
                this.setAttribute(attributeName, value);
            },
            configurable: true,
            enumerable: true,
        });
    }

    function reflectBooleanAttribute(prototype, attributeName, propertyName) {
        Object.defineProperty(prototype, propertyName, {
            get: function() {
                return this.hasAttribute(attributeName);
            },
            set: function(value) {
                if (value)
                    this.setAttribute(attributeName, '');
                else
                    this.removeAttribute(attributeName);
            },
        });
    }

    function defineInlineEventHandler(prototype, eventName) {
        var propertyName = 'on' + eventName;
        // FIXME: We should use symbols here instead.
        var functionPropertyName = propertyName + 'Function_';
        var eventHandlerPropertyName = propertyName + 'EventHandler_';
        Object.defineProperty(prototype, propertyName, {
            get: function() {
                var func = this[functionPropertyName];
                return func || null;
            },
            set: function(value) {
                var oldEventHandler = this[eventHandlerPropertyName];
                if (oldEventHandler)
                    this.removeEventListener(eventName, oldEventHandler);
                // Notice that we wrap |value| in an anonymous function so that the
                // author can't call removeEventListener themselves to unregister the
                // inline event handler.
                var newEventHandler = value ? function() { value.apply(this, arguments) } : null;
                if (newEventHandler)
                    this.addEventListener(eventName, newEventHandler);
                this[functionPropertyName] = value;
                this[eventHandlerPropertyName] = newEventHandler;
            },
        });
    }

    reflectAttribute(HTMLMarqueeElementPrototype, 'behavior', 'behavior');
    reflectAttribute(HTMLMarqueeElementPrototype, 'bgcolor', 'bgColor');
    reflectAttribute(HTMLMarqueeElementPrototype, 'direction', 'direction');
    reflectAttribute(HTMLMarqueeElementPrototype, 'height', 'height');
    reflectAttribute(HTMLMarqueeElementPrototype, 'hspace', 'hspace');
    reflectAttribute(HTMLMarqueeElementPrototype, 'vspace', 'vspace');
    reflectAttribute(HTMLMarqueeElementPrototype, 'width', 'width');
    reflectBooleanAttribute(HTMLMarqueeElementPrototype, 'truespeed', 'trueSpeed');

    defineInlineEventHandler(HTMLMarqueeElementPrototype, 'start');
    defineInlineEventHandler(HTMLMarqueeElementPrototype, 'finish');
    defineInlineEventHandler(HTMLMarqueeElementPrototype, 'bounce');

    HTMLMarqueeElementPrototype.createdCallback = function() {
        var shadow = this.createShadowRoot();
        var style = global.document.createElement('style');
    style.textContent = ':host { display: inline-block; width: -webkit-fill-available; overflow: hidden; text-align: initial; }' +
            ':host([direction="up"]), :host([direction="down"]) { height: 200px; }';
        shadow.appendChild(style);

        var mover = global.document.createElement('div');
        shadow.appendChild(mover);

        mover.appendChild(global.document.createElement('content'));

        this.loopCount_ = 0;
        this.mover_ = mover;
        this.player_ = null;
        this.continueCallback_ = null;

        for (var i = 0; i < kPresentationalAttributes.length; ++i)
            this.initializeAttribute_(kPresentationalAttributes[i]);
    };

    HTMLMarqueeElementPrototype.attachedCallback = function() {
        this.start();
    };

    HTMLMarqueeElementPrototype.detachedCallback = function() {
        this.stop();
    };

    HTMLMarqueeElementPrototype.attributeChangedCallback = function(name, oldValue, newValue) {
        switch (name) {
        case 'bgcolor':
            this.style.backgroundColor = newValue;
            break;
        case 'height':
            this.style.height = convertHTMLLengthToCSSLength(newValue);
            break;
        case 'hspace':
            var margin = convertHTMLLengthToCSSLength(newValue);
            this.style.marginLeft = margin;
            this.style.marginRight = margin;
            break;
        case 'vspace':
            var margin = convertHTMLLengthToCSSLength(newValue);
            this.style.marginTop = margin;
            this.style.marginBottom = margin;
            break;
        case 'width':
            this.style.width = convertHTMLLengthToCSSLength(newValue);
            break;
        case 'behavior':
        case 'direction':
            this.stop();
            this.loopCount_ = 0;
            this.start();
            break;
        }
    };

    HTMLMarqueeElementPrototype.initializeAttribute_ = function(name) {
        var value = this.getAttribute(name);
        if (value === null)
            return;
        this.attributeChangedCallback(name, null, value);
    };

    Object.defineProperty(HTMLMarqueeElementPrototype, 'scrollAmount', {
        get: function() {
            var value = this.getAttribute('scrollamount');
            var scrollAmount = convertToLong(value);
            if (isNaN(scrollAmount) || scrollAmount < 0)
                return kDefaultScrollAmount;
            return scrollAmount;
        },
        set: function(value) {
            if (value < 0)
                throw new DOMExceptionInPrivateScript("IndexSizeError", "The provided value (" + value + ") is negative.");
            this.setAttribute('scrollamount', value);
        },
    });

    Object.defineProperty(HTMLMarqueeElementPrototype, 'scrollDelay', {
        get: function() {
            var value = this.getAttribute('scrolldelay');
            var scrollDelay = convertToLong(value);
            if (isNaN(scrollDelay) || scrollDelay < 0)
                return kDefaultScrollDelayMS;
            return scrollDelay;
        },
        set: function(value) {
            if (value < 0)
                throw new DOMExceptionInPrivateScript("IndexSizeError", "The provided value (" + value + ") is negative.");
            this.setAttribute('scrolldelay', value);
        },
    });

    Object.defineProperty(HTMLMarqueeElementPrototype, 'loop', {
        get: function() {
            var value = this.getAttribute('loop');
            var loop = convertToLong(value);
            if (isNaN(loop) || loop <= 0)
                return kDefaultLoopLimit;
            return loop;
        },
        set: function(value) {
            if (value <= 0 && value != -1)
                throw new DOMExceptionInPrivateScript("IndexSizeError", "The provided value (" + value + ") is neither positive nor -1.");
            this.setAttribute('loop', value);
        },
    });

    HTMLMarqueeElementPrototype.getGetMetrics_ = function() {
        this.mover_.style.width = '-webkit-max-content';

        var moverStyle = global.getComputedStyle(this.mover_);
        var marqueeStyle = global.getComputedStyle(this);

        var metrics = {};
        metrics.contentWidth = parseInt(moverStyle.width);
        metrics.contentHeight = parseInt(moverStyle.height);
        metrics.marqueeWidth = parseInt(marqueeStyle.width);
        metrics.marqueeHeight = parseInt(marqueeStyle.height);

        this.mover_.style.width = '';
        return metrics;
    };

    HTMLMarqueeElementPrototype.getAnimationParameters_ = function() {
        var metrics = this.getGetMetrics_();

        var totalWidth = metrics.marqueeWidth + metrics.contentWidth;
        var totalHeight = metrics.marqueeHeight + metrics.contentHeight;

        var innerWidth = metrics.marqueeWidth - metrics.contentWidth;
        var innerHeight = metrics.marqueeHeight - metrics.contentHeight;

        var parameters = {};

        switch (this.behavior) {
        case kBehaviorScroll:
        default:
            switch (this.direction) {
            case kDirectionLeft:
            default:
                parameters.transformBegin = 'translateX(' + metrics.marqueeWidth + 'px)';
                parameters.transformEnd = 'translateX(-100%)';
                parameters.distance = totalWidth;
                break;
            case kDirectionRight:
                parameters.transformBegin = 'translateX(-' + metrics.contentWidth + 'px)';
                parameters.transformEnd = 'translateX(' + metrics.marqueeWidth + 'px)';
                parameters.distance = totalWidth;
                break;
            case kDirectionUp:
                parameters.transformBegin = 'translateY(' + metrics.marqueeHeight + 'px)';
                parameters.transformEnd = 'translateY(-' + metrics.contentHeight + 'px)';
                parameters.distance = totalHeight;
                break;
            case kDirectionDown:
                parameters.transformBegin = 'translateY(-' + metrics.contentHeight + 'px)';
                parameters.transformEnd = 'translateY(' + metrics.marqueeHeight + 'px)';
                parameters.distance = totalHeight;
                break;
            }
            break;
        case kBehaviorAlternate:
            switch (this.direction) {
            case kDirectionLeft:
            default:
                parameters.transformBegin = 'translateX(' + innerWidth + 'px)';
                parameters.transformEnd = 'translateX(0)';
                parameters.distance = innerWidth;
                break;
            case kDirectionRight:
                parameters.transformBegin = 'translateX(0)';
                parameters.transformEnd = 'translateX(' + innerWidth + 'px)';
                parameters.distance = innerWidth;
                break;
            case kDirectionUp:
                parameters.transformBegin = 'translateY(' + innerHeight + 'px)';
                parameters.transformEnd = 'translateY(0)';
                parameters.distance = innerHeight;
                break;
            case kDirectionDown:
                parameters.transformBegin = 'translateY(0)';
                parameters.transformEnd = 'translateY(' + innerHeight + 'px)';
                parameters.distance = innerHeight;
                break;
            }

            if (this.loopCount_ % 2) {
                var transform = parameters.transformBegin;
                parameters.transformBegin = parameters.transformEnd;
                parameters.transformEnd = transform;
            }

            break;
        case kBehaviorSlide:
            switch (this.direction) {
            case kDirectionLeft:
            default:
                parameters.transformBegin = 'translateX(' + metrics.marqueeWidth + 'px)';
                parameters.transformEnd = 'translateX(0)';
                parameters.distance = metrics.marqueeWidth;
                break;
            case kDirectionRight:
                parameters.transformBegin = 'translateX(-' + metrics.contentWidth + 'px)';
                parameters.transformEnd = 'translateX(' + innerWidth + 'px)';
                parameters.distance = metrics.marqueeWidth;
                break;
            case kDirectionUp:
                parameters.transformBegin = 'translateY(' + metrics.marqueeHeight + 'px)';
                parameters.transformEnd = 'translateY(0)';
                parameters.distance = metrics.marqueeHeight;
                break;
            case kDirectionDown:
                parameters.transformBegin = 'translateY(-' + metrics.contentHeight + 'px)';
                parameters.transformEnd = 'translateY(' + innerHeight + 'px)';
                parameters.distance = metrics.marqueeHeight;
                break;
            }
            break;
        }

        return parameters
    };

    HTMLMarqueeElementPrototype.shouldContinue_ = function() {
        var loop = this.loop;

        // By default, slide loops only once.
        if (loop <= 0 && this.behavior === kBehaviorSlide)
            loop = 1;

        if (loop <= 0)
            return true;
        return this.loopCount_ < loop;
    };

    HTMLMarqueeElementPrototype.continue_ = function() {
        if (!this.shouldContinue_()) {
            this.player_ = null;
            this.dispatchEvent(new Event('finish', false, true));
            return;
        }

        var parameters = this.getAnimationParameters_();

        var player = this.mover_.animate([
            { transform: parameters.transformBegin },
            { transform: parameters.transformEnd },
        ], {
            duration: parameters.distance * this.scrollDelay / this.scrollAmount,
            fill: 'forwards',
        });

        this.player_ = player;

        player.addEventListener('finish', function() {
            if (player != this.player_)
                return;
            ++this.loopCount_;
            this.continue_();
            if (this.player_ && this.behavior === kBehaviorAlternate)
                this.dispatchEvent(new Event('bounce', false, true));
        }.bind(this));
    };

    HTMLMarqueeElementPrototype.start = function() {
        if (this.continueCallback_ || this.player_)
            return;
        this.continueCallback_ = global.requestAnimationFrame(function() {
            this.continueCallback_ = null;
            this.continue_();
        }.bind(this));
        this.dispatchEvent(new Event('start', false, true));
    };

    HTMLMarqueeElementPrototype.stop = function() {
        if (!this.continueCallback_ && !this.player_)
            return;

        if (this.continueCallback_) {
            global.cancelAnimationFrame(this.continueCallback_);
            this.continueCallback_ = null;
            return;
        }

        // FIXME: Rather than canceling the animation, we really should just
        // pause the animation, but the pause function is still flagged as
        // experimental.
        if (this.player_) {
            var player = this.player_;
            this.player_ = null;
            player.cancel();
        }
    };

    // FIXME: We have to inject this HTMLMarqueeElement as a custom element in order to make
    // createdCallback, attachedCallback, detachedCallback and attributeChangedCallback workable.
    // global.document.registerElement('i-marquee', {
    //    prototype: HTMLMarqueeElementPrototype,
    // });
});
