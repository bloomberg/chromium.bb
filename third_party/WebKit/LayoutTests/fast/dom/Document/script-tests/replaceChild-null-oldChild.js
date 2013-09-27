description('Test behavior of Document.replaceChild() when oldChild is null.');

shouldThrow('document.replaceChild(document.firstChild, null)', '"NotFoundError: An attempt was made to reference a Node in a context where it does not exist."');
