(function() {

function click(button) {
  var rect = button.getBoundingClientRect();
  eventSender.mouseMoveTo(rect.left, rect.top);
  eventSender.mouseDown();
  eventSender.mouseUp();
}

var observer = new MutationObserver(mutations => {
  for (var mutation of mutations) {
    for (var node of mutation.addedNodes) {
      if (node.localName == 'button')
        click(node);
      else if (node.localName == 'iframe')
        observe(node.contentDocument);
    }
  }
});

function observe(target) {
  observer.observe(target, { childList: true, subtree: true });
}

// Handle what's already in the document.
for (var button of document.getElementsByTagName('button')) {
  click(button);
}
for (var iframe of document.getElementsByTagName('iframe')) {
  observe(iframe.contentDocument);
}

// Observe future changes.
observe(document);

})();
