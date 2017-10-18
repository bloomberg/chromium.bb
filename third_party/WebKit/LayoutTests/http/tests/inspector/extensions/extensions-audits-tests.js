var initialize_ExtensionsAuditsTest = function()
{
    InspectorTest.preloadModule('extensions_test_runner');

    // We will render DOM node results, so preload elements.
    InspectorTest.preloadPanel("elements");
    InspectorTest.preloadPanel("audits");
}
