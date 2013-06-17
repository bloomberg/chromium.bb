description("Test that collapseToStart() and collapseToEnd() throw INVALID_STATE_ERR if no selection is made.");

var sel = window.getSelection();
var textNode = document.createTextNode("abcdef");
document.body.appendChild(textNode);

shouldThrow("sel.collapseToStart()", "'InvalidStateError: An attempt was made to use an object that is not, or is no longer, usable.'");
shouldThrow("sel.collapseToEnd()", "'InvalidStateError: An attempt was made to use an object that is not, or is no longer, usable.'");

sel.selectAllChildren(textNode);

shouldBe("sel.collapseToStart()", "undefined");
shouldBe("sel.collapseToEnd()", "undefined");

document.body.removeChild(textNode);
