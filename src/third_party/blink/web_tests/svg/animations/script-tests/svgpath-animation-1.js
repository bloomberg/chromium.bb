description("Test 'by' animation on path. You should see a green 100x100 path and only PASS messages");
createSVGTestCase();

// Setup test document
var path = createSVGElement("path");
path.setAttribute("id", "path");
path.setAttribute("d", "M 40 40 L 60 40 L 60 60 L 40 60 z");
path.setAttribute("fill", "green");
path.setAttribute("onclick", "executeTest()");

var animate = createSVGElement("animate");
animate.setAttribute("id", "animation");
animate.setAttribute("attributeName", "d");
animate.setAttribute("from", "M 40 40 L 60 40 L 60 60 L 40 60 z");
animate.setAttribute("by", "M 0 0 L 100 0 L 100 100 L 0 100 z");
animate.setAttribute("begin", "click");
animate.setAttribute("fill", "freeze");
animate.setAttribute("dur", "4s");
path.appendChild(animate);
rootSVGElement.appendChild(path);

// Setup animation test
function sample1() {
    shouldBeEqualToString("path.getAttribute('d')", "M 40 40 L 60 40 L 60 60 L 40 60 z");
}

function sample2() {
    shouldBeEqualToString("path.getAttribute('d')", "M 40 40 L 109.975 40 L 109.975 109.975 L 40 109.975 Z");
}

function sample3() {
    shouldBeEqualToString("path.getAttribute('d')", "M 40 40 L 110.025 40 L 110.025 110.025 L 40 110.025 Z");
}

function sample4() {
    shouldBeEqualToString("path.getAttribute('d')", "M 40 40 L 159.975 40 L 159.975 159.975 L 40 159.975 Z");
}

function sample5() {
    shouldBeEqualToString("path.getAttribute('d')", "M 40 40 L 160 40 L 160 160 L 40 160 Z");
}

function executeTest() {
    const expectedValues = [
        // [animationId, time, sampleCallback]
        ["animation", 0.0,   sample1],
        ["animation", 1.999, sample2],
        ["animation", 2.001, sample3],
        ["animation", 3.999, sample4],
        ["animation", 4.001, sample5]
    ];

    runAnimationTest(expectedValues);
}

var successfullyParsed = true;
