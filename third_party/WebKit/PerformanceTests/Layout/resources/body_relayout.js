(function() {
    function createElement(tag, parent, className, id) {
        var el = document.createElement(tag);
        el.className = className;
        if (id)
            el.id = id;
        parent.appendChild(el);
        return el;
    }

    function createSet(width, height, nested) {
        var container = createElement("div", document.body, "container");
        for (var y = 0; y < height; ++y) {
            createElement("div", container, "float", "float" + x + "_" + y);
            for (var x = 0; x < width; ++x)
                createElement("div", container, "normal", "normal" + x + "_" + y);

            var nestedContainer = container;
            for ( ; nested > 0; --nested)
                nestedContainer = createElement("div", nestedContainer, "normal", "normal" + x + "_" + nested);
        }
        return container;
    }

    function createTestFunction(width, height, nested, runs) {
        var container = createSet(width, height, nested);
        nested = nested || 0;
        runs = runs || 10;
        return function() {
            for (var i = 0; i < runs; ++i) {
                // Force a layout.
                var body = document.getElementById("body")
                body.style['height'] = 800 + i + "px";
                document.body.offsetTop;
            }
            PerfTestRunner.resetRandomSeed();
        }
    }

    window.createBodyRelayoutTestFunction = createTestFunction;
})();
