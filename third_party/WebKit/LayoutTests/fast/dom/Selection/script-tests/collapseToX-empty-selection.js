description("Test that collapseToStart() and collapseToEnd() throw INVALID_STATE_ERR if no selection is made.");

var sel = window.getSelection();
var textNode = document.createTextNode("abcdef");
document.body.appendChild(textNode);

shouldThrow("sel.collapseToStart()", '"InvalidStateError: Failed to execute \'collapseToStart\' on \'Selection\': there is no selection."');
shouldThrow("sel.collapseToEnd()", '"InvalidStateError: Failed to execute \'collapseToEnd\' on \'Selection\': there is no selection."');

sel.selectAllChildren(textNode);

shouldBe("sel.collapseToStart()", "undefined");
shouldBe("sel.collapseToEnd()", "undefined");

document.body.removeChild(textNode);
