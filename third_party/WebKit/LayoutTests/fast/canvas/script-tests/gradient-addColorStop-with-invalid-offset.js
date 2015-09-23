description('This test checks invalid offsets on gradients.');

var gradient = document.createElement('canvas').getContext('2d').createLinearGradient(0, 0, 150, 0);

// Check that invalid offset values throw.
shouldThrow("gradient.addColorStop(-1, '#000')");
shouldThrow("gradient.addColorStop(2, '#000')");
shouldThrow("gradient.addColorStop(Infinity, '#000')");
shouldThrow("gradient.addColorStop(-Infinity, '#000')");
shouldThrow("gradient.addColorStop(NaN, '#000')");
