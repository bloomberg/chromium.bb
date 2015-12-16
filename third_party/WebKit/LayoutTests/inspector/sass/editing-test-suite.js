var initialize_SassEditingTest = function() {

InspectorTest.runEditingTests = function(cssAST)
{
    InspectorTest.runTestSuite([
        function testSetPropertyName(next)
        {
            var clone = cssAST.clone();
            for (var property of clone.rules[0].properties)
                property.name.setText("NEW-NAME");
            InspectorTest.addResult(clone.document.newText());
            next();
        },

        function testSetPropertyValue(next)
        {
            var clone = cssAST.clone();
            for (var property of clone.rules[0].properties)
                property.value.setText("NEW-VALUE");
            InspectorTest.addResult(clone.document.newText());
            next();
        },

        function testDisableProperties(next)
        {
            var clone = cssAST.clone();
            for (var property of clone.rules[0].properties)
                property.setDisabled(true);
            InspectorTest.addResult(clone.document.newText());
            next();
        },

        function testEnableProperties(next)
        {
            var clone = cssAST.clone();
            for (var property of clone.rules[0].properties)
                property.setDisabled(false);
            InspectorTest.addResult(clone.document.newText());
            next();
        },

        function testRemoveFirstProperty(next)
        {
            var clone = cssAST.clone();
            clone.rules[0].properties[0].remove();
            InspectorTest.addResult(clone.document.newText());
            next();
        },

        function testRemoveAllProperties(next)
        {
            var clone = cssAST.clone();
            var properties = clone.rules[0].properties;
            while (properties.length)
                properties[0].remove();
            InspectorTest.addResult(clone.document.newText());
            next();
        },

        function testInsertFirstProperty(next)
        {
            var clone = cssAST.clone();
            var rule = clone.rules[0];
            var anchor = rule.properties[0];
            rule.insertProperty("NEW-NAME", "NEW-VALUE", false, anchor, true);
            InspectorTest.addResult(clone.document.newText());
            next();
        },

        function testInsertLastProperty(next)
        {
            var clone = cssAST.clone();
            var rule = clone.rules[0];
            var anchor = rule.properties[rule.properties.length - 1];
            rule.insertProperty("NEW-NAME", "NEW-VALUE", false, anchor, false);
            InspectorTest.addResult(clone.document.newText());
            next();
        },

        function testInsertDisabledProperty(next)
        {
            var clone = cssAST.clone();
            var rule = clone.rules[0];
            var anchor = rule.properties[1];
            rule.insertProperty("NEW-NAME", "NEW-VALUE", true, anchor, true);
            InspectorTest.addResult(clone.document.newText());
            next();
        },

        function testInsertMultipleProperties(next)
        {
            var clone = cssAST.clone();
            var rule = clone.rules[0];
            var anchor = rule.properties[rule.properties.length - 1];
            rule.insertProperty("TRAILING-4", "VALUE", false, anchor, false);
            rule.insertProperty("TRAILING-3", "VALUE", false, anchor, false);
            rule.insertProperty("TRAILING-2", "VALUE", false, anchor, false);
            rule.insertProperty("TRAILING-1", "VALUE", false, anchor, false);
            InspectorTest.addResult(clone.document.newText());
            next();
        },

        function testAppendAndRemoveLastProperty(next)
        {
            var clone = cssAST.clone();
            var rule = clone.rules[0];
            var anchor = rule.properties[rule.properties.length - 1];
            rule.insertProperty("NEW-NAME", "NEW-VALUE", false, anchor, false);
            anchor.remove();
            InspectorTest.addResult(clone.document.newText());
            next();
        },

        function testComplexChange(next)
        {
            var clone = cssAST.clone();
            var rule = clone.rules[0];
            var lastProperty = rule.properties[rule.properties.length - 1];
            rule.insertProperty("NEW-NAME", "NEW-VALUE", false, lastProperty, false);
            lastProperty.name.setText("CHANGED");
            rule.properties[0].value.setText("CHANGED");
            rule.properties[1].setDisabled(true);
            InspectorTest.addResult(clone.document.newText());
            next();
        },
    ]);
}
}
