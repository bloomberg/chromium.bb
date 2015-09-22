

    Polymer({

      is: 'iron-icon',

      properties: {

        /**
         * The name of the icon to use. The name should be of the form:
         * `iconset_name:icon_name`.
         */
        icon: {
          type: String,
          observer: '_iconChanged'
        },

        /**
         * The name of the theme to used, if one is specified by the
         * iconset.
         */
        theme: {
          type: String,
          observer: '_updateIcon'
        },

        /**
         * If using iron-icon without an iconset, you can set the src to be
         * the URL of an individual icon image file. Note that this will take
         * precedence over a given icon attribute.
         */
        src: {
          type: String,
          observer: '_srcChanged'
        },
        
        _meta: {
          value: Polymer.Base.create('iron-meta', {type: 'iconset'})
        }
        
      },

      _DEFAULT_ICONSET: 'icons',

      _iconChanged: function(icon) {
        var parts = (icon || '').split(':');
        this._iconName = parts.pop();
        this._iconsetName = parts.pop() || this._DEFAULT_ICONSET;
        this._updateIcon();
      },

      _srcChanged: function(src) {
        this._updateIcon();
      },

      _usesIconset: function() {
        return this.icon || !this.src;
      },

      /** @suppress {visibility} */
      _updateIcon: function() {
        if (this._usesIconset()) {
          if (this._iconsetName) {
            this._iconset = this._meta.byKey(this._iconsetName);
            if (this._iconset) {
              this._iconset.applyIcon(this, this._iconName, this.theme);
            } else {
              this._warn(this._logf('_updateIcon', 'could not find iconset `'
                + this._iconsetName + '`, did you import the iconset?'));
            }
          }
        } else {
          if (!this._img) {
            this._img = document.createElement('img');
            this._img.style.width = '100%';
            this._img.style.height = '100%';
            this._img.draggable = false;
          }
          this._img.src = this.src;
          Polymer.dom(this.root).appendChild(this._img);
        }
      }

    });

  