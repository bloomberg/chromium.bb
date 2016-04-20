description("Test to check if setBaseAndExtent guard node with null owner document (Bug 31680)");

var sel = window.getSelection();
var docType = document.implementation.createDocumentType('c', null, null);

sel.setBaseAndExtent(docType, 0, null, 0);
shouldBeNull("sel.anchorNode");

sel.setBaseAndExtent(null, 0, docType, 0);
shouldBeNull("sel.anchorNode");

shouldThrow("sel.collapse(docType)", '"InvalidNodeTypeError: Failed to execute \'collapse\' on \'Selection\': The node provided is of type \'c\'."');

sel.selectAllChildren(docType);
shouldBeNull("sel.anchorNode");

sel.extend(docType, 0);
shouldBeNull("sel.anchorNode");

sel.containsNode(docType);
shouldBeNull("sel.anchorNode");

shouldBeFalse("sel.containsNode(docType)");

var successfullyParsed = true;
