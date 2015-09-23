description('Test behavior of Document.replaceChild() when oldChild is null.');

shouldThrow('document.replaceChild(document.firstChild, null)', '"TypeError: Failed to execute \'replaceChild\' on \'Node\': parameter 2 is not of type \'Node\'."');
