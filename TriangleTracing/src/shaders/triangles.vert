#version 150

uniform mat4 viewMatrix, projMatrix, modelMatrix;

in vec4 position;
in vec3 normal;  // NOT normal, <vertexxid, faceid, empty>

out vec4 position_in_world_space;
out vec3 frag_vxid_fid; // <vertexid, faceid, empty>
flat out uint flat_fid;

void main()
{
    position_in_world_space = viewMatrix * modelMatrix * position;
    gl_Position = projMatrix * position_in_world_space;

//    out_texCoord = texCoord;

    frag_vxid_fid = normal;
    flat_fid      = uint(round(normal.y));

    //gl_Position = projMatrix * viewMatrix * modelMatrix * vec4(position,1.0);
}

/*attribute vec4 qt_Vertex;
attribute vec4 qt_MultiTexCoord0;
uniform mat4 qt_ModelViewProjectionMatrix;
varying vec4 qt_TexCoord0;

void main(void)
{
    gl_Position = qt_ModelViewProjectionMatrix * qt_Vertex;
    qt_TexCoord0 = qt_MultiTexCoord0;
}*/

/*#version 410

layout (location = 0) in vec3 Position;

uniform mat4 gWVP;

void main()
{
    gl_Position = gWVP * vec4(Position, 1.0);
}

#version 410

#extension GL_EXT_gpu_shader4 : enable

out uvec3 FragColor;

uniform uint gDrawIndex;
uniform uint gObjectIndex;

void main()
{
    FragColor = uvec3(gObjectIndex, gDrawIndex, gl_PrimitiveID + 1);
}*/
