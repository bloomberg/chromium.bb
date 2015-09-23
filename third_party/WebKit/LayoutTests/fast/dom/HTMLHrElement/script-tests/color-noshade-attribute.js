description("HTMLHeaderElement color and noshade attribute test");

shouldBeEqualToString("window.getComputedStyle(document.getElementById('hrElement1'),null).getPropertyValue('border-color')","rgb(255, 0, 0)");
shouldBeEqualToString("window.getComputedStyle(document.getElementById('hrElement1'),null).getPropertyValue('background-color')","rgb(255, 0, 0)");
shouldBeEqualToString("window.getComputedStyle(document.getElementById('hrElement2'),null).getPropertyValue('border-color')","rgb(0, 0, 255)");
shouldBeEqualToString("window.getComputedStyle(document.getElementById('hrElement2'),null).getPropertyValue('background-color')","rgb(0, 0, 255)");
shouldBeEqualToString("window.getComputedStyle(document.getElementById('hrElement3'),null).getPropertyValue('border-color')","rgb(0, 0, 0)");
shouldBeEqualToString("window.getComputedStyle(document.getElementById('hrElement3'),null).getPropertyValue('background-color')","rgb(0, 0, 0)");
shouldBeEqualToString("window.getComputedStyle(document.getElementById('hrElement4'),null).getPropertyValue('border-color')","rgb(128, 128, 128)");
shouldBeEqualToString("window.getComputedStyle(document.getElementById('hrElement4'),null).getPropertyValue('background-color')","rgb(128, 128, 128)");
document.getElementById('hrElement5').setAttribute('color','yellow');
document.getElementById('hrElement5').setAttribute('noshade', '');
shouldBeEqualToString("window.getComputedStyle(document.getElementById('hrElement5'),null).getPropertyValue('border-color')","rgb(255, 255, 0)");
shouldBeEqualToString("window.getComputedStyle(document.getElementById('hrElement5'),null).getPropertyValue('background-color')","rgb(255, 255, 0)");
document.getElementById('hrElement6').setAttribute('noshade', '');
document.getElementById('hrElement6').setAttribute('color','green');
shouldBeEqualToString("window.getComputedStyle(document.getElementById('hrElement6'),null).getPropertyValue('border-color')","rgb(0, 128, 0)");
shouldBeEqualToString("window.getComputedStyle(document.getElementById('hrElement6'),null).getPropertyValue('background-color')","rgb(0, 128, 0)");

