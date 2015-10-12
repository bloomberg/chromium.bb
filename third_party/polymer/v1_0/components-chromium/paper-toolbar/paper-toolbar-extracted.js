(function() {

    'use strict';

    function classNames(obj) {
      var classNames = [];
      for (var key in obj) {
        if (obj.hasOwnProperty(key) && obj[key]) {
          classNames.push(key);
        }
      }

      return classNames.join(' ');
    }

    Polymer({

      is: 'paper-toolbar',

      hostAttributes: {
        'role': 'toolbar'
      },

      properties: {

        /**
         * Controls how the items are aligned horizontally when they are placed
         * at the bottom.
         * Options are `start`, `center`, `end`, `justified` and `around`.
         *
         * @attribute bottomJustify
         * @type string
         * @default ''
         */
        bottomJustify: {
          type: String,
          value: ''
        },

        /**
         * Controls how the items are aligned horizontally.
         * Options are `start`, `center`, `end`, `justified` and `around`.
         *
         * @attribute justify
         * @type string
         * @default ''
         */
        justify: {
          type: String,
          value: ''
        },

        /**
         * Controls how the items are aligned horizontally when they are placed
         * in the middle.
         * Options are `start`, `center`, `end`, `justified` and `around`.
         *
         * @attribute middleJustify
         * @type string
         * @default ''
         */
        middleJustify: {
          type: String,
          value: ''
        }

      },

      attached: function() {
        this._observer = this._observe(this);
        this._updateAriaLabelledBy();
      },

      detached: function() {
        if (this._observer) {
          this._observer.disconnect();
        }
      },

      _observe: function(node) {
        var observer = new MutationObserver(function() {
          this._updateAriaLabelledBy();
        }.bind(this));
        observer.observe(node, {
          childList: true,
          subtree: true
        });
        return observer;
      },

      _updateAriaLabelledBy: function() {
        var labelledBy = [];
        var contents = Polymer.dom(this.root).querySelectorAll('content');
        for (var content, index = 0; content = contents[index]; index++) {
          var nodes = Polymer.dom(content).getDistributedNodes();
          for (var node, jndex = 0; node = nodes[jndex]; jndex++) {
            if (node.classList && node.classList.contains('title')) {
              if (node.id) {
                labelledBy.push(node.id);
              } else {
                var id = 'paper-toolbar-label-' + Math.floor(Math.random() * 10000);
                node.id = id;
                labelledBy.push(id);
              }
            }
          }
        }
        if (labelledBy.length > 0) {
          this.setAttribute('aria-labelledby', labelledBy.join(' '));
        }
      },

      _computeBarClassName: function(barJustify) {
        var classObj = {
          'center': true,
          'horizontal': true,
          'layout': true,
          'toolbar-tools': true
        };

        // If a blank string or any falsy value is given, no other class name is
        // added.
        if (barJustify) {
          var justifyClassName = (barJustify === 'justified') ?
              barJustify :
              barJustify + '-justified';

          classObj[justifyClassName] = true;
        }

        return classNames(classObj);
      }

    });

  }());