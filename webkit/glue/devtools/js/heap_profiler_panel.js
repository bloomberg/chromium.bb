WebInspector.HeapProfilerPanel = function() {
  WebInspector.Panel.call(this);

  this.element.addStyleClass('heap-profiler');

  this.recordButton = document.createElement('button');
  this.recordButton.title = WebInspector.UIString('Take heap snapshot.');
  // TODO(mnaganov): Use an icon.
  this.recordButton.textContent = "SS";
  this.recordButton.id = 'take-heap-snapshot-status-bar-item';
  this.recordButton.className = 'status-bar-item';
  this.recordButton.addEventListener('click',
      this._takeSnapshotClicked.bind(this),
      false);
};

WebInspector.HeapProfilerPanel.prototype = {
  toolbarItemClass: 'heap-profiler',

  get toolbarItemLabel() {
    return WebInspector.UIString('Heap');
  },

  get statusBarItems() {
    return [this.recordButton];
  },

  show: function() {
    WebInspector.Panel.prototype.show.call(this);
  },

  reset: function() {
  },

  handleKeyEvent: function(event) {
  },

  addLogLine: function(logLine) {
    var line = document.createElement('div');
    line.innerHTML = logLine;
    this.element.appendChild(line);
  },

  _takeSnapshotClicked: function() {
    devtools.tools.getDebuggerAgent().startProfiling(
        devtools.DebuggerAgent.ProfilerModules.PROFILER_MODULE_HEAP_SNAPSHOT |
        devtools.DebuggerAgent.ProfilerModules.PROFILER_MODULE_HEAP_STATS |
        devtools.DebuggerAgent.ProfilerModules.PROFILER_MODULE_JS_CONSTRUCTORS);
  }
};

WebInspector.HeapProfilerPanel.prototype.__proto__ =
    WebInspector.Panel.prototype;
