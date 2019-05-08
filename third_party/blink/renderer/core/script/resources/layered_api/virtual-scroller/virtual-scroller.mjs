import {Repeats} from './virtual-repeater.mjs';

export class RangeChangeEvent extends Event {
  constructor(type, init) {
    super(type, init);
    this._first = Math.floor(init.first || 0);
    this._last = Math.floor(init.last || 0);
  }
  get first() {
    return this._first;
  }
  get last() {
    return this._last;
  }
}

export const RepeatsAndScrolls = Superclass => class extends Repeats
(Superclass) {
  constructor(config) {
    super();
    this._num = 0;
    this._first = -1;
    this._last = -1;
    this._prevFirst = -1;
    this._prevLast = -1;

    this._needsUpdateView = false;
    this._containerElement = null;
    this._layout = null;
    this._scrollTarget = null;
    // Keep track of original inline style of the container,
    // so it can be restored when container is changed.
    this._containerInlineStyle = null;
    // A sentinel element that sizes the container when
    // it is a scrolling element.
    this._sizer = null;
    // Layout provides these values, we set them on _render().
    this._scrollSize = null;
    this._scrollErr = null;
    this._childrenPos = null;

    this._containerSize = null;
    this._containerRO = new ResizeObserver(
        (entries) => this._containerSizeChanged(entries[0].contentRect));

    this._skipNextChildrenSizeChanged = false;
    this._childrenRO =
        new ResizeObserver((entries) => this._childrenSizeChanged(entries));

    if (config) {
      Object.assign(this, config);
    }
  }

  get container() {
    return this._container;
  }
  set container(container) {
    super.container = container;

    const oldEl = this._containerElement;
    // Consider document fragments as shadowRoots.
    const newEl =
        (container && container.nodeType === Node.DOCUMENT_FRAGMENT_NODE) ?
        container.host :
        container;
    if (oldEl === newEl) {
      return;
    }

    this._containerRO.disconnect();
    this._containerSize = null;

    if (oldEl) {
      if (this._containerInlineStyle) {
        oldEl.setAttribute('style', this._containerInlineStyle);
      } else {
        oldEl.removeAttribute('style');
      }
      this._containerInlineStyle = null;
      if (oldEl === this._scrollTarget) {
        oldEl.removeEventListener('scroll', this, {passive: true});
        this._sizer && this._sizer.remove();
      }
    } else {
      // First time container was setup, add listeners only now.
      addEventListener('scroll', this, {passive: true});
    }

    this._containerElement = newEl;

    if (newEl) {
      this._containerInlineStyle = newEl.getAttribute('style') || null;
      if (newEl === this._scrollTarget) {
        this._sizer = this._sizer || this._createContainerSizer();
        this._container.prepend(this._sizer);
      }
      this._scheduleUpdateView();
      this._containerRO.observe(newEl);
    }
  }

  get layout() {
    return this._layout;
  }
  set layout(layout) {
    if (layout === this._layout) {
      return;
    }

    if (this._layout) {
      this._measureCallback = null;
      this._layout.removeEventListener('scrollsizechange', this);
      this._layout.removeEventListener('scrollerrorchange', this);
      this._layout.removeEventListener('itempositionchange', this);
      this._layout.removeEventListener('rangechange', this);
      // Reset container size so layout can get correct viewport size.
      if (this._containerElement) {
        this._sizeContainer();
      }
    }

    this._layout = layout;

    if (this._layout) {
      if (typeof this._layout.updateItemSizes === 'function') {
        this._measureCallback = this._layout.updateItemSizes.bind(this._layout);
        this.requestRemeasure();
      }
      this._layout.addEventListener('scrollsizechange', this);
      this._layout.addEventListener('scrollerrorchange', this);
      this._layout.addEventListener('itempositionchange', this);
      this._layout.addEventListener('rangechange', this);
      this._scheduleUpdateView();
    }
  }

  /**
   * The element that generates scroll events and defines the container
   * viewport. The value `null` (default) corresponds to `window` as scroll
   * target.
   * @type {Element|null}
   */
  get scrollTarget() {
    return this._scrollTarget;
  }
  /**
   * @param {Element|null} target
   */
  set scrollTarget(target) {
    // Consider window as null.
    if (target === window) {
      target = null;
    }
    if (this._scrollTarget === target) {
      return;
    }
    if (this._scrollTarget) {
      this._scrollTarget.removeEventListener('scroll', this, {passive: true});
      if (this._sizer && this._scrollTarget === this._containerElement) {
        this._sizer.remove();
      }
    }

    this._scrollTarget = target;

    if (target) {
      target.addEventListener('scroll', this, {passive: true});
      if (target === this._containerElement) {
        this._sizer = this._sizer || this._createContainerSizer();
        this._container.prepend(this._sizer);
      }
    }
  }

  /**
   * @protected
   */
  _render() {
    // console.time(`render ${this._containerElement.localName}#${
    //     this._containerElement.id}`);

    this._childrenRO.disconnect();

    // Update layout properties before rendering to have correct
    // first, num, scroll size, children positions.
    this._layout.totalItems = this.totalItems;
    if (this._needsUpdateView) {
      this._needsUpdateView = false;
      this._updateView();
    }
    this._layout.reflowIfNeeded();
    // Keep rendering until there is no more scheduled renders.
    while (true) {
      if (this._pendingRender) {
        cancelAnimationFrame(this._pendingRender);
        this._pendingRender = null;
      }
      // Update scroll size and correct scroll error before rendering.
      this._sizeContainer(this._scrollSize);
      if (this._scrollErr) {
        // This triggers a 'scroll' event (async) which triggers another
        // _updateView().
        this._correctScrollError(this._scrollErr);
        this._scrollErr = null;
      }
      // Position children (_didRender()), and provide their measures to layout.
      super._render();
      this._layout.reflowIfNeeded();
      // If layout reflow did not provoke another render, we're done.
      if (!this._pendingRender) {
        break;
      }
    }
    // We want to skip the first ResizeObserver callback call as we already
    // measured the children.
    this._skipNextChildrenSizeChanged = true;
    this._kids.forEach(child => this._childrenRO.observe(child));

    // console.timeEnd(`render ${this._containerElement.localName}#${
    //     this._containerElement.id}`);
  }

  /**
   * Position children before they get measured.
   * Measuring will force relayout, so by positioning
   * them first, we reduce computations.
   * @protected
   */
  _didRender() {
    if (this._childrenPos) {
      this._positionChildren(this._childrenPos);
      this._childrenPos = null;
    }
  }

  /**
   * @param {!Event} event
   * @private
   */
  handleEvent(event) {
    switch (event.type) {
      case 'scroll':
        if (!this._scrollTarget || event.target === this._scrollTarget) {
          this._scheduleUpdateView();
        }
        break;
      case 'scrollsizechange':
        this._scrollSize = event.detail;
        this._scheduleRender();
        break;
      case 'scrollerrorchange':
        this._scrollErr = event.detail;
        this._scheduleRender();
        break;
      case 'itempositionchange':
        this._childrenPos = event.detail;
        this._scheduleRender();
        break;
      case 'rangechange':
        this._adjustRange(event.detail);
        break;
      default:
        console.warn('event not handled', event);
    }
  }
  /**
   * @return {!Element}
   * @private
   */
  _createContainerSizer() {
    const sizer = document.createElement('div');
    // When the scrollHeight is large, the height
    // of this element might be ignored.
    // Setting content and font-size ensures the element
    // has a size.
    Object.assign(sizer.style, {
      position: 'absolute',
      margin: '-2px 0 0 0',
      padding: 0,
      visibility: 'hidden',
      fontSize: '2px',
    });
    sizer.innerHTML = '&nbsp;';
    return sizer;
  }

  // Rename _ordered to _kids?
  /**
   * @protected
   */
  get _kids() {
    return this._ordered;
  }
  /**
   * @private
   */
  _scheduleUpdateView() {
    this._needsUpdateView = true;
    this._scheduleRender();
  }
  /**
   * @private
   */
  _updateView() {
    let width, height, top, left;
    if (this._scrollTarget === this._containerElement) {
      width = this._containerSize.width;
      height = this._containerSize.height;
      left = this._containerElement.scrollLeft;
      top = this._containerElement.scrollTop;
    } else {
      const containerBounds = this._containerElement.getBoundingClientRect();
      const scrollBounds = this._scrollTarget ?
          this._scrollTarget.getBoundingClientRect() :
          {top: 0, left: 0, width: innerWidth, height: innerHeight};
      const scrollerWidth = scrollBounds.width;
      const scrollerHeight = scrollBounds.height;
      const xMin = Math.max(
          0, Math.min(scrollerWidth, containerBounds.left - scrollBounds.left));
      const yMin = Math.max(
          0, Math.min(scrollerHeight, containerBounds.top - scrollBounds.top));
      const xMax = this._layout.direction === 'vertical' ?
          Math.max(
              0,
              Math.min(
                  scrollerWidth, containerBounds.right - scrollBounds.left)) :
          scrollerWidth;
      const yMax = this._layout.direction === 'vertical' ?
          scrollerHeight :
          Math.max(
              0,
              Math.min(
                  scrollerHeight, containerBounds.bottom - scrollBounds.top));
      width = xMax - xMin;
      height = yMax - yMin;
      left = Math.max(0, -(containerBounds.x - scrollBounds.left));
      top = Math.max(0, -(containerBounds.y - scrollBounds.top));
    }
    this._layout.viewportSize = {width, height};
    this._layout.viewportScroll = {top, left};
  }
  /**
   * @private
   */
  _sizeContainer(size) {
    if (this._scrollTarget === this._containerElement) {
      const left = size && size.width ? size.width - 1 : 0;
      const top = size && size.height ? size.height - 1 : 0;
      this._sizer.style.transform = `translate(${left}px, ${top}px)`;
    } else {
      const style = this._containerElement.style;
      style.minWidth = size && size.width ? size.width + 'px' : null;
      style.minHeight = size && size.height ? size.height + 'px' : null;
    }
  }
  /**
   * @private
   */
  _positionChildren(pos) {
    const kids = this._kids;
    Object.keys(pos).forEach(key => {
      const idx = key - this._first;
      const child = kids[idx];
      if (child) {
        const {top, left} = pos[key];
        // console.debug(`_positionChild #${this._container.id} >
        // #${child.id}: top ${top}`);
        child.style.position = 'absolute';
        child.style.transform = `translate(${left}px, ${top}px)`;
      }
    });
  }
  /**
   * @private
   */
  _adjustRange(range) {
    this.num = range.num;
    this.first = range.first;
    this._incremental = !(range.stable);
    if (range.remeasure) {
      this.requestRemeasure();
    } else if (range.stable) {
      this._notifyStable();
    }
  }
  /**
   * @protected
   */
  _shouldRender() {
    if (!super._shouldRender() || !this._layout) {
      return false;
    }
    // NOTE: we're about to render, but the ResizeObserver didn't execute yet.
    // Since we want to keep rAF timing, we compute _containerSize now.
    // Would be nice to have a way to flush ResizeObservers
    if (this._containerSize === null) {
      const {width, height} = this._containerElement.getBoundingClientRect();
      this._containerSize = {width, height};
    }
    return this._containerSize.width > 0 || this._containerSize.height > 0;
  }
  /**
   * @private
   */
  _correctScrollError(err) {
    if (this._scrollTarget) {
      this._scrollTarget.scrollTop -= err.top;
      this._scrollTarget.scrollLeft -= err.left;
    } else {
      window.scroll(window.scrollX - err.left, window.scrollY - err.top);
    }
  }
  /**
   * @protected
   */
  _notifyStable() {
    const {first, num} = this;
    const last = first + num - 1;
    this._container.dispatchEvent(
        new RangeChangeEvent('rangechange', {first, last}));
  }
  /**
   * @private
   */
  _containerSizeChanged(size) {
    const {width, height} = size;
    this._containerSize = {width, height};
    // console.debug('container changed size', this._containerSize);
    this._scheduleUpdateView();
  }
  /**
   * @private
   */
  _childrenSizeChanged() {
    if (this._skipNextChildrenSizeChanged) {
      this._skipNextChildrenSizeChanged = false;
    } else {
      this.requestRemeasure();
    }
  }
};

export const VirtualScroller = RepeatsAndScrolls(class {});
