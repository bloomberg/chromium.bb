// Generates code for a CSS paint API function which logs the given properties
// to the console.
//
// Usage:
// generatePaintStyleLogging([
//      '--foo',
//      'line-height',
// ]);

function generatePaintStyleLogging(properties) {
    const json = JSON.stringify(properties);
    return `
        registerPaint('test', class {
            static get inputProperties() { return ${json}; }
            paint(ctx, geom, styleMap) {
                const properties = styleMap.getProperties().sort();
                for (let i = 0; i < properties.length; i++) {
                    const value = styleMap.get(properties[i]);
                    console.log(properties[i] + ': ' + (value ? value.cssText: '[null]'));
                }
            }
        });
    `;
}
