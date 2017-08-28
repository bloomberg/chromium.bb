var initialize_StylesUpdateLinks = function() {
    function flattenRuleRanges(rule)
    {
        var ranges = [];
        var medias = rule.media || [];
        for (var i = 0; i < medias.length; ++i) {
            var media = medias[i];
            if (!media.range)
                continue;
            ranges.push({
                range: media.range,
                name: "media #" + i
            });
        }
        for (var i = 0; i < rule.selectors.length; ++i) {
            var selector = rule.selectors[i];
            if (!selector.range)
                continue;
            ranges.push({
                range: selector.range,
                name: "selector #" + i
            });
        }
        if (rule.style.range) {
            ranges.push({
                range: rule.style.range,
                name: "style range"
            });
        }
        var properties = rule.style.allProperties();
        for (var i = 0; i < properties.length; ++i) {
            var property = properties[i];
            if (!property.range)
                continue;
            ranges.push({
                range: property.range,
                name: "property >>" + property.text + "<<"
            });
        }
        return ranges;
    }

    function compareRuleRanges(lazyRule, originalRule)
    {
        if (lazyRule.selectorText !== originalRule.selectorText) {
            InspectorTest.addResult("Error: rule selectors are not equal: " + lazyRule.selectorText + " != " + originalRule.selectorText);
            return false;
        }
        var flattenLazy = flattenRuleRanges(lazyRule);
        var flattenOriginal = flattenRuleRanges(originalRule);
        if (flattenLazy.length !== flattenOriginal.length) {
            InspectorTest.addResult("Error: rule range amount is not equal: " + flattenLazy.length + " != " + flattenOriginal.length);
            return false
        }
        for (var i = 0; i < flattenLazy.length; ++i) {
            var lazyRange = flattenLazy[i];
            var originalRange = flattenOriginal[i];
            if (lazyRange.name !== originalRange.name) {
                InspectorTest.addResult("Error: rule names are not equal: " + lazyRange.name + " != " + originalRange.name);
                return false;
            }
            if (!lazyRange.range.equal(originalRange.range)) {
                InspectorTest.addResult("Error: ranges '" + lazyRange.name + "' are not equal: " + lazyRange.range.toString() + " != " + originalRange.range.toString());
                return false;
            }
        }
        InspectorTest.addResult(flattenLazy.length + " rule ranges are equal.");
        return true;
    }

    InspectorTest.validateRuleRanges = function(selector, rules, callback)
    {
        InspectorTest.selectNodeAndWaitForStyles("other", onOtherSelected);

        function onOtherSelected()
        {
            InspectorTest.selectNodeAndWaitForStyles(selector, onContainerSelected);
        }

        function onContainerSelected()
        {
            var fetchedRules = InspectorTest.getMatchedRules();
            if (fetchedRules.length !== rules.length) {
                InspectorTest.addResult(String.sprintf("Error: rules sizes are not equal! Expected: %d, actual: %d", fetchedRules.length, rules.length));
                InspectorTest.completeTest();
                return;
            }
            for (var i = 0; i < fetchedRules.length; ++i) {
                if (!compareRuleRanges(rules[i], fetchedRules[i])) {
                    InspectorTest.completeTest();
                    return;
                }
            }
            callback();
        }
    }

    InspectorTest.getMatchedRules = function()
    {
        var rules = [];
        for (var block of UI.panels.elements._stylesWidget._sectionBlocks) {
            for (var section of block.sections) {
                var rule = section.style().parentRule;
                if (rule)
                    rules.push(rule);
            }
        }
        return rules;
    }
}
