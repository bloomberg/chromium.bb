// A list of all SVG tags plus optional properties:
//   * needParent       (string)        - parent element required for a valid context
//   * needChld         (string list)   - child element(s) required for a valid context
//   * needAttr         (string map)    - attribute(s) required for a valid context
//   * noRenderer       (bool)          - true if the element doesn't have an associated renderer

var SvgTags = {
    a:                    { },
    altGlyph:             { },
    altGlyphDef:          { },
    altGlyphItem:         { },
    animate:              { noRenderer: true },
    animateColor:         { },
    animateMotion:        { },
    animateTransform:     { },
    circle:               { },
    clipPath:             { },
    color_profile:        { },
    cursor:               { },
    defs:                 { },
    desc:                 { noRenderer: true },
    ellipse:              { },
    feBlend:              { needParent: 'filter' },
    feColorMatrix:        { needParent: 'filter' },
    feComponentTransfer:  { needParent: 'filter' },
    feComposite:          { needParent: 'filter' },
    feConvolveMatrix:     { needParent: 'filter', needAttr: { kernelMatrix: '0 0 0 0 0 0 0 0 0' } },
    feDiffuseLighting:    { needParent: 'filter', needChld: [ 'fePointLight' ] },
    feDisplacementMap:    { needParent: 'filter' },
    feDistantLight:       { needParent: 'feSpecularLighting' },
    feDropShadow:         { },
    feFlood:              { needParent: 'filter' },
    feFuncA:              { },
    feFuncB:              { },
    feFuncG:              { },
    feFuncR:              { },
    feGaussianBlur:       { needParent: 'filter' },
    feImage:              { needParent: 'filter' },
    feMerge:              { needParent: 'filter', needChld: [ 'feMergeNode' ] },
    feMergeNode:          { needParent: 'feMerge' },
    feMorphology:         { needParent: 'filter' },
    feOffset:             { needParent: 'filter' },
    fePointLight:         { needParent: 'feSpecularLighting' },
    feSpecularLighting:   { needParent: 'filter', needChld: [ 'fePointLight' ] },
    feSpotLight:          { needParent: 'feSpecularLighting' },
    feTile:               { needParent: 'filter' },
    feTurbulence:         { needParent: 'filter' },
    filter:               { },
    font:                 { },
    font_face:            { },
    font_face_format:     { },
    font_face_name:       { },
    font_face_src:        { },
    font_face_uri:        { },
    foreignObject:        { },
    g:                    { },
    glyph:                { },
    glyphRef:             { },
    hkern:                { },
    image:                { },
    line:                 { },
    linearGradient:       { },
    marker:               { },
    mask:                 { },
    metadata:             { noRenderer: true },
    missing_glyph:        { },
    mpath:                { },
    path:                 { },
    pattern:              { },
    polygon:              { },
    polyline:             { },
    radialGradient:       { },
    rect:                 { },
    script:               { },
    set:                  { noRenderer: true },
    stop:                 { },
    style:                { },
    svg:                  { },
    switch:               { },
    symbol:               { },
    text:                 { },
    textPath:             { },
    title:                { noRenderer: true },
    tref:                 { },
    tspan:                { },
    use:                  { },
    view:                 { },
    vkern:                { },
}

// SVG element class shorthands as defined by the spec.
var SvgTagClasses = {
    CLASS_ANIMATION: [
        // http://www.w3.org/TR/SVG/intro.html#TermAnimationElement
        'animate', 'animateColor', 'animateMotion', 'animateTransform', 'set'
    ],

    CLASS_BASIC_SHAPE: [
        // http://www.w3.org/TR/SVG/intro.html#TermBasicShapeElement
        'circle', 'ellipse', 'line', 'polygon', 'polyline', 'rect'
    ],

    CLASS_CONTAINER: [
        // http://www.w3.org/TR/SVG/intro.html#TermContainerElement
        'a', 'defs', 'glyph', 'g', 'marker', 'mask', 'missing-glyph', 'pattern', 'svg', 'switch',
        'symbol'
    ],

    CLASS_DESCRIPTIVE: [
        // http://www.w3.org/TR/SVG/intro.html#TermDescriptiveElement
        'desc', 'metadata', 'title'
    ],

    CLASS_FILTER_PRIMITIVE: [
         // http://www.w3.org/TR/SVG/intro.html#TermFilterPrimitiveElement
        'feBlend', 'feColorMatrix', 'feComponentTransfer', 'feComposite', 'feConvolveMatrix',
        'feDiffuseLighting', 'feDisplacementMap', 'feFlood', 'feGaussianBlur', 'feImage',
        'feMerge', 'feMorphology', 'feOffset', 'feSpecularLighting', 'feTile', 'feTurbulence'
    ],

    CLASS_GRADIENT: [
        // http://www.w3.org/TR/SVG/intro.html#TermGradientElement
        'linearGradient', 'radialGradient'
    ],

    CLASS_GRAPHICS: [
        // http://www.w3.org/TR/SVG/intro.html#TermGraphicsElement
        'circle', 'ellipse', 'image', 'line', 'path', 'polygon', 'polyline', 'rect', 'text', 'use'
    ],

    CLASS_LIGHT_SOURCE: [
        // http://www.w3.org/TR/SVG/intro.html#TermLightSourceElement
        'feDistantLight', 'fePointLight', 'feSpotLight'
    ],

    CLASS_SHAPE: [
        // http://www.w3.org/TR/SVG/intro.html#TermShapeElement
        'circle', 'ellipse', 'line', 'path', 'polygon', 'polyline', 'rect'
    ],

    CLASS_STRUCTURAL: [
        // http://www.w3.org/TR/SVG/intro.html#TermStructuralElement
        'defs', 'g', 'svg', 'symbol', 'use'
    ],
};
