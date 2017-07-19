function createDOMTree(node, siblings, depth) {
    if (!depth)
        return;
    for (var i=0; i<siblings; i++) {
        var div = document.createElement("div");
        node.appendChild(div);
        createDOMTree(div, siblings, depth-1);
    }
}

function createDeepDOMTree() {
        createDOMTree(document.body, 2, 10);
}

function createShallowDOMTree() {
        createDOMTree(document.body, 10, 2);
}