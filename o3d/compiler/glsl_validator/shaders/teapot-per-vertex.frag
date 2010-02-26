uniform sampler2D u_sampler2d;

varying vec4 v_diffuse, v_specular;
varying vec2 v_texCoord;

void main()
{
    vec4 color = v_diffuse;

    gl_FragColor = color + v_specular;
}
