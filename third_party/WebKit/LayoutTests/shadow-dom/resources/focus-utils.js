'use strict';

function innermostActiveElement(element)
{
    element = element || document.activeElement;
    if (isIFrameElement(element)) {
        if (element.contentDocument.activeElement)
            return innermostActiveElement(element.contentDocument.activeElement);
        return element;
    }
    if (isShadowHost(element)) {
        let shadowRoot = window.internals.oldestShadowRoot(element);
        while (shadowRoot) {
            if (shadowRoot.activeElement)
                return innermostActiveElement(shadowRoot.activeElement);
            shadowRoot = window.internals.youngerShadowRoot(shadowRoot);
        }
    }
    return element;
}

function isInnermostActiveElement(path)
{
    const element = getNodeInComposedTree(path);
    if (!element)
        return false;
    return element === innermostActiveElement();
}

function shouldNavigateFocus(from, to, direction)
{
    const fromElement = getNodeInComposedTree(from);
    if (!fromElement)
        return false;

    fromElement.focus();
    if (!isInnermostActiveElement(from))
        return false;

    if (direction == 'forward')
        navigateFocusForward();
    else
        navigateFocusBackward();

    return isInnermostActiveElement(to);
}

function navigateFocusForward()
{
    if (window.eventSender)
        eventSender.keyDown('\t');
}

function navigateFocusBackward()
{
    if (window.eventSender)
        eventSender.keyDown('\t', ['shiftKey']);
}

function assert_focus_navigation_forward(elements)
{
    for (var i = 0; i + 1 < elements.length; ++i)
        assert_true(shouldNavigateFocus(elements[i], elements[i + 1], 'forward'),
                    'Focus should move from ' + elements[i] + ' to ' + elements[i + 1]);
}

function assert_focus_navigation_backward(elements)
{
    for (var i = 0; i + 1 < elements.length; ++i)
        assert_true(shouldNavigateFocus(elements[i], elements[i + 1], 'backward'),
                    'Focus should move from ' + elements[i] + ' to ' + elements[i + 1]);
}
