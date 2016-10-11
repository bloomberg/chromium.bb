var validWriteExpectations = ['ink',
                               'objects',
                               'objects ink'];

var writeInvalidExpectations = ['none objects', // Combinations with none invalid
                                'none', // none currently not supported
                                'abc def',
                                '',
                                'objects objects',
                                'ink ink',
                                'edges'] // not supported yet

setup({ explicit_done: true });

function styleAndComputedStyleReadback() {

    var testContainer = document.createElement("div");
    document.body.appendChild(testContainer);

    for (validValue of validWriteExpectations) {
        testContainer.style.textDecorationSkip = validValue;
        test(function() {
            assert_equals(testContainer.style.textDecorationSkip, validValue);
            assert_equals(getComputedStyle(testContainer).textDecorationSkip, validValue);
        }, 'text-decoration-skip value ' + validValue + ' should be equal when reading it back from style and getComputedstyle.');
    }

    document.body.removeChild(testContainer);
}

function testInheritance() {
    var testContainer = document.createElement('div');
    var testContainerChild = document.createElement('div');
    testContainer.appendChild(testContainerChild);
    document.body.appendChild(testContainer);

    for (validValue of validWriteExpectations) {
        testContainer.style.textDecorationSkip = validValue;
        test(function() {
            assert_equals(testContainerChild.style.textDecorationSkip, '');
            assert_equals(getComputedStyle(testContainerChild).textDecorationSkip, validValue);
        }, 'text-decoration-skip value ' + validValue + ' should be inherited to computed style of child.');
    }
    document.body.removeChild(testContainer);
}

function validWriteTests() {
    for (validValue of validWriteExpectations) {
        test(function() {
            assert_true(CSS.supports('text-decoration-skip', validValue));
        }, 'Value ' + validValue + ' valid for property text-decoration-skip');
    }
}

function invalidWriteTests() {
    for (invalidValue of writeInvalidExpectations) {
        test(function() {
            assert_false(CSS.supports('text-decoration-skip', invalidValue));
        }, 'Value ' + invalidValue + ' invalid for property text-decoration-skip');
    }
}

window.addEventListener('load', function() {
    validWriteTests();
    invalidWriteTests();
    styleAndComputedStyleReadback();
    testInheritance();
    done();
});
