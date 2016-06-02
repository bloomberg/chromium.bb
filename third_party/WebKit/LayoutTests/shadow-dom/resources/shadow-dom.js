function removeWhiteSpaceOnlyTextNodes(node)
{
    for (var i = 0; i < node.childNodes.length; i++) {
        var child = node.childNodes[i];
        if (child.nodeType === Node.TEXT_NODE && child.nodeValue.trim().length == 0) {
            node.removeChild(child);
            i--;
        } else if (child.nodeType === Node.ELEMENT_NODE || child.nodeType === Node.DOCUMENT_FRAGMENT_NODE) {
            removeWhiteSpaceOnlyTextNodes(child);
        }
    }
    if (node.shadowRoot) {
        removeWhiteSpaceOnlyTextNodes(node.shadowRoot);
    }
}

function convertTemplatesToShadowRootsWithin(node) {
    var nodes = node.querySelectorAll("template");
    for (var i = 0; i < nodes.length; ++i) {
        var template = nodes[i];
        var mode = template.getAttribute("data-mode");
        var parent = template.parentNode;
        parent.removeChild(template);
        var shadowRoot;
        if (!mode || mode == 'v0'){
            shadowRoot = parent.createShadowRoot();
        } else {
            shadowRoot = parent.attachShadow({'mode': mode});
        }
        var expose = template.getAttribute("data-expose-as");
        if (expose)
            window[expose] = shadowRoot;
        if (template.id)
            shadowRoot.id = template.id;
        var fragments = document.importNode(template.content, true);
        shadowRoot.appendChild(fragments);

        convertTemplatesToShadowRootsWithin(shadowRoot);
    }
}

function isShadowHost(node)
{
    return node && node.nodeType == Node.ELEMENT_NODE && node.shadowRoot;
}

function isIFrameElement(element)
{
    return element && element.nodeName == 'IFRAME';
}

// Returns node from shadow/iframe tree "path".
function getNodeInComposedTree(path)
{
    var ids = path.split('/');
    var node = document.getElementById(ids[0]);
    for (var i = 1; node != null && i < ids.length; ++i) {
        if (isIFrameElement(node))
            node = node.contentDocument.getElementById(ids[i]);
        else if (isShadowHost(node))
            node = node.shadowRoot.getElementById(ids[i]);
        else
            return null;
    }
    return node;
}

function createTestTree(node) {

  let labels = {};

  function attachShadowFromTemplate(template) {
    let parent = template.parentNode;
    parent.removeChild(template);
    let shadowRoot = parent.attachShadow({mode: template.getAttribute('data-mode')});
    let label = template.getAttribute('label');
    if (label) {
      shadowRoot.id = label;
      labels[label] = shadowRoot;
    }
    shadowRoot.appendChild(document.importNode(template.content, true));
    return shadowRoot;
  }

  function walk(root) {
    if (root.getAttribute && root.getAttribute('label')) {
      labels[root.getAttribute('label')] = root;
    }
    for (let e of Array.from(root.querySelectorAll('[label]'))) {
      labels[e.getAttribute('label')] = e;
    }
    for (let e of Array.from(root.querySelectorAll('template'))) {
      walk(attachShadowFromTemplate(e));
    }
  }

  walk(node.cloneNode(true));
  return labels;
}

function dispatchEventWithLog(nodes, target, event) {

  function labelFor(e) {
    if (e.getAttribute && e.getAttribute('label')) {
      return e.getAttribute('label');
    }
    return e.id || e.tagName;
  }

  let log = [];
  let attachedNodes = [];
  for (let label in nodes) {
    let startingNode = nodes[label];
    for (let node = startingNode; node; node = node.parentNode) {
      if (attachedNodes.indexOf(node) >= 0)
        continue;
      let label = labelFor(node);
      if (!label)
        continue;
      attachedNodes.push(node);
      node.addEventListener(event.type, (e) => {
        log.push([label,
                  event.relatedTarget ? labelFor(event.relatedTarget) : null,
                  event.composedPath().map((n) => {
                    return labelFor(n);
                  })]);
      });
    }
  }
  target.dispatchEvent(event);
  return log;
}

// This function assumes that testharness.js is available.
function assert_event_path_equals(actual, expected) {
  assert_equals(actual.length, expected.length);
  for (let i = 0; i < actual.length; ++i) {
    assert_equals(actual[i][0], expected[i][0], 'currentTarget at ' + i + ' should be same');
    assert_equals(actual[i][1], expected[i][1], 'relatedTarget at ' + i + ' should be same');
    assert_array_equals(actual[i][2], expected[i][2], 'composedPath at ' + i + ' should be same');
  }
}
