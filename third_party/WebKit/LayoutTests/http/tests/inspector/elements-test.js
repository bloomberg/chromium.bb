var initialize_ElementTest = function() {

InspectorTest.preloadPanel("elements");

InspectorTest.inlineStyleSection = function()
{
    return UI.panels.elements._stylesWidget._sectionBlocks[0].sections[0];
}

InspectorTest.computedStyleWidget = function()
{
    return UI.panels.elements._computedStyleWidget;
}

InspectorTest.dumpComputedStyle = function(doNotAutoExpand)
{
    var computed = InspectorTest.computedStyleWidget();
    var treeOutline = computed._propertiesOutline;
    var children = treeOutline.rootElement().children();
    for (var treeElement of children) {
        var property = treeElement[Elements.ComputedStyleWidget._propertySymbol];
        if (property.name === "width" || property.name === "height")
            continue;
        var dumpText = "";
        dumpText += treeElement.title.querySelector(".property-name").textContent;
        dumpText += " ";
        dumpText += treeElement.title.querySelector(".property-value").textContent;
        InspectorTest.addResult(dumpText);
        if (doNotAutoExpand && !treeElement.expanded)
            continue;
        for (var trace of treeElement.children()) {
            var title = trace.title;
            var dumpText = "";
            if (trace.title.classList.contains("property-trace-inactive"))
                dumpText += "OVERLOADED ";
            dumpText += title.querySelector(".property-trace-value").textContent;
            dumpText += " - ";
            dumpText += title.querySelector(".property-trace-selector").textContent;
            var link = title.querySelector(".trace-link");
            if (link)
                dumpText += " " + extractLinkText(link);
            InspectorTest.addResult("    " + dumpText);
        }
    }
}

InspectorTest.findComputedPropertyWithName = function(name)
{
    var computed = InspectorTest.computedStyleWidget();
    var treeOutline = computed._propertiesOutline;
    var children = treeOutline.rootElement().children();
    for (var treeElement of children) {
        var property = treeElement[Elements.ComputedStyleWidget._propertySymbol];
        if (property.name === name)
            return treeElement;
    }
    return null;
}

InspectorTest.firstMatchedStyleSection = function()
{
    return UI.panels.elements._stylesWidget._sectionBlocks[0].sections[1];
}

InspectorTest.firstMediaTextElementInSection = function(section)
{
    return section.element.querySelector(".media-text");
}

InspectorTest.findNode = function(matchFunction, callback)
{
    callback = InspectorTest.safeWrap(callback);
    var result = null;
    var pendingRequests = 0;
    function processChildren(node)
    {
        try {
            if (result)
                return;

            var pseudoElementsMap = node.pseudoElements();
            var pseudoElements = pseudoElementsMap ? pseudoElementsMap.valuesArray() : [];
            var children = (node.children() || []).concat(node.shadowRoots()).concat(pseudoElements);
            if (node.templateContent())
                children.push(node.templateContent());
            else if (node.importedDocument())
                children.push(node.importedDocument());

            for (var i = 0; i < children.length; ++i) {
                var childNode = children[i];
                if (matchFunction(childNode)) {
                    result = childNode;
                    callback(result);
                    return;
                }
                pendingRequests++;
                childNode.getChildNodes(processChildren.bind(null, childNode));
            }
        } finally {
            pendingRequests--;
        }

        if (!result && !pendingRequests)
            callback(null);
    }

    InspectorTest.domModel.requestDocument(documentRequested.bind(this));
    function documentRequested(doc)
    {
        pendingRequests++;
        doc.getChildNodes(processChildren.bind(null, doc));
    }
};

InspectorTest.nodeWithId = function(idValue, callback)
{
    function nodeIdMatches(node)
    {
        return node.getAttribute("id") === idValue;
    }
    InspectorTest.findNode(nodeIdMatches, callback);
}

InspectorTest.querySelector = function(selector, callback)
{
    InspectorTest.domModel.requestDocument(documentRequested.bind(this));

    function documentRequested(doc)
    {
        InspectorTest.domModel.querySelector(doc.id, selector, nodeSelected);
    }

    function nodeSelected(nodeId)
    {
        callback(InspectorTest.domModel.nodeForId(nodeId));
    }
}

InspectorTest.shadowRootByHostId = function(idValue, callback)
{
    function shadowRootMatches(node)
    {
        return node.isShadowRoot() && node.parentNode.getAttribute("id") === idValue;
    }
    InspectorTest.findNode(shadowRootMatches, callback);
}

InspectorTest.nodeWithClass = function(classValue, callback)
{
    function nodeClassMatches(node)
    {
        var classAttr = node.getAttribute("class");
        return classAttr && classAttr.indexOf(classValue) > -1;
    }
    InspectorTest.findNode(nodeClassMatches, callback);
}

InspectorTest.expandedNodeWithId = function(idValue)
{
    var result;
    function callback(node)
    {
        result = node;
    }
    InspectorTest.nodeWithId(idValue, callback);
    return result;
}

InspectorTest.selectNode = function(node)
{
    return Common.Revealer.revealPromise(node);
}

InspectorTest.selectNodeWithId = function(idValue, callback)
{
    callback = InspectorTest.safeWrap(callback);
    function onNodeFound(node)
    {
        InspectorTest.selectNode(node).then(callback.bind(null, node));
    }
    InspectorTest.nodeWithId(idValue, onNodeFound);
}

function waitForStylesRebuild(matchFunction, callback, requireRebuild)
{
    (function sniff(node, rebuild)
    {
        if ((rebuild || !requireRebuild) && node && matchFunction(node)) {
            callback();
            return;
        }
        InspectorTest.addSniffer(Elements.StylesSidebarPane.prototype, "_nodeStylesUpdatedForTest", sniff);
    })(null);
}

InspectorTest.waitForStyles = function(idValue, callback, requireRebuild)
{
    callback = InspectorTest.safeWrap(callback);

    function nodeWithId(node)
    {
        return node.getAttribute("id") === idValue;
    }

    waitForStylesRebuild(nodeWithId, callback, requireRebuild);
}

InspectorTest.waitForStylesForClass = function(classValue, callback, requireRebuild)
{
    callback = InspectorTest.safeWrap(callback);

    function nodeWithClass(node)
    {
        var classAttr = node.getAttribute("class");
        return classAttr && classAttr.indexOf(classValue) > -1;
    }

    waitForStylesRebuild(nodeWithClass, callback, requireRebuild);
}

InspectorTest.waitForSelectorCommitted = function(callback)
{
    InspectorTest.addSniffer(Elements.StylePropertiesSection.prototype, "_editingSelectorCommittedForTest", callback);
}

InspectorTest.waitForMediaTextCommitted = function(callback)
{
    InspectorTest.addSniffer(Elements.StylePropertiesSection.prototype, "_editingMediaTextCommittedForTest", callback);
}

InspectorTest.waitForStyleApplied = function(callback)
{
    InspectorTest.addSniffer(Elements.StylePropertyTreeElement.prototype, "styleTextAppliedForTest", callback);
}

InspectorTest.selectNodeAndWaitForStyles = function(idValue, callback)
{
    callback = InspectorTest.safeWrap(callback);

    var targetNode;

    InspectorTest.waitForStyles(idValue, stylesUpdated, true);
    InspectorTest.selectNodeWithId(idValue, nodeSelected);

    function nodeSelected(node)
    {
        targetNode = node;
    }

    function stylesUpdated()
    {
        callback(targetNode);
    }
}

InspectorTest.selectPseudoElementAndWaitForStyles = function(parentId, pseudoType, callback)
{
    callback = InspectorTest.safeWrap(callback);

    var targetNode;

    waitForStylesRebuild(isPseudoElement, stylesUpdated, true);
    InspectorTest.findNode(isPseudoElement, nodeFound)

    function nodeFound(node)
    {
        targetNode = node;
        Common.Revealer.reveal(node);
    }

    function stylesUpdated()
    {
        callback(targetNode);
    }

    function isPseudoElement(node)
    {
        return node.parentNode && node.parentNode.getAttribute("id") === parentId && node.pseudoType() == pseudoType;
    }
}

InspectorTest.selectNodeAndWaitForStylesWithComputed = function(idValue, callback)
{
    callback = InspectorTest.safeWrap(callback);
    InspectorTest.selectNodeAndWaitForStyles(idValue, onSidebarRendered);

    function onSidebarRendered(node)
    {
        InspectorTest.computedStyleWidget().doUpdate().then(callback.bind(null, node));
    }
}

InspectorTest.firstElementsTreeOutline = function()
{
    return UI.panels.elements._treeOutlines[0];
}

InspectorTest.filterMatchedStyles = function(text)
{
    var regex = text ? new RegExp(text, "i") : null;
    InspectorTest.addResult("Filtering styles by: " + text);
    UI.panels.elements._stylesWidget.onFilterChanged(regex);
}

InspectorTest.dumpRenderedMatchedStyles = function()
{
    var sectionBlocks = UI.panels.elements._stylesWidget._sectionBlocks;
    for (var block of sectionBlocks) {
        for (var section of block.sections) {
            // Skip sections which were filtered out.
            if (section.element.classList.contains("hidden"))
                continue;
            dumpRenderedSection(section);
        }
    }

    function dumpRenderedSection(section)
    {
        InspectorTest.addResult(section._selectorElement.textContent + " {");
        var rootElement = section.propertiesTreeOutline.rootElement();
        for (var i = 0; i < rootElement.childCount(); ++i)
            dumpRenderedProperty(rootElement.childAt(i));
        InspectorTest.addResult("}");
    }

    function dumpRenderedProperty(property)
    {
        var text = new Array(4).join(" ");
        text += property.nameElement.textContent;
        text += ":";
        if (property.isExpandable())
            text += property.expanded ? "v" : ">";
        else
            text += " ";
        text += property.valueElement.textContent;
        if (property.listItemElement.classList.contains("filter-match"))
            text = "F" + text.substring(1);
        InspectorTest.addResult(text);
        if (!property.expanded)
            return;
        var indent = new Array(8).join(" ");
        for (var i = 0; i < property.childCount(); ++i) {
            var childProperty = property.childAt(i);
            var text = indent;
            text += String.sprintf("%s: %s", childProperty.nameElement.textContent, childProperty.valueElement.textContent);
            if (childProperty.listItemElement.classList.contains("filter-match"))
                text = "F" + text.substring(1);
            InspectorTest.addResult(text);
        }
    }
}

InspectorTest.dumpSelectedElementStyles = function(excludeComputed, excludeMatched, omitLonghands, includeSelectorGroupMarks)
{
    var sectionBlocks = UI.panels.elements._stylesWidget._sectionBlocks;
    if (!excludeComputed)
        InspectorTest.dumpComputedStyle();
    for (var block of sectionBlocks) {
        for (var section of block.sections) {
            if (section.style().parentRule && excludeMatched)
                continue;
            if (section.element.previousSibling && section.element.previousSibling.className === "sidebar-separator") {
                var nodeDescription = "";
                if (section.element.previousSibling.firstElementChild)
                    nodeDescription = section.element.previousSibling.firstElementChild.shadowRoot.lastChild.textContent;
                InspectorTest.addResult("======== " + section.element.previousSibling.textContent + nodeDescription + " ========");
            }
            printStyleSection(section, omitLonghands, includeSelectorGroupMarks);
        }
    }
}

function printStyleSection(section, omitLonghands, includeSelectorGroupMarks)
{
    if (!section)
        return;
    InspectorTest.addResult("[expanded] " + (section.propertiesTreeOutline.element.classList.contains("no-affect") ? "[no-affect] " : ""));

    var medias = section._titleElement.querySelectorAll(".media-list .media");
    for (var i = 0; i < medias.length; ++i) {
        var media = medias[i];
        InspectorTest.addResult(media.textContent);
    }
    var selector = section._titleElement.querySelector(".selector") || section._titleElement.querySelector(".keyframe-key");
    var selectorText = includeSelectorGroupMarks ? buildMarkedSelectors(selector) : selector.textContent;
    // Dump " {".
    selectorText += selector.nextSibling.textContent;
    var anchor = section._titleElement.querySelector(".styles-section-subtitle");
    if (anchor) {
        var anchorText = extractLinkText(anchor);
        selectorText += String.sprintf(" (%s)", anchorText);
    }
    InspectorTest.addResult(selectorText);

    InspectorTest.dumpStyleTreeOutline(section.propertiesTreeOutline, omitLonghands ? 1 : 2);
    InspectorTest.addResult("");
}

function extractLinkText(element)
{
    var anchor = element.querySelector(".devtools-link");
    if (!anchor)
        return element.textContent;
    var anchorText = anchor.textContent;
    var info = Components.Linkifier._linkInfo(anchor);
    var uiLocation = info && info.uiLocation;
    var anchorTarget = uiLocation ? (uiLocation.uiSourceCode.name() + ":" + (uiLocation.lineNumber + 1) + ":" + (uiLocation.columnNumber + 1)) : "";
    return anchorText + " -> " + anchorTarget;
}

function buildMarkedSelectors(element)
{
    var result = "";
    for (var node = element.firstChild; node; node = node.nextSibling) {
        if (node.nodeType === Node.ELEMENT_NODE && node.classList.contains("selector-matches"))
            result += "[$" + node.textContent + "$]";
        else
            result += node.textContent;
    }
    return result;
}

InspectorTest.toggleStyleProperty = function(propertyName, checked)
{
    var treeItem = InspectorTest.getElementStylePropertyTreeItem(propertyName);
    treeItem._toggleEnabled({ target: { checked: checked }, consume: function() { } });
}

InspectorTest.toggleMatchedStyleProperty = function(propertyName, checked)
{
    var treeItem = InspectorTest.getMatchedStylePropertyTreeItem(propertyName);
    treeItem._toggleEnabled({ target: { checked: checked }, consume: function() { } });
}

InspectorTest.eventListenersWidget = function()
{
    UI.viewManager.showView("elements.eventListeners");
    return self.runtime.sharedInstance(Elements.EventListenersWidget);
}

InspectorTest.showEventListenersWidget = function()
{
    return UI.viewManager.showView("elements.eventListeners");
}

InspectorTest.expandAndDumpSelectedElementEventListeners = function(callback, force)
{
    InspectorTest.expandAndDumpEventListeners(InspectorTest.eventListenersWidget()._eventListenersView, callback, force);
}

InspectorTest.removeFirstEventListener = function()
{
    var treeOutline = InspectorTest.eventListenersWidget()._eventListenersView._treeOutline;
    var listenerTypes = treeOutline.rootElement().children();
    for (var i = 0; i < listenerTypes.length; i++) {
        var listeners = listenerTypes[i].children();
        if (listeners.length && !listenerTypes[i].hidden) {
            listeners[0].eventListener().remove();
            listeners[0]._removeListenerBar();
            break;
        }
    }
}

InspectorTest.dumpObjectPropertySectionDeep = function(section)
{
    function domNodeToString(node) {
        if (node)
            return "'" + node.textContent + "'";
        else
            return "null";
    }
    function dumpTreeElementRecursively(treeElement, prefix) {
        if ("nameElement" in treeElement)
            InspectorTest.addResult(prefix + domNodeToString(treeElement.nameElement) + " => " + domNodeToString(treeElement.valueElement));
        else
            InspectorTest.addResult(prefix + treeElement.title);
        for (var i = 0; i < treeElement.childCount(); i++)
            dumpTreeElementRecursively(treeElement.childAt(i), prefix + "    ");
    }

    var childNodes = section.propertiesTreeOutline.rootElement().children();
    for (var i = 0; i < childNodes.length; i++) {
        dumpTreeElementRecursively(childNodes[i], "");
    }
}

// FIXME: this returns the first tree item found (may fail for same-named properties in a style).
InspectorTest.getElementStylePropertyTreeItem = function(propertyName)
{
    return InspectorTest.getFirstPropertyTreeItemForSection(InspectorTest.inlineStyleSection(), propertyName);
};

// FIXME: this returns the first tree item found (may fail for same-named properties in a style).
InspectorTest.getMatchedStylePropertyTreeItem = function(propertyName)
{
    var sectionBlocks = UI.panels.elements._stylesWidget._sectionBlocks;
    for (var block of sectionBlocks) {
        for (var section of block.sections) {
            var treeItem = InspectorTest.getFirstPropertyTreeItemForSection(section, propertyName);
            if (treeItem)
                return treeItem;
        }
    }
    return null;
};

InspectorTest.getFirstPropertyTreeItemForSection = function(section, propertyName)
{
    var outline = section.propertiesTreeOutline.rootElement();
    for (var i = 0; i < outline.childCount(); ++i) {
        var treeItem = outline.childAt(i);
        if (treeItem.name === propertyName)
            return treeItem;
    }
    return null;
};

InspectorTest.dumpStyleTreeOutline = function(treeItem, depth)
{
    var children = treeItem.rootElement().children();
    for (var i = 0; i < children.length; ++i)
        InspectorTest.dumpStyleTreeItem(children[i], "", depth || 2);
};

InspectorTest.dumpStyleTreeItem = function(treeItem, prefix, depth)
{
    // Filter out width and height properties in order to minimize
    // potential diffs.
    if (treeItem.listItemElement.textContent.indexOf(" width:") !== -1 ||
        treeItem.listItemElement.textContent.indexOf(" height:") !== -1)
        return;

    if (treeItem.listItemElement.classList.contains("inherited"))
        return;
    var typePrefix = "";
    if (treeItem.listItemElement.classList.contains("overloaded") || treeItem.listItemElement.classList.contains("inactive") || treeItem.listItemElement.classList.contains("not-parsed-ok"))
        typePrefix += "/-- overloaded --/ ";
    if (treeItem.listItemElement.classList.contains("disabled"))
        typePrefix += "/-- disabled --/ ";
    var textContent = treeItem.listItemElement.textContent;

    InspectorTest.addResult(prefix + typePrefix + textContent);
    if (--depth) {
        treeItem.expand();
        var children = treeItem.children();
        for (var i = 0; children && i < children.length; ++i)
            InspectorTest.dumpStyleTreeItem(children[i], prefix + "    ", depth);
    }
};

InspectorTest.dumpElementsTree = function(rootNode, depth, resultsArray)
{
    function beautify(element)
    {
        return element.innerText.replace(/\u200b/g, "").replace(/\n/g, "\\n").trim();
    }

    function dumpMap(name, map)
    {
        var result = [];
        for (var id of map.keys())
            result.push(id + "=" + map.get(id));
        if (!result.length)
            return "";
        return name + ":[" + result.join(",") + "]";
    }

    function markersDataDump(treeItem)
    {
        if (treeItem._elementCloseTag)
            return "";

        var markers = "";
        var node = treeItem._node;
        if (node) {
            markers += dumpMap("markers", node._markers);
            var dump = node._subtreeMarkerCount ? "subtreeMarkerCount:" + node._subtreeMarkerCount : "";
            if (dump) {
                if (markers)
                    markers += ", ";
                markers += dump;
            }
            if (markers)
                markers = " [" + markers + "]";
        }
        return markers;
    }

    function print(treeItem, prefix, depth)
    {
        if (!treeItem.root) {
            var expander;
            if (treeItem.isExpandable()) {
                if (treeItem.expanded)
                    expander = "- ";
                else
                    expander = "+ ";
            } else
                expander = "  ";

            var markers = markersDataDump(treeItem);
            var value = prefix + expander + beautify(treeItem.listItemElement) + markers;
            if (treeItem.shadowHostToolbar) {
                value = prefix + expander + "shadow-root ";
                for (var i = 0; i < treeItem.shadowHostToolbar.children.length; ++i) {
                    var button = treeItem.shadowHostToolbar.children[i];
                    var toggled = button.disabled;
                    var name = (toggled ? "<" : "") + button.textContent + (toggled ? ">" : "");
                    value += name + " ";
                }
            }
            if (resultsArray)
                resultsArray.push(value);
            else
                InspectorTest.addResult(value);
        }

        if (!treeItem.expanded)
            return;

        var children = treeItem.children();
        var newPrefix = treeItem.root ? "" : prefix + "    ";
        for (var i = 0; depth && children && i < children.length; ++i) {
            if (!children[i]._elementCloseTag)
                print(children[i], newPrefix, depth - 1);
            else
                print(children[i], prefix, depth);
        }
    }

    var treeOutline = InspectorTest.firstElementsTreeOutline();
    treeOutline.runPendingUpdates();
    print(rootNode ? treeOutline.findTreeElement(rootNode) : treeOutline.rootElement(), "", depth || 10000);
};

InspectorTest.dumpDOMUpdateHighlights = function(rootNode, callback, depth)
{
    var hasHighlights = false;

    InspectorTest.addSniffer(Elements.ElementsTreeOutline.prototype, "_updateModifiedNodes", didUpdate);

    function didUpdate()
    {
        var treeOutline = InspectorTest.firstElementsTreeOutline();
        print(rootNode ? treeOutline.findTreeElement(rootNode) : treeOutline.rootElement(), "", depth || 10000);
        if (!hasHighlights)
            InspectorTest.addResult("<No highlights>");
        if (callback)
            callback();
    }

    function print(treeItem, prefix, depth)
    {
        if (!treeItem.root) {
            var elementXPath = Components.DOMPresentationUtils.xPath(treeItem.node(), true);
            var highlightedElements = treeItem.listItemElement.querySelectorAll(".dom-update-highlight");
            for (var i = 0; i < highlightedElements.length; ++i) {
                var element = highlightedElements[i];
                var classList = element.classList;
                var xpath = elementXPath;
                if (classList.contains("webkit-html-attribute-name")) {
                    xpath += "/@" + element.textContent + " (empty)";
                } else if (classList.contains("webkit-html-attribute-value")) {
                    name = element.parentElement.querySelector(".webkit-html-attribute-name").textContent;
                    xpath += "/@" + name + " " + element.textContent;
                } else if (classList.contains("webkit-html-text-node")) {
                    xpath += "/text() \"" + element.textContent + "\"";
                }
                InspectorTest.addResult(prefix + xpath);
                hasHighlights = true;
            }
        }

        if (!treeItem.expanded)
            return;

        var children = treeItem.children();
        var newPrefix = treeItem.root ? "" : prefix + "    ";
        for (var i = 0; depth && children && i < children.length; ++i) {
            if (!children[i]._elementCloseTag)
                print(children[i], newPrefix, depth - 1);
        }
    }
}

InspectorTest.expandElementsTree = function(callback)
{
    var expandedSomething = false;
    callback = InspectorTest.safeWrap(callback);

    function expand(treeItem)
    {
        var children = treeItem.children();
        for (var i = 0; children && i < children.length; ++i) {
            var child = children[i];
            if (child.isExpandable() && !child.expanded) {
                child.expand();
                expandedSomething = true;
            }
            expand(child);
        }
    }

    function onAllNodesAvailable()
    {
        InspectorTest.firstElementsTreeOutline().runPendingUpdates();
        expand(InspectorTest.firstElementsTreeOutline().rootElement());
        // Make all promises succeed.
        setTimeout(callback.bind(null, expandedSomething));
    }
    InspectorTest.findNode(function() { return false; }, onAllNodesAvailable);
};

InspectorTest.dumpDOMAgentTree = function(node)
{
    if (!InspectorTest.domModel._document)
        return;

    function dump(node, prefix)
    {
        InspectorTest.addResult(prefix + node.nodeName());
        prefix = prefix + "    ";

        if (node.templateContent())
            dump(node.templateContent(), prefix);
        if (node.importedDocument())
            dump(node.importedDocument(), prefix);
        var shadowRoots = node.shadowRoots();
        for (var i = 0; i < shadowRoots.length; ++i)
            dump(shadowRoots[i], prefix);
        var children = node.children();
        for (var i = 0; children && i < children.length; ++i)
            dump(children[i], prefix);
    }

    dump(node, "");
};

InspectorTest.rangeText = function(range)
{
    if (!range)
        return "[undefined-undefined]";
    return "[" + range.startLine + ":" + range.startColumn + "-" + range.endLine + ":" + range.endColumn + "]";
}

InspectorTest.generateUndoTest = function(testBody)
{
    function result(next)
    {
        var testNode = InspectorTest.expandedNodeWithId(/function\s([^(]*)/.exec(testBody)[1]);
        InspectorTest.addResult("Initial:");
        InspectorTest.dumpElementsTree(testNode);

        testBody(undo);

        function undo()
        {
            InspectorTest.addResult("Post-action:");
            InspectorTest.dumpElementsTree(testNode);
            InspectorTest.expandElementsTree(expandedCallback);

            function expandedCallback(expandedSomething)
            {
                if (expandedSomething) {
                    InspectorTest.addResult("== Expanded: ==");
                    InspectorTest.dumpElementsTree(testNode);
                }
                InspectorTest.domModel.undo(redo);
            }
        }

        function redo()
        {
            InspectorTest.addResult("Post-undo (initial):");
            InspectorTest.dumpElementsTree(testNode);
            InspectorTest.expandElementsTree(expandedCallback);

            function expandedCallback(expandedSomething)
            {
                if (expandedSomething) {
                    InspectorTest.addResult("== Expanded: ==");
                    InspectorTest.dumpElementsTree(testNode);
                }
                InspectorTest.domModel.redo(done);
            }
        }

        function done()
        {
            InspectorTest.addResult("Post-redo (action):");
            InspectorTest.dumpElementsTree(testNode);
            InspectorTest.expandElementsTree(expandedCallback);

            function expandedCallback(expandedSomething)
            {
                if (expandedSomething) {
                    InspectorTest.addResult("== Expanded: ==");
                    InspectorTest.dumpElementsTree(testNode);
                }
                next();
            }
        }
    }
    result.toString = function()
    {
        return testBody.toString();
    }
    return result;
}

const indent = "    ";

InspectorTest.dumpRulesArray = function(rules, currentIndent)
{
    if (!rules)
        return;
    currentIndent = currentIndent || "";
    for (var i = 0; i < rules.length; ++i)
        InspectorTest.dumpRule(rules[i], currentIndent);
}

InspectorTest.dumpRuleMatchesArray = function(matches, currentIndent)
{
    if (!matches)
        return;
    currentIndent = currentIndent || "";
    for (var i = 0; i < matches.length; ++i)
        InspectorTest.dumpRule(matches[i].rule, currentIndent);
}

InspectorTest.dumpRule = function(rule, currentIndent)
{
    function selectorRange()
    {
        var selectors = rule.selectorList.selectors;
        if (!selectors || !selectors[0].range)
            return "";
        var ranges = [];
        for (var i = 0; i < selectors.length; ++i) {
            var range = selectors[i].range;
            ranges.push(range.startLine + ":" + range.startColumn + "-" + range.endLine + ":" + range.endColumn);
        }
        return ", " + ranges.join("; ");
    }

    currentIndent = currentIndent || "";

    if (!rule.type || rule.type === "style") {
        InspectorTest.addResult(currentIndent + rule.selectorList.text + ": [" + rule.origin + selectorRange() + "] {");
        InspectorTest.dumpStyle(rule.style, currentIndent + indent);
        InspectorTest.addResult(currentIndent + "}");
        return;
    }

    if (rule.type === "media") {
        InspectorTest.addResult(currentIndent + "@media " + rule.mediaText + " {");
        InspectorTest.dumpRulesArray(rule.childRules, currentIndent + indent);
        InspectorTest.addResult(currentIndent + "}");
        return;
    }

    if (rule.type === "import") {
        InspectorTest.addResult(currentIndent + "@import: header=" + InspectorTest.rangeText(rule.headerRange) + ", body=" + InspectorTest.rangeText(rule.bodyRange));
        return;
    }

    if (rule.type === "page" || rule.type === "font-face") {
        if (rule.type === "page")
            InspectorTest.addResult(currentIndent + rule.selectorList.text + " {");
        else
            InspectorTest.addResult(currentIndent + "@" + rule.type + " " + (rule.selectorList.text ? rule.selectorList.text + " " : "")  + "{");
        InspectorTest.dumpStyle(rule.style, currentIndent + indent);
        InspectorTest.addResult(currentIndent + "}");
        return;
    }

    if (rule.type === "charset") {
        InspectorTest.addResult("@charset");
        return;
    }

    InspectorTest.addResult(currentIndent + "[UNKNOWN RULE]: header=" + InspectorTest.rangeText(rule.headerRange) + ", body=" + InspectorTest.rangeText(rule.bodyRange));
}

InspectorTest.dumpStyle = function(style, currentIndent)
{
    currentIndent = currentIndent || "";
    if (!style) {
        InspectorTest.addResult(currentIndent + "[NO STYLE]");
        return;
    }
    for (var i = 0; i < style.cssProperties.length; ++i) {
        var property = style.cssProperties[i];
        if (!property.disabled)
            InspectorTest.addResult(currentIndent + "['" + property.name + "':'" + property.value + "'" + (property.important ? " is-important" : "") + (("parsedOk" in property) ? " non-parsed" : "") +"] @" + InspectorTest.rangeText(property.range) + " ");
        else
            InspectorTest.addResult(currentIndent + "[text='" + property.text + "'] disabled");
    }
}

InspectorTest.dumpCSSStyleDeclaration = function(style, currentIndent)
{
    currentIndent = currentIndent || "";
    if (!style) {
        InspectorTest.addResult(currentIndent + "[NO STYLE]");
        return;
    }
    var properties = style.allProperties();
    for (var i = 0; i < properties.length; ++i) {
        var property = properties[i];
        if (!property.disabled)
            InspectorTest.addResult(currentIndent + "['" + property.name + "':'" + property.value + "'" + (property.important ? " is-important" : "") + (!property["parsedOk"] ? " non-parsed" : "") +"] @" + InspectorTest.rangeText(property.range) + " ");
        else
            InspectorTest.addResult(currentIndent + "[text='" + property.text + "'] disabled");
    }
}

InspectorTest.dumpBreadcrumb = function(message)
{
    if (message)
        InspectorTest.addResult(message + ":");
    var result = [];
    var crumbs = UI.panels.elements._breadcrumbs.crumbsElement;
    var crumb = crumbs.lastChild;
    while (crumb) {
        result.unshift(crumb.textContent);
        crumb = crumb.previousSibling;
    }
    InspectorTest.addResult(result.join(" > "));
}

InspectorTest.matchingSelectors = function(matchedStyles, rule)
{
    var selectors = [];
    var matchingSelectors = matchedStyles.matchingSelectors(rule);
    for (var i = 0; i < matchingSelectors.length; ++i)
        selectors.push(rule.selectors[matchingSelectors[i]].text);
    return "[" + selectors.join(", ") + "]";
}

InspectorTest.addNewRuleInStyleSheet = function(styleSheetHeader, selector, callback)
{
    InspectorTest.addSniffer(Elements.StylesSidebarPane.prototype, "_addBlankSection", onBlankSection.bind(null, selector, callback));
    UI.panels.elements._stylesWidget._createNewRuleInStyleSheet(styleSheetHeader);
}

InspectorTest.addNewRule = function(selector, callback)
{
    // Click "Add new rule".
    document.querySelector(".styles-pane-toolbar").shadowRoot.querySelector(".largeicon-add").click();
    InspectorTest.addSniffer(Elements.StylesSidebarPane.prototype, "_addBlankSection", onBlankSection.bind(null, selector, callback));
}

function onBlankSection(selector, callback)
{
    var section = InspectorTest.firstMatchedStyleSection();
    if (typeof selector === "string")
        section._selectorElement.textContent = selector;
    section._selectorElement.dispatchEvent(InspectorTest.createKeyEvent("Enter"));
    InspectorTest.waitForSelectorCommitted(callback.bind(null, section));
}

InspectorTest.dumpInspectorHighlightJSON = function(idValue, callback)
{
    InspectorTest.nodeWithId(idValue, nodeResolved);

    function nodeResolved(node)
    {
        InspectorTest.DOMAgent.getHighlightObjectForTest(node.id, report);
    }

    function report(error, result)
    {
        InspectorTest.addResult(idValue + JSON.stringify(result, null, 2));
        callback();
    }
}

InspectorTest.waitForAnimationAdded = function(callback)
{
    InspectorTest.addSniffer(Animation.AnimationTimeline.prototype, "_addAnimationGroup", callback);
}

InspectorTest.dumpAnimationTimeline = function(timeline)
{
    for (var ui of timeline._uiAnimations) {
        InspectorTest.addResult(ui.animation().type());
        InspectorTest.addResult(ui._nameElement.innerHTML);
        InspectorTest.addResult(ui._svg.innerHTML);
    }
}

};
