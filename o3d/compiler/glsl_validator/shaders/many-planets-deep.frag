uniform sampler2D sampler2d;

varying float v_Dot;
varying vec2 v_texCoord;

void main()
{
    vec4 color = texture2D(sampler2d,v_texCoord);
    color += vec4(0.1,0.1,0.1,1);
    gl_FragColor = vec4(color.xyz * v_Dot, color.a);
}
