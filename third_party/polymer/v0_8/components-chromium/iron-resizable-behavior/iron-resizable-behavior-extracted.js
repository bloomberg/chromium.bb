
  /**
  `IronResizableBehavior` is a behavior that can be used in Polymer elements to
  coordinate the flow of resize events between "resizers" (elements that control the
  size or hidden state of their children) and "resizables" (elements that need to be
  notified when they are resized or un-hidden by their parents in order to take
  action on their new measurements).
  Elements that perform measurement should add the `IronResizableBehavior` behavior to
  their element definition and listen for the `iron-resize` event on themselves.
  This event will be fired when they become showing after having been hidden,
  when they are resized explicitly by another resizable, or when the window has been
  resized.
  Note, the `iron-resize` event is non-bubbling.
  @group Polymer Behaviors
  @element iron-resizable-behavior
  @homepage github.io
  */
  Polymer.IronResizableBehavior = {
    properties: {
      _parentResizable: {
        type: Object,
        observer: '_parentResizableChanged'
      }
    },

    listeners: {
      'iron-request-resize-notifications': '_onIronRequestResizeNotifications'
    },

    created: function() {
      // We don't really need property effects on these, and also we want them
      // to be created before the `_parentResizable` observer fires:
      this._interestedResizables = [];
      this._boundNotifyResize = this.notifyResize.bind(this);
    },

    attached: function() {
      this.fire('iron-request-resize-notifications', null, {
        node: this,
        bubbles: true
      });

      if (!this._parentResizable) {
        window.addEventListener('resize', this._boundNotifyResize);
        this.notifyResize();
      }
    },

    detached: function() {
      if (this._parentResizable) {
        this._parentResizable.stopResizeNotificationsFor(this);
      } else {
        window.removeEventListener('resize', this._boundNotifyResize);
      }

      this._parentResizable = null;
    },

    /**
     * Can be called to manually notify a resizable and its descendant
     * resizables of a resize change.
     */
    notifyResize: function() {
      if (!this.isAttached) {
        return;
      }

      this._interestedResizables.forEach(function(resizable) {
        // TODO(cdata): Currently behaviors cannot define "abstract" methods..
        if (!this.resizerShouldNotify || this.resizerShouldNotify(resizable)) {
          resizable.notifyResize();
        }
      }, this);

      this.fire('iron-resize', null, {
        node: this,
        bubbles: false
      });
    },

    /**
     * Used to assign the closest resizable ancestor to this resizable
     * if the ancestor detects a request for notifications.
     */
    assignParentResizable: function(parentResizable) {
      this._parentResizable = parentResizable;
    },

    /**
     * Used to remove a resizable descendant from the list of descendants
     * that should be notified of a resize change.
     */
    stopResizeNotificationsFor: function(target) {
      var index = this._interestedResizables.indexOf(target);

      if (index > -1) {
        this._interestedResizables.splice(index, 1);
      }
    },

    // TODO(cdata): Currently behaviors cannot define "abstract" methods.
    // resizerShouldNotify: function(el) { return true; },

    _parentResizableChanged: function(parentResizable) {
      if (parentResizable) {
        window.removeEventListener('resize', this._boundNotifyResize);
      }
    },

    _onIronRequestResizeNotifications: function(event) {
      var target = event.path ? event.path[0] : event.target;

      if (target === this) {
        return;
      }

      if (this._interestedResizables.indexOf(target) === -1) {
        this._interestedResizables.push(target);
      }

      target.assignParentResizable(this);

      event.stopPropagation();
    }
  };
