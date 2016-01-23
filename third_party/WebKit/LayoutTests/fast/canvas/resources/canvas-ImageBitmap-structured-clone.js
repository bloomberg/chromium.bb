self.onmessage = function(e) {
  postMessage({label:e.data.label, data:e.data.data});
};
