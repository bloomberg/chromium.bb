function initialize_cssTest()
{

InspectorTest.dumpStyleSheetText = function(styleSheetId, callback)
{
    InspectorTest.sendCommandOrDie("CSS.getStyleSheetText", { styleSheetId: styleSheetId }, onStyleSheetText);
    function onStyleSheetText(result)
    {
        InspectorTest.log("==== Style sheet text ====");
        InspectorTest.log(result.text);
        callback();
    }
}

function updateStyleSheetRange(command, styleSheetId, expectError, options, callback)
{
    options.styleSheetId = styleSheetId;
    if (expectError)
        InspectorTest.sendCommand(command, options, onResponse);
    else
        InspectorTest.sendCommandOrDie(command, options, onSuccess);

    function onSuccess()
    {
        InspectorTest.dumpStyleSheetText(styleSheetId, callback);
    }

    function onResponse(message)
    {
        if (!message.error) {
            InspectorTest.log("ERROR: protocol method call did not return expected error. Instead, the following message was received: " + JSON.stringify(message));
            InspectorTest.completeTest();
            return;
        }
        InspectorTest.log("Expected protocol error: " + message.error.message);
        callback();
    }
}

InspectorTest.setPropertyText = updateStyleSheetRange.bind(null, "CSS.setPropertyText");
InspectorTest.setRuleSelector = updateStyleSheetRange.bind(null, "CSS.setRuleSelector");
InspectorTest.addRule = updateStyleSheetRange.bind(null, "CSS.addRule");

InspectorTest.requestMainFrameId = function(callback)
{
    InspectorTest.sendCommandOrDie("Page.enable", {}, pageEnabled);

    function pageEnabled()
    {
        InspectorTest.sendCommandOrDie("Page.getResourceTree", {}, resourceTreeLoaded);
    }

    function resourceTreeLoaded(payload)
    {
        callback(payload.frameTree.frame.id);
    }
};

InspectorTest.requestDocumentNodeId = function(callback)
{
    InspectorTest.sendCommandOrDie("DOM.getDocument", {}, onGotDocument);

    function onGotDocument(result)
    {
        callback(result.root.nodeId);
    }
};

InspectorTest.requestNodeId = function(documentNodeId, selector, callback)
{
    InspectorTest.sendCommandOrDie("DOM.querySelector", { "nodeId": documentNodeId , "selector": selector }, onGotNode);

    function onGotNode(result)
    {
        callback(result.nodeId);
    }
};

InspectorTest.dumpRuleMatch = function(ruleMatch)
{
    function log(indent, string)
    {
        var indentString = Array(indent+1).join(" ");
        InspectorTest.log(indentString + string);
    }

    var rule = ruleMatch.rule;
    var matchingSelectors = ruleMatch.matchingSelectors;
    var media = rule.media || [];
    var mediaLine = "";
    for (var i = 0; i < media.length; ++i)
        mediaLine += (i > 0 ? " " : "") + media[i].text;
    var baseIndent = 0;
    if (mediaLine.length) {
        log(baseIndent, "@media " + mediaLine);
        baseIndent += 4;
    }
    var selectorLine = "";
    var selectors = rule.selectorList.selectors;
    for (var i = 0; i < selectors.length; ++i) {
        if (i > 0)
            selectorLine += ", ";
        var matching = matchingSelectors.indexOf(i) !== -1;
        if (matching)
            selectorLine += "*";
        selectorLine += selectors[i].value;
        if (matching)
            selectorLine += "*";
    }
    selectorLine += " {";
    selectorLine += "    " + rule.origin;
    log(baseIndent, selectorLine);
    var style = rule.style;
    var cssProperties = style.cssProperties;
    for (var i = 0; i < cssProperties.length; ++i) {
        var cssProperty = cssProperties[i];
        var propertyLine = cssProperty.name + ": " + cssProperty.value + ";";
        log(baseIndent + 4, propertyLine);
    }
    log(baseIndent, "}");
};

InspectorTest.displayName = function(url)
{
    return url.substr(url.lastIndexOf("/") + 1);
};

InspectorTest.loadAndDumpMatchingRulesForNode = function(nodeId, callback)
{
    InspectorTest.sendCommandOrDie("CSS.getMatchedStylesForNode", { "nodeId": nodeId }, matchingRulesLoaded);

    function matchingRulesLoaded(result)
    {
        InspectorTest.log("Dumping matched rules: ");
        var ruleMatches = result.matchedCSSRules;
        for (var i = 0; i < ruleMatches.length; ++i) {
            var ruleMatch = ruleMatches[i];
            var origin = ruleMatch.rule.origin;
            if (origin !== "inspector" && origin !== "regular")
                continue;
            InspectorTest.dumpRuleMatch(ruleMatch);
        }
        callback();
    }
}

InspectorTest.loadAndDumpMatchingRules = function(documentNodeId, selector, callback)
{
    InspectorTest.requestNodeId(documentNodeId, selector, nodeIdLoaded);

    function nodeIdLoaded(nodeId)
    {
        InspectorTest.loadAndDumpMatchingRulesForNode(nodeId, callback);
    }
}

}
