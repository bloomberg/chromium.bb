/* Copyright 2018 The Chromium Authors. All rights reserved.
   Use of this source code is governed by a BSD-style license that can be
   found in the LICENSE file.
*/
'use strict';
tr.exportTo('cp', () => {
  /**
   * Like Polymer.Path.set(), but returns a modified clone of root instead of
   * modifying root. In order to compute a new value from the existing value at
   * path efficiently, instead of calling Path.get() and then Path.set(),
   * `value` may be a callback that takes the existing value and returns
   * a new value.
   *
   * @param {!Object|!Array} root
   * @param {string|!Array} path
   * @param {*|function} value
   * @return {!Object|!Array}
   */
  function setImmutable(root, path, value) {
    if (path === '') {
      path = [];
    } else if (typeof(path) === 'string') {
      path = path.split('.');
    }
    // Based on dot-prop-immutable:
    // https://github.com/debitoor/dot-prop-immutable/blob/master/index.js
    root = Array.isArray(root) ? [...root] : {...root};
    if (path.length === 0) {
      if (typeof value === 'function') {
        return value(root);
      }
      return value;
    }
    let node = root;
    const maxDepth = path.length - 1;
    for (let depth = 0; depth < maxDepth; ++depth) {
      const key = Array.isArray(node) ? parseInt(path[depth]) : path[depth];
      const obj = node[key];
      node[key] = Array.isArray(obj) ? [...obj] : {...obj};
      node = node[key];
    }
    const key = Array.isArray(node) ? parseInt(path[maxDepth]) : path[maxDepth];
    if (typeof value === 'function') {
      node[key] = value(node[key]);
    } else {
      node[key] = value;
    }
    return root;
  }

  function deepFreeze(o) {
    Object.freeze(o);
    for (const [name, value] of Object.entries(o)) {
      if (typeof(value) !== 'object') continue;
      if (Object.isFrozen(value)) continue;
      if (value.__proto__ !== Object.prototype &&
          value.__proto__ !== Array.prototype) {
        continue;
      }
      deepFreeze(value);
    }
  }

  function isElementChildOf(el, potentialParent) {
    if (!el) return false;
    if (el === potentialParent) return false;
    while (el) {
      if (el === potentialParent) return true;
      if (el.parentNode) {
        el = el.parentNode;
      } else if (el.host) {
        el = el.host;
      } else {
        return false;
      }
    }
    return false;
  }

  function getActiveElement() {
    let element = document.activeElement;
    while (element !== null && element.shadowRoot) {
      element = element.shadowRoot.activeElement;
    }
    return element;
  }

  function afterRender() {
    return new Promise(resolve => {
      Polymer.RenderStatus.afterNextRender({}, () => {
        resolve();
      });
    });
  }

  function timeout(ms) {
    return new Promise(resolve => setTimeout(resolve, ms));
  }

  function animationFrame() {
    return new Promise(resolve => requestAnimationFrame(resolve));
  }

  function idle() {
    new Promise(resolve => requestIdleCallback(resolve));
  }

  async function sha(s) {
    s = new TextEncoder('utf-8').encode(s);
    const hash = await crypto.subtle.digest('SHA-256', s);
    const view = new DataView(hash);
    let hex = '';
    for (let i = 0; i < view.byteLength; i += 4) {
      hex += ('00000000' + view.getUint32(i).toString(16)).slice(-8);
    }
    return hex;
  }

  const DOCUMENT_READY = (async() => {
    while (document.readyState !== 'complete') {
      await animationFrame();
    }
  })();

  /*
   * Returns the bounding rect of the given element.
   */
  async function measureElement(element) {
    await DOCUMENT_READY;
    return element.getBoundingClientRect();
  }

  // measureText() below takes a string and optional style options, renders the
  // text in this div, and returns the size of the text. This div helps
  // measureText() render its arguments invisibly.
  const MEASURE_TEXT_HOST = document.createElement('div');
  MEASURE_TEXT_HOST.style.position = 'fixed';
  MEASURE_TEXT_HOST.style.visibility = 'hidden';
  MEASURE_TEXT_HOST.style.zIndex = -1000;
  MEASURE_TEXT_HOST.readyPromise = (async() => {
    await DOCUMENT_READY;
    document.body.appendChild(MEASURE_TEXT_HOST);
  })();

  // Assuming the computed style of MEASURE_TEXT_HOST doesn't change, measuring
  // a string with the same options should always return the same size, so the
  // measurements can be memoized. Also, measuring text is asynchronous, so this
  // cache can store promises in case callers try to measure the same text twice
  // in the same frame.
  const MEASURE_TEXT_CACHE = new Map();
  const MAX_MEASURE_TEXT_CACHE_SIZE = 1000;

  /*
   * Returns the bounding rect of the given textContent after applying the given
   * opt_options to a <span> containing textContent.
   */
  async function measureText(textContent, opt_options) {
    await MEASURE_TEXT_HOST.readyPromise;

    let cacheKey = {textContent, ...opt_options};
    cacheKey = JSON.stringify(cacheKey, Object.keys(cacheKey).sort());
    if (MEASURE_TEXT_CACHE.has(cacheKey)) {
      return await MEASURE_TEXT_CACHE.get(cacheKey);
    }

    const span = document.createElement('span');
    span.style.whiteSpace = 'nowrap';
    span.style.display = 'inline-block';
    span.textContent = textContent;
    Object.assign(span.style, opt_options);
    MEASURE_TEXT_HOST.appendChild(span);

    const promise = cp.measureElement(span).then(({width, height}) => {
      return {width, height};
    });
    while (MEASURE_TEXT_CACHE.size > MAX_MEASURE_TEXT_CACHE_SIZE) {
      MEASURE_TEXT_CACHE.delete(MEASURE_TEXT_CACHE.keys().next().value);
    }
    MEASURE_TEXT_CACHE.set(cacheKey, promise);
    const rect = await promise;
    MEASURE_TEXT_CACHE.set(cacheKey, rect);
    MEASURE_TEXT_HOST.removeChild(span);
    return rect;
  }

  function measureTrace() {
    const events = [];
    const loadTimes = Object.entries(performance.timing.toJSON()).filter(p =>
      p[1] > 0);
    loadTimes.sort((a, b) => a[1] - b[1]);
    const start = loadTimes.shift()[1];
    for (const [name, timeStamp] of loadTimes) {
      events.push({
        name: 'load:' + name,
        start,
        end: timeStamp,
        duration: timeStamp - start,
      });
    }
    for (const measure of performance.getEntriesByType('measure')) {
      const name = measure.name.replace(/[ \.]/g, ':').replace(
          ':reducers:', ':').replace(':actions:', ':');
      events.push({
        name,
        start: measure.startTime,
        duration: measure.duration,
        end: measure.startTime + measure.duration,
      });
    }
    return events;
  }

  function measureHistograms() {
    const histograms = new tr.v.HistogramSet();
    const unit = tr.b.Unit.byName.timeDurationInMs_smallerIsBetter;
    for (const event of measureTrace()) {
      let hist = histograms.getHistogramNamed(event.name);
      if (!hist) {
        hist = histograms.createHistogram(event.name, unit, []);
      }
      hist.addSample(event.duration);
    }
    return histograms;
  }

  function measureTable() {
    const table = [];
    for (const hist of measureHistograms()) {
      table.push([hist.average, hist.name]);
    }
    table.sort((a, b) => (b[0] - a[0]));
    return table.map(p =>
      parseInt(p[0]).toString().padEnd(6) + p[1]).join('\n');
  }

  /*
   * Returns a Polymer properties descriptor object.
   *
   * Usage:
   * const FooState = {
   *   abc: options => options.abc || 0,
   *   def: {reflectToAttribute: true, value: options => options.def || [],},
   * };
   * FooElement.properties = buildProperties('state', FooState);
   * FooElement.buildState = options => buildState(FooState, options);
   */
  function buildProperties(statePropertyName, configs) {
    const statePathPropertyName = statePropertyName + 'Path';
    const properties = {
      [statePathPropertyName]: {type: String},
      [statePropertyName]: {
        readOnly: true,
        statePath(state) {
          const statePath = this[statePathPropertyName];
          if (statePath === undefined) return {};
          return Polymer.Path.get(state, statePath) || {};
        },
      },
    };
    for (const [name, config] of Object.entries(configs)) {
      if (name === statePathPropertyName || name === statePropertyName) {
        throw new Error('Invalid property name: ' + name);
      }
      properties[name] = {
        readOnly: true,
        computed: `identity_(${statePropertyName}.${name})`,
      };
      if (typeof(config) === 'object') {
        for (const [paramName, paramValue] of Object.entries(config)) {
          if (paramName === 'value') continue;
          properties[name][paramName] = paramValue;
        }
      }
    }
    return properties;
  }

  /*
   * Returns a new object with the same shape as `configs` but with values taken
   * from `options`.
   * See buildProperties for description of `configs`.
   */
  function buildState(configs, options) {
    const state = {};
    for (const [name, config] of Object.entries(configs)) {
      switch (typeof(config)) {
        case 'object':
          state[name] = config.value(options);
          break;
        case 'function':
          state[name] = config(options);
          break;
        default:
          throw new Error('Invalid property config: ' + config);
      }
    }
    return state;
  }

  function normalize(columns, cells) {
    const dict = {};
    for (let i = 0; i < columns.length; ++i) {
      dict[columns[i]] = cells[i];
    }
    return dict;
  }

  async function* asGenerator(promise) {
    yield await promise;
  }

  /**
   * BatchIterator reduces processing costs by batching results and errors
   * from an array of tasks. A task can either be a promise or an asynchronous
   * iterator. In other words, use this class when it is costly to iteratively
   * process the output of each task (e.g. when rendering to DOM).
   *
   *   const tasks = urls.map(fetch);
   *   for await (const {results, errors} of new BatchIterator(tasks)) {
   *     render(results);
   *     renderErrors(errors);
   *   }
   */
  class BatchIterator {
    constructor(tasks, getDelay = cp.timeout) {
      // `tasks` may include either simple Promises or async generators.
      this.results_ = [];
      this.errors_ = [];
      this.promises_ = new Set();

      for (const task of tasks) this.add(task);

      this.getDelay_ = ms => {
        const promise = getDelay(ms).then(() => {
          promise.resolved = true;
        });
        return promise;
      };
    }

    add(task) {
      if (task instanceof Promise) task = asGenerator(task);
      this.generate_(task);
    }

    // Adds a Promise to this.promises_ that resolves when the generator next
    // resolves, and deletes itself from this.promises_.
    // If the generator is not done, the result is pushed to this.results_ or
    // the error is pushed to this.errors_, and another Promise is added to
    // this.promises_.
    generate_(generator) {
      const wrapped = (async() => {
        try {
          const {value, done} = await generator.next();
          if (done) return;
          this.results_.push(value);
          this.generate_(generator);
        } catch (error) {
          this.errors_.push(error);
        } finally {
          this.promises_.delete(wrapped);
        }
      })();
      this.promises_.add(wrapped);
    }

    batch_() {
      const batch = {results: this.results_, errors: this.errors_};
      this.results_ = [];
      this.errors_ = [];
      return batch;
    }

    async* [Symbol.asyncIterator]() {
      if (!this.promises_.size) return;

      // Yield the first result immediately in order to allow the user to start
      // to understand it (c.f. First Contentful Paint), and also to measure how
      // long it takes the caller to render the data. Use that measurement as an
      // estimation of how long to wait before yielding the next batch of
      // results. This first batch may contain multiple results/errors if
      // multiple tasks resolve in the same tick, or if a generator yields
      // multiple results synchronously.
      await Promise.race(this.promises_);
      let start = performance.now();
      yield this.batch_();
      let processingMs = performance.now() - start;

      while (this.promises_.size ||
             this.results_.length || this.errors_.length) {
        // Wait for a result or error to become available.
        // This may not be necessary if a promise resolved while the caller was
        // processing previous results.
        if (!this.results_.length && !this.errors_.length) {
          await Promise.race(this.promises_);
        }

        // Wait for either the delay to resolve or all generators to be done.
        // This can't use Promise.all() because generators can add new promises.
        const delay = this.getDelay_(processingMs);
        while (!delay.resolved && this.promises_.size) {
          await Promise.race([delay, ...this.promises_]);
        }

        start = performance.now();
        yield this.batch_();
        processingMs = performance.now() - start;
      }
    }
  }

  const ZERO_WIDTH_SPACE = String.fromCharCode(0x200b);
  const NON_BREAKING_SPACE = String.fromCharCode(0xA0);

  function breakWords(str) {
    if (!str) return NON_BREAKING_SPACE;

    // Insert spaces before underscores.
    str = str.replace(/_/g, ZERO_WIDTH_SPACE + '_');

    // Insert spaces after colons and dots.
    str = str.replace(/\./g, '.' + ZERO_WIDTH_SPACE);
    str = str.replace(/:/g, ':' + ZERO_WIDTH_SPACE);

    // Insert spaces before camel-case words.
    str = str.split(/([a-z][A-Z])/g);
    str = str.map((s, i) => {
      if ((i % 2) === 0) return s;
      return s[0] + ZERO_WIDTH_SPACE + s[1];
    });
    str = str.join('');
    return str;
  }

  function plural(count, pluralSuffix = 's', singularSuffix = '') {
    if (count === 1) return singularSuffix;
    return pluralSuffix;
  }

  /**
   * Compute a given number of colors by evenly spreading them around the
   * sinebow hue circle, or, if a Range of brightnesses is given, the hue x
   * brightness cylinder.
   *
   * @param {Number} numColors
   * @param {!Range} opt_options.brightnessRange
   * @param {Number} opt_options.brightnessPct
   * @param {Number} opt_options.hueOffset
   * @return {!Array.<!tr.b.Color>}
   */
  function generateColors(numColors, opt_options) {
    const options = opt_options || {};
    const brightnessRange = options.brightnessRange;
    const hueOffset = options.hueOffset || 0;
    const colors = [];
    if (numColors > 15 && brightnessRange) {
      // Evenly spread numColors around the surface of the hue x brightness
      // cylinder. Maximize distance between (huePct, brightnessPct) vectors.
      const numCycles = Math.round(numColors / 15);
      for (let i = 0; i < numCycles; ++i) {
        colors.push.apply(colors, generateColors(15, {
          brightnessPct: brightnessRange.lerp(i / (numCycles - 1)),
        }));
      }
    } else {
      // Evenly spread numColors throughout the sinebow hue circle.
      const brightnessPct = (options.brightnessPct === undefined) ? 0.5 :
        options.brightnessPct;
      for (let i = 0; i < numColors; ++i) {
        const huePct = hueOffset + (i / numColors);
        const [r, g, b] = tr.b.SinebowColorGenerator.sinebow(huePct);
        const rgba = tr.b.SinebowColorGenerator.calculateColor(
            r, g, b, 1, brightnessPct * 2);
        colors.push(tr.b.Color.fromString(rgba));
      }
    }
    return colors;
  }

  return {
    BatchIterator,
    NON_BREAKING_SPACE,
    ZERO_WIDTH_SPACE,
    afterRender,
    animationFrame,
    breakWords,
    buildProperties,
    buildState,
    deepFreeze,
    generateColors,
    getActiveElement,
    idle,
    isElementChildOf,
    measureElement,
    measureHistograms,
    measureTable,
    measureText,
    measureTrace,
    normalize,
    plural,
    setImmutable,
    sha,
    timeout,
  };
});
