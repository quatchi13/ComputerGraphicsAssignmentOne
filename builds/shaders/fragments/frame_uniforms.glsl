// Stores uniforms that change every frame (ex: time, camera data)
layout (std140, binding = 0) uniform b_FrameLevelUniforms {
    // The camera's view matrix
    uniform mat4 u_View;
    // The camera's projection matrix
    uniform mat4 u_Projection;
    // The combined viewProject matrix
    uniform mat4 u_ViewProjection;
    // The position of the camera in world space
    uniform vec4  u_CamPos;
    // The time in seconds since the start of the application
    uniform float u_Time;    
    // The time in seconds since the last frame
    uniform float u_DeltaTime;
    // Lets us store up to 32 bool flags in one value
    uniform uint  u_Flags;
};

// Stores uniforms that change every object/instance
layout (std140, binding = 1) uniform b_InstanceLevelUniforms {
    // Complete MVP
    uniform mat4 u_ModelViewProjection;
    // Just the model transform, we'll do worldspace lighting
    uniform mat4 u_Model;
    // Just the model * view, for converting to view space
    uniform mat4 u_ModelView;
    // Normal Matrix for transforming normals
    uniform mat4 u_NormalMatrix;
};

#define FLAG_ENABLE_LIGHTING_NONE (1 << 0)
#define FLAG_ENABLE_LIGHTING_AMBIENT_ONLY (1 << 1)
#define FLAG_ENABLE_LIGHTING_SPECULAR_ONLY (1 << 2)
#define FLAG_ENABLE_LIGHTING_AMBIENT_AND_SPECULAR (1 << 3)
#define FLAG_ENABLE_LIGHTING_AMBIENT_AND_SPECULAR_AND_SHADER (1 << 4)
#define FLAG_ENABLE_COLOR_DIFFUSE_WARP (1 << 5)
#define FLAG_ENABLE_COLOR_SPECULAR_WARP (1 << 6)
#define FLAG_ENABLE_COLOR_GRADING_WARM (1 << 7)
#define FLAG_ENABLE_COLOR_GRADING_COOL (1 << 8)
#define FLAG_ENABLE_COLOR_GRADING_CUSTOM (1 << 9)



bool IsFlagSet(uint flag) {
    return (u_Flags & flag) != 0;
}