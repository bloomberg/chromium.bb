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
