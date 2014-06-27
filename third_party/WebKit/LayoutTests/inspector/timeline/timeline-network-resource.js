// Sample script resource to find in timeline data

var element = document.createElement("div");
element.innerHTML = "Script resource loaded";
document.body.appendChild(element);

testRunner.evaluateInWebInspector(1000, "InspectorTest.scriptEvaluated();");