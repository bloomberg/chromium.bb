description(
'This test checks invalid colors on gradients.'
);

var gradient = document.createElement('canvas').getContext('2d').createLinearGradient(0, 0, 150, 0);

shouldThrow("gradient.addColorStop(0, '')", "'SyntaxError: An invalid or illegal string was specified.'");
shouldThrow("gradient.addColorStop(0, '#cc')", "'SyntaxError: An invalid or illegal string was specified.'");
shouldThrow("gradient.addColorStop(0, 'rgb(257, 0)')", "'SyntaxError: An invalid or illegal string was specified.'");
shouldThrow("gradient.addColorStop(0, 'rgb(257, 0, 5, 0)')", "'SyntaxError: An invalid or illegal string was specified.'");
