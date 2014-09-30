function createShadowRoot()
{
    var children = Array.prototype.slice.call(arguments);
    if ((children[0] instanceof Object) && !(children[0] instanceof Node))
        return {'isShadowRoot': true,
                'attributes': children[0],
                'children': children.slice(1)};
    return {'isShadowRoot': true,
            'children': children};
}

function createUserAgentShadowRoot()
{
    var shadowRoot = createShadowRoot.apply(null, arguments);
    shadowRoot.isUserAgentShadowRoot = true;
    return shadowRoot;
}

// This function can take optional child elements, which might be a result of createShadowRoot(), as arguments[2:].
function createDOM(tagName, attributes)
{
    var element = document.createElement(tagName);
    for (var name in attributes)
        element.setAttribute(name, attributes[name]);
    var childElements = Array.prototype.slice.call(arguments, 2);
    for (var i = 0; i < childElements.length; ++i) {
        var child = childElements[i];
        if (child.isShadowRoot) {
            var shadowRoot;
            if (child.isUserAgentShadowRoot) {
                shadowRoot = window.internals.createUserAgentShadowRoot(element);
            } else {
                shadowRoot = element.createShadowRoot();
            }
            if (child.attributes) {
                for (var attribute in child.attributes) {
                    // Shadow Root does not have setAttribute.
                    shadowRoot[attribute] = child.attributes[attribute];
                }
            }
            for (var j = 0; j < child.children.length; ++j)
                shadowRoot.appendChild(child.children[j]);
        } else
            element.appendChild(child);
    }
    return element;
}

function isShadowHost(node)
{
    return window.internals.oldestShadowRoot(node);
}

function isShadowRoot(node)
{
    return node instanceof window.ShadowRoot;
}

function isIframeElement(element)
{
    return element && element.nodeName == 'IFRAME';
}

// You can spefify youngerShadowRoot by consecutive slashes.
// See LayoutTests/fast/dom/shadow/get-element-by-id-in-shadow-root.html for actual usages.
function getNodeInTreeOfTrees(path)
{
    var ids = path.split('/');
    var node = document.getElementById(ids[0]);
    for (var i = 1; node != null && i < ids.length; ++i) {
        if (isIframeElement(node)) {
            node = node.contentDocument.getElementById(ids[i]);
            continue;
        }
        if (isShadowRoot(node))
            node = internals.youngerShadowRoot(node);
        else if (internals.oldestShadowRoot(node))
            node = internals.oldestShadowRoot(node);
        else
            return null;
        if (ids[i] != '')
            node = node.getElementById(ids[i]);
    }
    return node;
}

function dumpNode(node)
{
    if (!node)
      return 'null';
    if (node.id)
        return '#' + node.id;
    return '' + node;
}

function dumpNodeList(nodeList) {
    var result = "";
    var length = nodeList.length;
    for (var i = 0; i < length; i++)
        result += dumpNode(nodeList[i]) + ", ";
    result += "length: " + length;
    return result;
}

function shouldBeEqualAsArray(nodeList, expectedNodes)
{
    // FIXME: Avoid polluting the global namespace.
    window.nodeList = nodeList;
    window.expectedNodes = expectedNodes;
    shouldBe("nodeList.length", "expectedNodes.length");
    for (var i = 0; i < nodeList.length; ++i) {
        shouldBe("nodeList.item(" + i + ")", "expectedNodes[" + i + "]");
    }
}

function innermostActiveElement(element)
{
    element = element || document.activeElement;
    if (isIframeElement(element)) {
        if (element.contentDocument.activeElement)
            return innermostActiveElement(element.contentDocument.activeElement);
        return element;
    }
    if (isShadowHost(element)) {
        var shadowRoot = window.internals.oldestShadowRoot(element);
        while (shadowRoot) {
            if (shadowRoot.activeElement)
                return innermostActiveElement(shadowRoot.activeElement);
            shadowRoot = window.internals.youngerShadowRoot(shadowRoot);
        }
    }
    return element;
}

function isInnermostActiveElement(id)
{
    var element = getNodeInTreeOfTrees(id);
    if (!element) {
        debug('FAIL: There is no such element with id: '+ from);
        return false;
    }
    if (element == innermostActiveElement())
        return true;
    debug('Expected innermost activeElement is ' + id + ', but actual innermost activeElement is ' + dumpNode(innermostActiveElement()));
    return false;
}

function shouldNavigateFocus(from, to, direction)
{
    debug('Should move from ' + from + ' to ' + to + ' in ' + direction);
    var fromElement = getNodeInTreeOfTrees(from);
    if (!fromElement) {
      debug('FAIL: There is no such element with id: '+ from);
      return;
    }
    fromElement.focus();
    if (!isInnermostActiveElement(from)) {
        debug('FAIL: Can not be focused: '+ from);
        return;
    }
    if (direction == 'forward')
        navigateFocusForward();
    else
        navigateFocusBackward();
    if (isInnermostActiveElement(to))
        debug('PASS');
    else
        debug('FAIL');
}

function navigateFocusForward()
{
    eventSender.keyDown('\t');
}

function navigateFocusBackward()
{
    eventSender.keyDown('\t', ['shiftKey']);
}

function testFocusNavigationFowrad(elements)
{
    for (var i = 0; i + 1 < elements.length; ++i)
        shouldNavigateFocus(elements[i], elements[i + 1], 'forward');
}

function testFocusNavigationBackward(elements)
{
    for (var i = 0; i + 1 < elements.length; ++i)
        shouldNavigateFocus(elements[i], elements[i + 1], 'backward');
}

function dumpComposedShadowTree(node, indent)
{
    indent = indent || "";
    var output = indent + dumpNode(node) + "\n";
    var child;
    for (child = internals.firstChildByWalker(node); child; child = internals.nextSiblingByWalker(child))
         output += dumpComposedShadowTree(child, indent + "\t");
    return output;
}

function lastNodeByWalker(root)
{
    var lastNode = root;
    while (internals.lastChildByWalker(lastNode))
        lastNode = internals.lastChildByWalker(lastNode);
    return lastNode;
}

function showComposedShadowTreeByTraversingInForward(root)
{
    var node = root;
    var last = lastNodeByWalker(root);
    while (node) {
        debug(dumpNode(node));
        if (node == last)
            break;
        node = internals.nextNodeByWalker(node);
    }
}

function showComposedShadowTreeByTraversingInBackward(root)
{
    var node = lastNodeByWalker(root);
    while (node) {
        debug(dumpNode(node));
        if (node == root)
            break;
        node = internals.previousNodeByWalker(node);
    }
}

function showComposedShadowTree(node)
{
    debug('Composed Shadow Tree:');
    debug(dumpComposedShadowTree(node));

    debug('Traverse in forward.');
    showComposedShadowTreeByTraversingInForward(node);

    debug('Traverse in backward.');
    showComposedShadowTreeByTraversingInBackward(node);

    debug('');
}

function showNextNode(node) {
    var next = internals.nextNodeByWalker(node);
    debug('Next node of [' + dumpNode(node) + '] is [' + dumpNode(next) + ']');
}
