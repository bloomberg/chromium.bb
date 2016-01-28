var initialize_SassTest = function() {

InspectorTest.preloadModule("sass");

var sassWorkspaceAdapter = null;
InspectorTest.sassWorkspaceAdapter = function()
{
    if (!sassWorkspaceAdapter)
        sassWorkspaceAdapter = new WebInspector.SASSWorkspaceAdapter(InspectorTest.cssModel, WebInspector.workspace, WebInspector.networkMapping);
    return sassWorkspaceAdapter;
}

var cssParser = null;

InspectorTest.cssParser = function()
{
    if (!cssParser)
        cssParser = new WebInspector.CSSParser();
    return cssParser;
}

InspectorTest.parseCSS = function(url, text)
{
    return WebInspector.SASSSupport.parseCSS(InspectorTest.cssParser(), url, text);
}

InspectorTest.parseSCSS = function(url, text)
{
    return self.runtime.instancePromise(WebInspector.TokenizerFactory)
        .then(onTokenizer);

    function onTokenizer(tokenizer)
    {
        return WebInspector.SASSSupport.parseSCSS(url, text, tokenizer);
    }
}

InspectorTest.loadASTMapping = function(header, callback)
{
    console.assert(header.cssModel() === InspectorTest.sassWorkspaceAdapter()._cssModel, "The header could not be processed by main target workspaceAdapter");
    var tokenizerFactory = null;
    var sourceMap = null;

    var completeSourceMapURL = WebInspector.ParsedURL.completeURL(header.sourceURL, header.sourceMapURL);
    WebInspector.SourceMap.load(completeSourceMapURL, header.sourceURL, onSourceMapLoaded);

    self.runtime.instancePromise(WebInspector.TokenizerFactory)
        .then(tf => tokenizerFactory = tf)
        .then(maybeStartLoading);

    function onSourceMapLoaded(sm)
    {
        sourceMap = sm;
        maybeStartLoading();
    }

    function maybeStartLoading()
    {
        if (!sourceMap || !tokenizerFactory)
            return;
        var client = InspectorTest.sassWorkspaceAdapter().trackSources(sourceMap);
        WebInspector.SASSLiveSourceMap._loadMapping(client, InspectorTest.cssParser(), tokenizerFactory, sourceMap)
            .then(callback)
            .then(() => client.dispose())
    }
}

InspectorTest.dumpAST = function(ast)
{
    var lines = [String.sprintf("=== AST === %s", ast.document.url)];
    for (var i = 0; i < ast.rules.length; ++i) {
        var rule = ast.rules[i];
        lines.push(String.sprintf("rule %d: \"%s\"", i, rule.selector));
        var ruleLines = dumpRule(rule);
        lines = lines.concat(indent(ruleLines));
    }
    lines.push("======");
    InspectorTest.addResult(lines.join("\n"));
    return ast;

    function dumpRule(rule)
    {
        var lines = [];
        for (var i = 0; i < rule.properties.length; ++i) {
            var property = rule.properties[i];
            lines.push("property " + i);
            var propertyLines = dumpProperty(property);
            lines = lines.concat(indent(propertyLines));
        }
        return lines;
    }

    function dumpProperty(property)
    {
        var lines = [];
        lines.push(String.sprintf("name: \"%s\"", property.name.text));
        lines = lines.concat(indent(dumpTextNode(property.name)));
        lines.push(String.sprintf("value: \"%s\"", property.value.text));
        lines = lines.concat(indent(dumpTextNode(property.value)));
        lines.push(String.sprintf("range: %s", property.range.toString()));
        lines.push(String.sprintf("disabled: %s", property.disabled));
        return lines;
    }

    function dumpTextNode(textNode)
    {
        return [String.sprintf("range: %s", textNode.range.toString())];
    }

}

function indent(lines)
{
    return lines.map(line => "    " + line);
}

InspectorTest.dumpASTDiff = function(diff)
{
    InspectorTest.addResult("=== Diff ===");
    var changesPerRule = new Map();
    for (var change of diff.changes) {
        var oldRule = change.oldRule;
        var ruleChanges = changesPerRule.get(oldRule);
        if (!ruleChanges) {
            ruleChanges = [];
            changesPerRule.set(oldRule, ruleChanges);
        }
        ruleChanges.push(change);
    }
    var T = WebInspector.SASSSupport.PropertyChangeType;
    for (var rule of changesPerRule.keys()) {
        var changes = changesPerRule.get(rule);
        var names = [];
        var values = [];
        for (var property of rule.properties) {
            names.push(str(property.name, "    "));
            values.push(str(property.value));
        }
        for (var i = changes.length - 1; i >= 0; --i) {
            var change = changes[i];
            var newProperty = change.newRule.properties[change.newPropertyIndex];
            var oldProperty = change.oldRule.properties[change.oldPropertyIndex];
            switch (change.type) {
            case T.PropertyAdded:
                names.splice(change.oldPropertyIndex, 0, str(newProperty.name, "[+] "));
                values.splice(change.oldPropertyIndex, 0, str(newProperty.value));
                break;
            case T.PropertyRemoved:
                names[change.oldPropertyIndex] = str(oldProperty.name, "[-] ");
                break;
            case T.PropertyToggled:
                names[change.oldPropertyIndex] = str(oldProperty.name, "[T] ");
                break;
            case T.NameChanged:
                names[change.oldPropertyIndex] = str(oldProperty.name, "[M] ");
                break;
            case T.ValueChanged:
                values[change.oldPropertyIndex] = str(oldProperty.value, "[M] ");
                break;
            }
        }
        InspectorTest.addResult("Changes for rule: " + rule.selector);
        names = indent(names);
        for (var i = 0; i < names.length; ++i)
            InspectorTest.addResult(names[i] + ": " + values[i]);
    }

    function str(node, prefix)
    {
        prefix = prefix || "";
        return prefix + node.text.trim();
    }
}

InspectorTest.validateASTRanges = function(ast)
{
    var invalidNodes = [];
    for (var rule of ast.rules) {
        for (var property of rule.properties) {
            validate(property.name);
            validate(property.value);
        }
    }
    if (invalidNodes.length) {
        InspectorTest.addResult("Bad ranges: " + invalidNodes.length);
        for (var node of invalidNodes)
            InspectorTest.addResult(String.sprintf("  - range: %s text: %s", node.range.toString(), node.text));
    } else {
        InspectorTest.addResult("Ranges OK.");
    }
    return ast;

    function validate(textNode)
    {
        if (textNode.range.extract(textNode.document.text) !== textNode.text)
            invalidNodes.push(textNode);
    }
}

InspectorTest.validateMapping = function(mapping)
{
    InspectorTest.addResult("Mapped CSS: " + mapping._cssToSass.size);
    InspectorTest.addResult("Mapped SCSS: " + mapping._sassToCss.size);
    var cssNodes = mapping._cssToSass.keysArray();
    var staleCSS = 0;
    var staleSASS = 0;
    for (var i = 0; i < cssNodes.length; ++i) {
        var cssNode = cssNodes[i];
        staleCSS += cssNode.document !== mapping.cssAST().document ? 1 : 0;
        var sassNode = mapping.toSASSNode(cssNode);
        var sassAST = mapping.sassModels().get(sassNode.document.url);
        staleSASS += sassNode.document !== sassAST.document ? 1 : 0;
    }
    if (staleCSS || staleSASS) {
        InspectorTest.addResult("ERROR: found stale entries");
        InspectorTest.addResult("   -stale CSS: " + staleCSS);
        InspectorTest.addResult("   -stale SASS: " + staleSASS);
    } else {
        InspectorTest.addResult("No stale entries found.");
    }
}

InspectorTest.updateCSSText = function(url, newText)
{
    var styleSheetIds = InspectorTest.cssModel.styleSheetIdsForURL(url)
    var promises = styleSheetIds.map(id => InspectorTest.cssModel.setStyleSheetText(id, newText, true));
    return Promise.all(promises);
}

InspectorTest.updateSASSText = function(url, newText)
{
    var uiSourceCode = WebInspector.workspace.uiSourceCodeForURL(url);
    uiSourceCode.addRevision(newText);
}

}

