function initialize_EditDOMTests() {
    InspectorTest.preloadModule("elements_test_runner");

    // Preload codemirror which is used for "Edit as HTML".
    InspectorTest.preloadModule("text_editor");
}
