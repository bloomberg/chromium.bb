description(
'This test checks invalid colors on gradients.'
);

var gradient = document.createElement('canvas').getContext('2d').createLinearGradient(0, 0, 150, 0);

shouldThrow("gradient.addColorStop(0, '')", '"SyntaxError: Failed to execute \'addColorStop\' on \'CanvasGradient\': The value provided (\'\') could not be parsed as a color."');
shouldThrow("gradient.addColorStop(0, '#cc')", '"SyntaxError: Failed to execute \'addColorStop\' on \'CanvasGradient\': The value provided (\'#cc\') could not be parsed as a color."');
shouldThrow("gradient.addColorStop(0, 'rgb(257, 0)')", '"SyntaxError: Failed to execute \'addColorStop\' on \'CanvasGradient\': The value provided (\'rgb(257, 0)\') could not be parsed as a color."');
shouldThrow("gradient.addColorStop(0, 'rgb(257, 0, 5, 0)')", '"SyntaxError: Failed to execute \'addColorStop\' on \'CanvasGradient\': The value provided (\'rgb(257, 0, 5, 0)\') could not be parsed as a color."');
