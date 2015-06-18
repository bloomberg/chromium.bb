

  Polymer({

    is: 'iron-image',

    properties: {
      /**
       * The URL of an image.
       */
      src: {
        observer: '_srcChanged',
        type: String,
        value: ''
      },

      /**
       * When true, the image is prevented from loading and any placeholder is
       * shown.  This may be useful when a binding to the src property is known to
       * be invalid, to prevent 404 requests.
       */
      preventLoad: {
        type: Boolean,
        value: false
      },

      /**
       * Sets a sizing option for the image.  Valid values are `contain` (full
       * aspect ratio of the image is contained within the element and
       * letterboxed) or `cover` (image is cropped in order to fully cover the
       * bounds of the element), or `null` (default: image takes natural size).
       */
      sizing: {
        type: String,
        value: null
      },

      /**
       * When a sizing option is uzed (`cover` or `contain`), this determines
       * how the image is aligned within the element bounds.
       */
      position: {
        type: String,
        value: 'center'
      },

      /**
       * When `true`, any change to the `src` property will cause the `placeholder`
       * image to be shown until the
       */
      preload: {
        type: Boolean,
        value: false
      },

      /**
       * This image will be used as a background/placeholder until the src image has
       * loaded.  Use of a data-URI for placeholder is encouraged for instant rendering.
       */
      placeholder: {
        type: String,
        value: null
      },

      /**
       * When `preload` is true, setting `fade` to true will cause the image to
       * fade into place.
       */
      fade: {
        type: Boolean,
        value: false
      },

      /**
       * Read-only value that is true when the image is loaded.
       */
      loaded: {
        notify: true,
        type: Boolean,
        value: false
      },

      /**
       * Read-only value that tracks the loading state of the image when the `preload`
       * option is used.
       */
      loading: {
        notify: true,
        type: Boolean,
        value: false
      },

      /**
       * Can be used to set the width of image (e.g. via binding); size may also be
       * set via CSS.
       */
      width: {
        observer: '_widthChanged',
        type: Number,
        value: null
      },

      /**
       * Can be used to set the height of image (e.g. via binding); size may also be
       * set via CSS.
       *
       * @attribute height
       * @type number
       * @default null
       */
      height: {
        observer: '_heightChanged',
        type: Number,
        value: null
      },

      _placeholderBackgroundUrl: {
        type: String,
        computed: '_computePlaceholderBackgroundUrl(preload,placeholder)',
        observer: '_placeholderBackgroundUrlChanged'
      },

      requiresPreload: {
        type: Boolean,
        computed: '_computeRequiresPreload(preload,loaded)'
      },

      canLoad: {
        type: Boolean,
        computed: '_computeCanLoad(preventLoad, src)'
      }

    },

    observers: [
      '_transformChanged(sizing, position)',
      '_loadBehaviorChanged(canLoad, preload, loaded)',
      '_loadStateChanged(src, preload, loaded)',
    ],

    ready: function() {
      if (!this.hasAttribute('role')) {
        this.setAttribute('role', 'img');
      }
    },

    _computeImageVisibility: function() {
      return !!this.sizing;
    },

    _computePlaceholderVisibility: function() {
      return !this.preload || (this.loaded && !this.fade);
    },

    _computePlaceholderClassName: function() {
      if (!this.preload) {
        return '';
      }

      var className = 'fit';
      if (this.loaded && this.fade) {
        className += ' faded-out';
      }
      return className;
    },

    _computePlaceholderBackgroundUrl: function() {
      if (this.preload && this.placeholder) {
        return 'url(' + this.placeholder + ')';
      }

      return null;
    },

    _computeRequiresPreload: function() {
      return this.preload && !this.loaded;
    },

    _computeCanLoad: function() {
      return Boolean(!this.preventLoad && this.src);
    },

    _widthChanged: function() {
      this.style.width = isNaN(this.width) ? this.width : this.width + 'px';
    },

    _heightChanged: function() {
      this.style.height = isNaN(this.height) ? this.height : this.height + 'px';
    },

    _srcChanged: function(newSrc, oldSrc) {
      if (newSrc !== oldSrc) {
        this.loaded = false;
      }
    },

    _placeholderBackgroundUrlChanged: function() {
      this.$.placeholder.style.backgroundImage =
        this._placeholderBackgroundUrl;
    },

    _transformChanged: function() {
      var placeholderStyle = this.$.placeholder.style;

      this.style.backgroundSize =
        placeholderStyle.backgroundSize = this.sizing;

      this.style.backgroundPosition =
        placeholderStyle.backgroundPosition =
        this.sizing ? this.position : '';

      this.style.backgroundRepeat =
        placeholderStyle.backgroundRepeat =
        this.sizing ? 'no-repeat' : '';
    },

    _loadBehaviorChanged: function() {
      var img;

      if (!this.canLoad) {
        return;
      }

      if (this.requiresPreload) {
        img = new Image();
        img.src = this.src;

        this.loading = true;

        img.onload = function() {
          this.loading = false;
          this.loaded = true;
        }.bind(this);
      } else {
        this.loaded = true;
      }
    },

    _loadStateChanged: function() {
      if (this.requiresPreload) {
        return;
      }

      if (this.sizing) {
        this.style.backgroundImage = this.src ? 'url(' + this.src + ')': '';
      } else {
        this.$.img.src = this.src || '';
      }
    }
  });

