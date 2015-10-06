var rotateZeroDegreesTests = {
  style: 'margin: 5px;',
  tests: {
    animateRotateNoFromAxis: {
      keyframes: [
        { rotate: '0deg' },
        { rotate: '90deg 0 1 0' },
      ],
      style: 'background: magenta;',
      samples: getLinearSamples(20, 0, 1)
    },

    animateRotateNoToAxis: {
      keyframes: [
        { rotate: '0deg 1 0 0' },
        { rotate: '90deg' },
      ],
      style: 'background: yellow;',
      samples: getLinearSamples(20, 0, 1)
    },

    animateRotateFromZeroUnder360: {
      keyframes: [
        { rotate: '0deg 1 0 0' },
        { rotate: '90deg 0 1 0' },
      ],
      style: 'background: cyan;',
      samples: getLinearSamples(20, 0, 1)
    },

    animateRotateToZeroUnder360: {
      keyframes: [
        { rotate: '90deg 0 1 0' },
        { rotate: '0deg 1 0 0' },
      ],
      style: 'background: indigo;',
      samples: getLinearSamples(20, 0, 1)
    },

    animateRotateFromZero: {
      keyframes: [
        { rotate: '0deg 1 0 0' },
        { rotate: '450deg 0 1 0' },
      ],
      style: 'background: green;',
      samples: getLinearSamples(20, 0, 1)
    },

    animateRotateToZero: {
      keyframes: [
        { rotate: '450deg 0 1 0' },
        { rotate: '0deg 1 0 0' },
      ],
      style: 'background: red;',
      samples: getLinearSamples(20, 0, 1)
    },

    animateRotateFromAndToZero: {
      keyframes: [
        { rotate: '0deg 0 1 0' },
        { rotate: '0deg 1 0 0' },
      ],
      style: 'background: blue;',
      samples: getLinearSamples(20, 0, 1)
    },
  }
};

