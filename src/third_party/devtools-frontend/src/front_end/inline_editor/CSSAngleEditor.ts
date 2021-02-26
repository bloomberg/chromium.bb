// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import * as Common from '../common/common.js';
import * as LitHtml from '../third_party/lit-html/lit-html.js';

import {Angle, AngleUnit, get2DTranslationsForAngle, getAngleFromRadians, getNewAngleFromEvent, getRadiansFromAngle} from './CSSAngleUtils.js';

const {render, html} = LitHtml;
const styleMap = LitHtml.Directives.styleMap;

const ClockDialLength = 6;

export interface CSSAngleEditorData {
  angle: Angle;
  onAngleUpdate: (angle: Angle) => void;
  background: string;
}

export class CSSAngleEditor extends HTMLElement {
  private readonly shadow = this.attachShadow({mode: 'open'});
  private angle: Angle = {
    value: 0,
    unit: AngleUnit.Rad,
  };
  private onAngleUpdate?: (angle: Angle) => void;
  private background = '';
  private clockRadius = 77 / 2;  // By default the clock is 77 * 77.
  private dialTemplates?: LitHtml.TemplateResult[];
  private mousemoveThrottler = new Common.Throttler.Throttler(16.67 /* 60fps */);

  set data(data: CSSAngleEditorData) {
    this.angle = data.angle;
    this.onAngleUpdate = data.onAngleUpdate;
    this.background = data.background;
    this.render();
  }

  private updateAngleFromMousePosition(mouseX: number, mouseY: number, shouldSnapToMultipleOf15Degrees: boolean): void {
    const clock = this.shadow.querySelector('.clock');
    if (!clock || !this.onAngleUpdate) {
      return;
    }

    const {top, right, bottom, left} = clock.getBoundingClientRect();
    this.clockRadius = (right - left) / 2;
    const clockCenterX = (left + right) / 2;
    const clockCenterY = (bottom + top) / 2;
    const radian = -Math.atan2(mouseX - clockCenterX, mouseY - clockCenterY) + Math.PI;
    if (shouldSnapToMultipleOf15Degrees) {
      const multipleInRadian = getRadiansFromAngle({
        value: 15,
        unit: AngleUnit.Deg,
      });
      const closestMultipleOf15Degrees = Math.round(radian / multipleInRadian) * multipleInRadian;
      this.onAngleUpdate(getAngleFromRadians(closestMultipleOf15Degrees, this.angle.unit));
    } else {
      this.onAngleUpdate(getAngleFromRadians(radian, this.angle.unit));
    }
  }

  private onEditorMousedown(event: MouseEvent): void {
    event.stopPropagation();
    this.updateAngleFromMousePosition(event.pageX, event.pageY, event.shiftKey);
  }

  private onEditorMousemove(event: MouseEvent): void {
    const isPressed = event.buttons === 1;
    if (!isPressed) {
      return;
    }

    this.mousemoveThrottler.schedule(() => {
      this.updateAngleFromMousePosition(event.pageX, event.pageY, event.shiftKey);
      return Promise.resolve();
    });
  }

  private onEditorWheel(event: WheelEvent): void {
    if (!this.onAngleUpdate) {
      return;
    }

    const newAngle = getNewAngleFromEvent(this.angle, event);
    if (newAngle) {
      this.onAngleUpdate(newAngle);
    }

    event.preventDefault();
  }

  private render() {
    const clockStyles = {
      background: this.background,
    };

    const {translateX, translateY} = get2DTranslationsForAngle(this.angle, this.clockRadius / 2);
    const handStyles = {
      transform: `translate(${translateX}px, ${translateY}px) rotate(${this.angle.value}${this.angle.unit})`,
    };

    // Disabled until https://crbug.com/1079231 is fixed.
    // clang-format off
    render(html`
      <style>
        .clock, .pointer, .center, .hand, .dial {
          position: absolute;
        }

        .clock {
          top: 6px;
          width: 6em;
          height: 6em;
          background-color: white;
          border: 0.5em solid var(--border-color);
          border-radius: 9em;
          box-shadow: var(--drop-shadow), inset 0 0 15px hsl(0 0% 0% / 25%);
          transform: translateX(-3em);
        }

        :host-context(.-theme-with-dark-background) .clock {
          background-color: hsl(225 5% 27%);
        }

        .pointer {
          margin: auto;
          top: 0;
          left: -0.4em;
          right: 0;
          width: 0;
          height: 0;
          border-style: solid;
          border-width: 0 0.9em 0.9em 0.9em;
          border-color: transparent transparent var(--border-color) transparent;
        }

        .center, .hand, .dial {
          margin: auto;
          top: 0;
          left: 0;
          right: 0;
          bottom: 0;
        }

        .center, .hand {
          box-shadow: 0 0 2px hsl(0 0% 0% / 20%);
        }

        :host-context(.-theme-with-dark-background) .center,
        :host-context(.-theme-with-dark-background) .hand {
          box-shadow: 0 0 2px hsl(0 0% 0% / 60%);
        }

        .center {
          width: 0.7em;
          height: 0.7em;
          border-radius: 10px;
        }

        .dial {
          width: 2px;
          height: ${ClockDialLength}px;
          background-color: var(--dial-color);
          border-radius: 1px;
        }

        .hand {
          height: 50%;
          width: 0.3em;
          background: var(--accent-fg-color);
        }

        .hand::before {
          content: '';
          display: inline-block;
          position: absolute;
          top: -0.6em;
          left: -0.35em;
          width: 1em;
          height: 1em;
          border-radius: 1em;
          cursor: pointer;
          box-shadow: 0 0 5px hsl(0 0% 0% / 30%);
        }

        :host-context(.-theme-with-dark-background) .hand::before {
          box-shadow: 0 0 5px hsl(0 0% 0% / 80%);
        }

        .hand::before,
        .center {
          background-color: var(--accent-fg-color);
        }
      </style>

      <div class="editor">
        <span class="pointer"></span>
        <div
          class="clock"
          style=${styleMap(clockStyles)}
          @mousedown=${this.onEditorMousedown}
          @mousemove=${this.onEditorMousemove}
          @wheel=${this.onEditorWheel}>
          ${this.renderDials()}
          <div class="hand" style=${styleMap(handStyles)}></div>
          <span class="center"></span>
        </div>
      </div>
    `, this.shadow, {
      eventContext: this,
    });
    // clang-format on
  }

  private renderDials() {
    if (!this.dialTemplates) {
      // Disabled until https://crbug.com/1079231 is fixed.
      // clang-format off
      this.dialTemplates = [0, 45, 90, 135, 180, 225, 270, 315].map(deg => {
        const radius = this.clockRadius - ClockDialLength - 3 /* clock border */;
        const {translateX, translateY} = get2DTranslationsForAngle({
          value: deg,
          unit: AngleUnit.Deg,
        }, radius);
        const dialStyles = {
          transform: `translate(${translateX}px, ${translateY}px) rotate(${deg}deg)`,
        };
        return html`<span class="dial" style=${styleMap(dialStyles)}></span>`;
      });
      // clang-format on
    }

    return this.dialTemplates;
  }
}

if (!customElements.get('devtools-css-angle-editor')) {
  customElements.define('devtools-css-angle-editor', CSSAngleEditor);
}

declare global {
  interface HTMLElementTagNameMap {
    'devtools-css-angle-editor': CSSAngleEditor;
  }
}
