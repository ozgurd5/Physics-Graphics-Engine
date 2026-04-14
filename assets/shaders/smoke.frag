#version 460 core
in  vec2 v_TexCoord;
out vec4 FragColor;

uniform sampler2D u_Smoke;

void main()
{
    FragColor = texture(u_Smoke, v_TexCoord);
}
