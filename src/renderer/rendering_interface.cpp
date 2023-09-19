#include "../pch.h"
#include "renderer.h"
#include "glad/glad.h"
#include "shelf_pack.hpp"

#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image/stb_image.h"

extern "C"
{
#include <ft2build.h>
#include FT_FREETYPE_H
}

struct FontManager
{
  std::unordered_map<sr::FontHandle, sr::Font> LoadedFonts;
  FT_Library Libary;
};

FontManager *sFontManager = nullptr;

void InitFontManager()
{
  if (sFontManager)
  {
    return;
  }

  sFontManager = new FontManager();
  FT_Init_FreeType(&sFontManager->Libary);
}

void CleanUpFontManager()
{
  if (sFontManager)
  {
    delete sFontManager;
    sFontManager = nullptr;
  }
}

FontManager *GetFontManager()
{
  if (!sFontManager)
  {
    InitFontManager();
  }
  return sFontManager;
}

sr::SRContext *sr::SRC = nullptr;

const char *basicMeshVertexShader = R"(
  #version 330 core

  layout(location = 0) in vec3 vPosition;
  layout(location = 1) in vec3 vNormal;
  layout(location = 2) in vec2 vTexCoord;
  layout(location = 3) in vec4 vColor;
  layout(location = 4) in vec4 vColor2;

  uniform mat4 ProjectionMatrix = mat4(1.0);

  out vec4 Color;
  out vec4 Color2;
  out vec2 TexCoord;
  out vec3 Normal;


  void main()
  {
    Color = vColor;
    Color2 = vColor2;
    TexCoord = vTexCoord;
    Normal = vNormal;
    gl_Position = ProjectionMatrix * vec4(vPosition, 1.0);
  }
)";

const char *basicMeshFragmentShader = R"(
  #version 330 core

  layout(location = 0) out vec4 fragColor;

  in vec4 Color;
  in vec2 TexCoord;

  uniform sampler2D Texture;
  uniform bool      UseTexture = false;

  void main()
  {
    //fragColor = vec4(TexCoord, 0.0, 1.0);
    if (UseTexture)
    {
      fragColor = texture(Texture, TexCoord) * Color;
    }
    else
    {
      fragColor = Color;
    }
  }
)";

const char *distanceFieldFragmentShader = R"(
    #version 330 core

  layout(location = 0) out vec4 fragColor;

  in vec4 Color;
  in vec4 Color2;
  in vec2 TexCoord;
  in vec3 Normal; // use x for border width

  uniform sampler2D Texture;


  vec3 outline_color  = vec3(0.2,0.2,0.7);

  uniform float glyph_center   = 0.5;
  uniform float smoothing = 0.04;
  //uniform float outline_center = 0.5 - outlineWidth;

  void main() {
    float outlineWidth = 0.5 - clamp(Normal.x, 0.0, 0.5);

    vec4 sdf = texture(Texture, TexCoord.st);
    float d  = sdf.r;


    vec4 result = vec4(0.0);
    if (Normal.x < 0.01) {
      float alpha = smoothstep(glyph_center - smoothing, glyph_center + smoothing, d);

      result = vec4(Color.xyz, alpha);
    }
    else {
      float alpha = smoothstep(outlineWidth - smoothing, outlineWidth + smoothing, d);
      float outline_factor = smoothstep(glyph_center, glyph_center + smoothing, d);

      result = vec4(mix(Color2.xyz, Color.xyz, outline_factor), alpha);
    }

    // Drop Shadow
    //float d2 = texture(Texture, TexCoord.st+vec2(0.01, 0.01)).r + smoothing;
    
    //fragColor = vec4(mix(Color.xyz, Color2.xyz, border), alpha);



    fragColor = result;
  }

)";

static const unsigned int FONT_TEXTURE_SIZE = 2048;
static const unsigned int FONT_TEXTURE_DEPTH = 1;

char const *gl_error_string(GLenum const err)
{
  switch (err)
  {
  // opengl 2 errors (8)
  case GL_NO_ERROR:
    return "GL_NO_ERROR";

  case GL_INVALID_ENUM:
    return "GL_INVALID_ENUM";

  case GL_INVALID_VALUE:
    return "GL_INVALID_VALUE";

  case GL_INVALID_OPERATION:
    return "GL_INVALID_OPERATION";

  case GL_STACK_OVERFLOW:
    return "GL_STACK_OVERFLOW";

  case GL_STACK_UNDERFLOW:
    return "GL_STACK_UNDERFLOW";

  case GL_OUT_OF_MEMORY:
    return "GL_OUT_OF_MEMORY";

  // opengl 3 errors (1)
  case GL_INVALID_FRAMEBUFFER_OPERATION:
    return "GL_INVALID_FRAMEBUFFER_OPERATION";
  default:
    return nullptr;
  }
}

#define glCall(x)                                                                          \
  x;                                                                                       \
  {                                                                                        \
    GLenum glob_err = 0;                                                                   \
    while ((glob_err = glGetError()) != GL_NO_ERROR)                                       \
    {                                                                                      \
      printf("GL_ERROR calling \"%s\": %s %s\n", #x, gl_error_string(glob_err), __FILE__); \
    }                                                                                      \
  }

namespace sr
{

  static RectangleCorners operator+(const RectangleCorners &corner, const glm::vec2 &offset)
  {
    RectangleCorners result = corner;
    result.TopLeft += offset;
    result.BottomLeft += offset;
    result.BottomRight += offset;
    result.TopRight += offset;
    return result;
  }

  static RectangleCorners &operator+=(RectangleCorners &corner, const glm::vec2 &offset)
  {
    corner.TopLeft += offset;
    corner.BottomLeft += offset;
    corner.BottomRight += offset;
    corner.TopRight += offset;
    return corner;
  }

  static bool operator==(const ScissorTest &sc1, const ScissorTest &sc2)
  {
    if (sc1.Enabled != sc2.Enabled)
    {
      return false;
    }

    if (sc1.Enabled)
    {
      return sc1.X == sc2.X && sc1.Y == sc2.Y &&
             sc1.Width == sc2.Width && sc1.Height == sc2.Height;
    }

    // cs1 and cs2 are disabled, so we return true
    return true;
  }

  static bool operator!=(const ScissorTest &sc1, const ScissorTest &sc2)
  {
    return !(sc1 == sc2);
  }

  R_API void srLoad(SRLoadProc loadAddress)
  {
    gladLoadGLLoader(loadAddress);
    srInitGL();
    if (SRC == NULL)
    {
      SRC = new SRContext();
      srInitContext(SRC);
    }
  }

  R_API void srTerminate()
  {
    if (SRC)
    {
      // Unload loaded meshes
      for (Mesh &mesh : SRC->AutoReleaseMeshes)
      {
        srUnloadMesh(&mesh);
      }

      delete SRC;
      SRC = NULL;
    }

    CleanUpFontManager();
  }

  R_API void srInitGL()
  {
    SR_TRACE("OpenGL-Context: %s", glGetString(GL_VERSION));
    glCall(glEnable(GL_DEPTH_TEST));
    glCall(glEnable(GL_BLEND));
    glCall(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
    glCall(glEnable(GL_MULTISAMPLE));
  }

  R_API void srInitContext(SRContext *context)
  {
    if (context->DefaultShader.ID == 0)
    {
      context->DefaultShader = srLoadShader(basicMeshVertexShader, basicMeshFragmentShader);
    }
    if (context->DistanceFieldShader.ID == 0)
    {
      context->DistanceFieldShader = srLoadShader(basicMeshVertexShader, distanceFieldFragmentShader);
    }
    context->Scissor.Enabled = false;
    context->MainRenderBatch = srLoadRenderBatch(5000);
  }

  R_API SRContext *srGetContext()
  {
    return SRC;
  }

  R_API void srNewFrame(int frameWidth, int frameHeight, int windowWidth, int windowHeight)
  {
    glDisable(GL_SCISSOR_TEST);
    srDisableScissor();
    srViewport(0, 0, (float)frameWidth, (float)frameHeight);
    srClearColor(0.8f, 0.8f, 0.8f, 1.0f);
    srClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    SRC->CurrentProjection = glm::orthoLH(0.0f, (float)windowWidth, (float)windowHeight, 0.0f, -1.0f, 1.0f);
    SRC->MainRenderBatch.CurrentDepth = 0.0f;

    assert((frameWidth / windowWidth) == (frameHeight / windowHeight) && "Frame and window scale should be the same on both axis!");
    SRC->ScissorScale = frameWidth / windowWidth;
    SRC->WindowHeight = windowHeight;
  }

  R_API void srEndFrame()
  {
    srDrawRenderBatch(&SRC->MainRenderBatch);
  }

  R_API void srClear(int mask)
  {
    glCall(glClear(mask));
  }

  R_API void srClearColor(float r, float g, float b, float a)
  {
    glCall(glClearColor(r, g, b, a));
  }

  R_API void srViewport(float x, float y, float width, float height)
  {
    glCall(glViewport(x, y, width, height));
  }

  R_API void srSetPolygonFillMode(PolygonFillMode_ mode)
  {
    switch (mode)
    {
    case PolygonFillMode_Fill:
      glCall(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL));
      break;
    case PolygonFillMode_Line:
      glCall(glPolygonMode(GL_FRONT_AND_BACK, GL_LINE));
      break;
    }
  }

  R_API Color srGetColorFromFloat(float r, float g, float b, float a)
  {
    // Clip values between 0 and 1
    r = srClamp(r, 0.0f, 1.0f);
    g = srClamp(g, 0.0f, 1.0f);
    b = srClamp(b, 0.0f, 1.0f);
    a = srClamp(a, 0.0f, 1.0f);

    return (((unsigned char)(a * 255)) << 24) | (((unsigned char)(b * 255)) << 16) | (((unsigned char)(g * 255)) << 8) | ((unsigned char)(r * 255));
  }

  R_API Color srGetColorFromFloat(const glm::vec4 &color)
  {
    return srGetColorFromFloat(color.x, color.y, color.z, color.w);
  }

  R_API glm::vec4 srGetFloatFromColor(Color c)
  {
    float r = (c & 0x000000ff) >> 0;
    float g = (c & 0x0000ff) >> 8;
    float b = (c & 0x00ff) >> 16;
    float a = (c & 0xff) >> 24;
    return {r, g, b, a};
  }

  // MATH
  R_API RectangleCorners srGetRotatedRectangle(const Rectangle &rect, float rotation)
  {
    RectangleCorners result;

    float sin = rotation != 0 ? sinf(rotation * DEG2RAD) : 0;
    float cos = rotation != 0 ? cosf(rotation * DEG2RAD) : 1;

    float dx = -rect.OriginX;
    float dy = -rect.OriginY;

    result.TopLeft.x = dx * cos - (dy + rect.Height) * sin;
    result.TopLeft.y = dx * sin + (dy + rect.Height) * cos;

    result.TopRight.x = (dx + rect.Width) * cos - (dy + rect.Height) * sin;
    result.TopRight.y = (dx + rect.Width) * sin + (dy + rect.Height) * cos;

    result.BottomLeft.x = dx * cos - dy * sin;
    result.BottomLeft.y = dx * sin + dy * cos;

    result.BottomRight.x = (dx + rect.Width) * cos - dy * sin;
    result.BottomRight.y = (dx + rect.Width) * sin + dy * cos;

    return result;
  }

  R_API Shader srLoadShader(const char *vertSrc, const char *fragSrc)
  {
    int prog_id = glCall(glCreateProgram());

    int vert_id = srCompileShader(GL_VERTEX_SHADER, vertSrc);
    int frag_id = srCompileShader(GL_FRAGMENT_SHADER, fragSrc);

    glCall(glAttachShader(prog_id, vert_id));
    glCall(glAttachShader(prog_id, frag_id));

    glCall(glLinkProgram(prog_id));

    // NOTE: All uniform variables are intitialised to 0 when a program links

    int error;
    glCall(glGetProgramiv(prog_id, GL_LINK_STATUS, &error));

    if (error == 0)
    {
      SR_TRACE("SHADER: [ID %i] Failed to link shader program", prog_id);

      int maxLength = 0;
      glCall(glGetProgramiv(prog_id, GL_INFO_LOG_LENGTH, &maxLength));

      if (maxLength > 0)
      {
        int length = 0;
        char *log = (char *)malloc(maxLength * sizeof(char));
        glCall(glGetProgramInfoLog(prog_id, maxLength, &length, log));
        SR_TRACE("SHADER: [ID %i] Link error: %s", prog_id, log);
        free(log);
      }

      glCall(glDeleteProgram(prog_id));

      prog_id = 0;
    }

    if (prog_id == 0)
    {
      // In case shader loading fails, we return the default shader
      SR_TRACE("SHADER: Failed to load custom shader code.");
    }
    /*
    else
    {
        // Get available shader uniforms
        // NOTE: This information is useful for debug...
        int uniformCount = -1;
        glGetProgramiv(id, GL_ACTIVE_UNIFORMS, &uniformCount);

        for (int i = 0; i < uniformCount; i++)
        {
            int namelen = -1;
            int num = -1;
            char name[256] = { 0 };     // Assume no variable names longer than 256
            GLenum type = GL_ZERO;

            // Get the name of the uniforms
            glGetActiveUniform(id, i, sizeof(name) - 1, &namelen, &num, &type, name);

            name[namelen] = 0;
            TRACELOGD("SHADER: [ID %i] Active uniform (%s) set at location: %i", id, name, glGetUniformLocation(id, name));
        }
    }*/

    srDeleteShader(GL_VERTEX_SHADER, vert_id);
    srDeleteShader(GL_FRAGMENT_SHADER, frag_id);

    Shader result = {0};
    result.ID = prog_id;
    result.UniformLocations = new int[(size_t)EUniformLocation::UNIFORM_MAX_SIZE];
    return result;
  }

  R_API int srCompileShader(int shader_type, const char *shader_source)
  {
    int result = glCall(glCreateShader(shader_type));
    glCall(glShaderSource(result, 1, &shader_source, NULL));

    glCall(glCompileShader(result));
    GLint error = 0;
    glCall(glGetShaderiv(result, GL_COMPILE_STATUS, &error));
    if (error == 0)
    {
      switch (shader_type)
      {
      case GL_VERTEX_SHADER:
        SR_TRACE("SHADER: Failed Compiling Vertex Shader [ID %i]", result);
        break;
      case GL_FRAGMENT_SHADER:
        SR_TRACE("SHADER: Failed Compiling Fragment Shader [ID %i]", result);
        break;
      case GL_GEOMETRY_SHADER:
        SR_TRACE("SHADER: Failed Compiling Geometry Shader [ID %i]", result);
        break;

      default:
        break;
      }

      int maxLength = 0;
      glCall(glGetShaderiv(result, GL_INFO_LOG_LENGTH, &maxLength));

      if (maxLength > 0)
      {
        int length = 0;
        char *log = (char *)malloc(maxLength * sizeof(char));
        glCall(glGetShaderInfoLog(result, maxLength, &length, log));
        SR_TRACE("SHADER: [ID %i] Compile error: %s", result, log);
        free(log);
      }
    }
    else
    {
      switch (shader_type)
      {
      case GL_VERTEX_SHADER:
        SR_TRACE("SHADER: Successfully compiled Vertex Shader [ID %i]", result);
        break;
      case GL_FRAGMENT_SHADER:
        SR_TRACE("SHADER: Successfully compiled Fragment Shader [ID %i]", result);
        break;
      case GL_GEOMETRY_SHADER:
        SR_TRACE("SHADER: Successfully compiled Geometry Shader [ID %i]", result);
        break;
      default:
        break;
      }
    }

    return result;
  }

  R_API void srDeleteShader(int shader_type, unsigned int id)
  {
    glCall(glDeleteShader(id));
  }

  R_API void srUseShader(Shader shader)
  {
    glCall(glUseProgram(shader.ID));
  }

  R_API unsigned int srShaderGetUniformLocation(const char *name, Shader shader, bool show_err)
  {
    unsigned int result = glCall(glGetUniformLocation(shader.ID, name));
    if (result == -1 && show_err)
    {
      SR_TRACE("Could not find uniform location %s [ID %i]", name, shader.ID);
    }
    return result;
  }

  R_API void srShaderSetUniform1b(Shader shader, const char *name, bool value)
  {
    unsigned int location = srShaderGetUniformLocation(name, shader);
    if (location != -1)
    {
      glCall(glUniform1i(location, value));
    }
  }

  R_API void srShaderSetUniform1i(Shader shader, const char *name, int value)
  {
    unsigned int location = srShaderGetUniformLocation(name, shader);
    if (location != -1)
    {
      glCall(glUniform1i(location, value));
    }
  }

  R_API void srShaderSetUniform1f(Shader shader, const char *name, float value)
  {
    unsigned int location = srShaderGetUniformLocation(name, shader);
    if (location != -1)
    {
      glCall(glUniform1f(location, value));
    }
  }

  R_API void srShaderSetUniform2f(Shader shader, const char *name, const glm::vec2 &value)
  {
    unsigned int location = srShaderGetUniformLocation(name, shader);
    if (location != -1)
    {
      glCall(glUniform2f(location, value.x, value.y));
    }
  }

  R_API void srShaderSetUniform3f(Shader shader, const char *name, const glm::vec3 &value)
  {
    unsigned int location = srShaderGetUniformLocation(name, shader);
    if (location != -1)
    {
      glCall(glUniform3f(location, value.x, value.y, value.z));
    }
  }

  R_API void srShaderSetUniformMat4(Shader shader, const char *name, const glm::mat4 &value)
  {
    unsigned int location = srShaderGetUniformLocation(name, shader);
    if (location != -1)
    {
      glCall(glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value)));
    }
  }

  R_API void srSetDefaultShaderUniforms(Shader shader)
  {
    srUseShader(shader);

    srShaderSetUniformMat4(shader, "ProjectionMatrix", SRC->CurrentProjection);
    srShaderSetUniform1i(shader, "Texture", 0);
  }

  R_API unsigned int srTextureFormatToGL(TextureFormat_ format)
  {
    switch (format)
    {
    case TextureFormat_R8:
      return GL_RED;
    case TextureFormat_RGB8:
      return GL_RGB;
    case TextureFormat_RGBA8:
      return GL_RGBA;
    }
    return 0;
  }

  R_API Texture srLoadTexture(unsigned int width, unsigned int height, TextureFormat_ format)
  {
    Texture result;
    result.ID = 0;
    glCall(glGenTextures(1, &result.ID));
    glCall(glBindTexture(GL_TEXTURE_2D, result.ID));

    glCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER));
    glCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER));
    glCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
    glCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

    glCall(glBindTexture(GL_TEXTURE_2D, 0));

    const size_t bytePerPixel = srTextureFormatSize(format);

    const size_t size = width * height * bytePerPixel;

    unsigned char *buffer = new unsigned char[size];
    memset(buffer, 0, size);

    srTextureSetData(result, width, height, format, buffer);
    delete[] buffer;
    return result;
  }

  R_API Texture srLoadTextureFromFile(const char *path)
  {
    int width = 0;
    int height = 0;
    int comp = 0;
    unsigned char *data = stbi_load(path, &width, &height, &comp, 3);

    if (data == NULL)
    {
      SR_TRACE("Cannot load texture\"%s\"", path);
      return Texture{};
    }
    SR_TRACE("Loading texture \"%s\". W=%d H=%d C=%d", path, width, height, comp);

    TextureFormat_ format = TextureFormat_RGBA8;
    if (comp == 1)
    {
      format = TextureFormat_R8;
    }
    else if (comp == 3)
    {
      format = TextureFormat_RGB8;
    }
    Texture result = srLoadTexture(width, height, format);

    srTextureSetData(result, width, height, format, data);

    stbi_image_free(data);
    return result;
  }

  R_API void srUnloadTexture(Texture *texture)
  {
    if (texture->ID != 0)
    {
      glCall(glDeleteTextures(1, &texture->ID));
      texture->ID = 0;
    }
  }

  R_API void srBindTexture(Texture texture)
  {
    glCall(glBindTexture(GL_TEXTURE_2D, texture.ID));
  }

  R_API void srTextureSetData(Texture texture, unsigned int width, unsigned int height, TextureFormat_ format, unsigned char *data)
  {
    srBindTexture(texture);
    glCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT));
    glCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT));
    glCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR));
    glCall(glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR));

    GLint swizzleMask[] = {GL_RED, GL_RED, GL_RED, GL_RED};
    glCall(glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask));

    glCall(glPixelStorei(GL_UNPACK_ALIGNMENT, 1));

    glCall(glTexImage2D(GL_TEXTURE_2D, 0, srTextureFormatToGL(format), width, height, 0, srTextureFormatToGL(format), GL_UNSIGNED_BYTE, data));
    glCall(glGenerateMipmap(GL_TEXTURE_2D));

    srBindTexture({0});
  }

  R_API void srTexturePrintData(Texture texutre, const unsigned int width, const unsigned int height, TextureFormat_ format)
  {
    const size_t bytePerPixel = srTextureFormatSize(format);
    const size_t size = width * height * bytePerPixel;

    unsigned char *buffer = new unsigned char[size];
    glGetTexImage(GL_TEXTURE_2D, 0, srTextureFormatToGL(format), GL_UNSIGNED_BYTE, buffer);

    printf("Texture data\n");
    for (unsigned int y = 0; y < height; y++)
    {
      for (unsigned int x = 0; x < width; x++)
      {
        printf("0x%08x, ", buffer[(x + y * height) * bytePerPixel]);
      }
      printf("\n");
    }

    delete[] buffer;
  }

  R_API void srPushMaterial(const Material &mat)
  {
    Texture currentDrawTexture = SRC->MainRenderBatch.DrawCalls[SRC->MainRenderBatch.CurrentDraw].Mat.Texture0;
    if (currentDrawTexture.ID != 0 && currentDrawTexture.ID != mat.Texture0.ID)
    {
      // We push a texture where we already have one
      // We override the last pushed for now. This should not happen
      // We warn here for now
    }
    SRC->MainRenderBatch.DrawCalls[SRC->MainRenderBatch.CurrentDraw].Mat = mat;
  }

  // Vertex Arrays

  R_API unsigned int srGetVertexAttributeComponentCount(EVertexAttributeType type)
  {
    switch (type)
    {
    case EVertexAttributeType::INT:
    case EVertexAttributeType::UINT:
    case EVertexAttributeType::BOOL:
    case EVertexAttributeType::FLOAT:
      return 1;
    case EVertexAttributeType::FLOAT2:
    case EVertexAttributeType::INT2:
      return 2;
    case EVertexAttributeType::FLOAT3:
    case EVertexAttributeType::INT3:
      return 3;
    case EVertexAttributeType::FLOAT4:
    case EVertexAttributeType::INT4:
    case EVertexAttributeType::BYTE4:
      return 4;
    }
    SR_TRACE("ERROR: Could not get vertex attrib type component count");

    return 0;
  }

  R_API unsigned int srGetVertexAttributeTypeSize(EVertexAttributeType type)
  {
    switch (type)
    {
    case EVertexAttributeType::INT:
      return sizeof(int);
    case EVertexAttributeType::UINT:
      return sizeof(unsigned int);
    case EVertexAttributeType::BOOL:
      return sizeof(bool);
    case EVertexAttributeType::FLOAT:
      return sizeof(float);
    case EVertexAttributeType::FLOAT2:
      return sizeof(float) * 2;
    case EVertexAttributeType::INT2:
      return sizeof(int) * 2;
    case EVertexAttributeType::FLOAT3:
      return sizeof(float) * 3;
    case EVertexAttributeType::INT3:
      return sizeof(int) * 3;
    case EVertexAttributeType::FLOAT4:
      return sizeof(float) * 4;
    case EVertexAttributeType::INT4:
      return sizeof(int) * 4;
    case EVertexAttributeType::BYTE4:
      return sizeof(unsigned char) * 4;
    }
    SR_TRACE("ERROR: Could not get vertex attrib type component count");

    return 0;
  }

  R_API unsigned int srGetGLVertexAttribType(EVertexAttributeType type)
  {
    switch (type)
    {

    case EVertexAttributeType::FLOAT:
    case EVertexAttributeType::FLOAT2:
    case EVertexAttributeType::FLOAT3:
    case EVertexAttributeType::FLOAT4:
      return GL_FLOAT;
    case EVertexAttributeType::INT:
    case EVertexAttributeType::INT2:
    case EVertexAttributeType::INT3:
    case EVertexAttributeType::INT4:
      return GL_INT;
    case EVertexAttributeType::UINT:
      return GL_UNSIGNED_INT;
    case EVertexAttributeType::BOOL:
      return GL_BOOL;
    case EVertexAttributeType::BYTE4:
      return GL_UNSIGNED_BYTE;
    }
    SR_TRACE("ERROR: Could not convert vertex attrib type");
    return 0;
  }

  R_API size_t srGetVertexLayoutSize(const VertexArrayLayout &layout)
  {
    size_t size = 0;
    for (const VertexArrayLayoutElement &elem : layout)
    {
      size += srGetVertexAttributeTypeSize(elem.ElementType);
    }
    return size;
  }

  // TODO: VAO check support. (If done remove TODO in header file)
  R_API unsigned int srLoadVertexArray()
  {
    unsigned int result = 0;
    glCall(glGenVertexArrays(1, &result));
    return result;
  }

  R_API void srUnloadVertexArray(unsigned int id)
  {
    if (id)
    {
      glCall(glDeleteVertexArrays(1, &id));
    }
  }

  R_API bool srBindVertexArray(unsigned int id)
  {
    bool result = false;
    // TODO: Vertex arrays not supported
    glCall(glBindVertexArray(id));
    result = true;
    return result;
  }

  R_API void srSetVertexAttribute(unsigned int location, unsigned int numElements, unsigned int type, bool normalized, int stride, const void *pointer)
  {
    glVertexAttribPointer(location, numElements, type, normalized, stride, pointer);
  }

  R_API void srEnableVertexAttribute(unsigned int location)
  {
    glEnableVertexAttribArray(location);
  }

  R_API Mesh srLoadMesh(const MeshInit &initData)
  {
    Mesh result{};
    if (initData.Vertices.size() == 0)
    {
      SR_TRACE("No Vertices provided for LoadingMesh");
      return result;
    }
    assert(!(initData.Vertices.size() == (initData.Normals.size() == 0 ? initData.Vertices.size() : initData.Normals.size()) == (initData.TexCoord1.size() == 0 ? initData.Vertices.size() : initData.TexCoord1.size())));

    result.VertexCount = initData.Vertices.size();
    result.ElementCount = initData.Indices.size();

    result.Vertices = new glm::vec3[result.VertexCount];
    memcpy(result.Vertices, initData.Vertices.data(), result.VertexCount * sizeof(glm::vec3));

    if (initData.Normals.size() > 0)
    {
      result.Normals = new glm::vec3[result.VertexCount];
      memcpy(result.Normals, initData.Normals.data(), result.VertexCount * sizeof(glm::vec3));
    }

    if (initData.TexCoord1.size() > 0)
    {
      result.TextureCoords0 = new glm::vec2[result.VertexCount];
      memcpy(result.TextureCoords0, initData.TexCoord1.data(), result.VertexCount * sizeof(glm::vec2));
    }

    if (initData.Colors.size() > 0)
    {
      result.Colors = new Color[result.VertexCount];
      memcpy(result.Colors, initData.Colors.data(), result.VertexCount * sizeof(Color));
    }

    if (result.ElementCount > 0)
    {
      result.Indices = new unsigned int[result.ElementCount];
      memcpy(result.Indices, initData.Indices.data(), result.ElementCount * sizeof(unsigned int));
    }

    srUploadMesh(&result);
    srDeleteMeshCPUData(&result); // Delete mesh data. We only want to delete the openglbuffers

    SRC->AutoReleaseMeshes.push_back(result); // Push to autorelease on cleanup
    return result;
  }

  R_API unsigned int srLoadVertexBuffer(void *data, size_t data_size)
  {
    unsigned int result = 0;
    glCall(glGenBuffers(1, &result));
    glCall(glBindBuffer(GL_ARRAY_BUFFER, result));

    glCall(glBufferData(GL_ARRAY_BUFFER, data_size, data, GL_STATIC_DRAW));
    return result;
  }

  R_API unsigned int srLoadElementBuffer(void *data, size_t data_size)
  {
    unsigned int result = 0;
    glCall(glGenBuffers(1, &result));
    glCall(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, result));

    glCall(glBufferData(GL_ELEMENT_ARRAY_BUFFER, data_size, data, GL_STATIC_DRAW));
    return result;
  }

  R_API void srUnloadBuffer(unsigned int id)
  {
    if (id)
    {
      glCall(glDeleteBuffers(1, &id));
    }
  }

  R_API void srBindVertexBuffer(unsigned int id)
  {
    glCall(glBindBuffer(GL_ARRAY_BUFFER, id));
  }

  R_API void srBindElementBuffer(unsigned int id)
  {
    glCall(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, id));
  }

  R_API void srDrawMesh(const Mesh &mesh)
  {
    if (mesh.VAO.VAO <= 0)
    {
      SR_TRACE("ERROR: DrawMesh failed. VertexArray not initialized!");
      return;
    }
    srSetDefaultShaderUniforms(SRC->DefaultShader);

    if (!srBindVertexArray(mesh.VAO.VAO))
    {
      for (const auto &buffer : mesh.VAO.VBOs)
      {
        srBindVertexBuffer(buffer.ID);

        unsigned int vertexSize = srGetVertexLayoutSize(buffer.Layout);
        unsigned int currentOffset = 0;
        for (const VertexArrayLayoutElement &elem : buffer.Layout)
        {
          srSetVertexAttribute(elem.Location, srGetVertexAttributeComponentCount(elem.ElementType), srGetGLVertexAttribType(elem.ElementType), elem.Normalized, vertexSize, (const void *)(unsigned long long)currentOffset);
          srEnableVertexAttribute(elem.Location);
          currentOffset += srGetVertexAttributeTypeSize(elem.ElementType);
        }
      }
    }

    if (mesh.VAO.IBO != 0)
    {
      // srBindElementBuffer(mesh.VAO.IBO); // In case mac does not work use this here
      glCall(glDrawElements(GL_TRIANGLES, mesh.ElementCount, GL_UNSIGNED_INT, NULL));
    }
    else
    {
      glCall(glDrawArrays(GL_TRIANGLES, 0, mesh.VertexCount));
    }
  }

  R_API void srUploadMesh(Mesh *mesh)
  {
    if (mesh->VAO.VAO > 0)
    {
      // Mesh allready loaded to GPU
      return;
    }
    VertexBuffers vertexArray;
    vertexArray.VAO = srLoadVertexArray();
    srBindVertexArray(vertexArray.VAO);

    unsigned int vbo_position = srLoadVertexBuffer(mesh->Vertices, mesh->VertexCount * sizeof(glm::vec3));
    srEnableVertexAttribute(0);
    srSetVertexAttribute(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    vertexArray.VBOs.push_back({{VertexArrayLayoutElement(EVertexAttributeType::FLOAT3, 0)}, vbo_position});

    if (mesh->Normals)
    {
      unsigned int vbo_normal = srLoadVertexBuffer(mesh->Normals, mesh->VertexCount * sizeof(glm::vec3));
      srEnableVertexAttribute(1);
      srSetVertexAttribute(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
      vertexArray.VBOs.push_back({{VertexArrayLayoutElement(EVertexAttributeType::FLOAT3, 1)}, vbo_normal});
    }

    if (mesh->TextureCoords0)
    {
      unsigned int vbo_textCoords = srLoadVertexBuffer(mesh->TextureCoords0, mesh->VertexCount * sizeof(glm::vec2));
      srEnableVertexAttribute(2);
      srSetVertexAttribute(2, 2, GL_FLOAT, GL_FALSE, 0, 0);
      vertexArray.VBOs.push_back({{VertexArrayLayoutElement(EVertexAttributeType::FLOAT2, 2)}, vbo_textCoords});
    }

    if (mesh->Colors)
    {
      unsigned int vbo_colors = srLoadVertexBuffer(mesh->Colors, mesh->VertexCount * sizeof(Color));
      srEnableVertexAttribute(3);
      srSetVertexAttribute(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, 0, 0);
      vertexArray.VBOs.push_back({{VertexArrayLayoutElement(EVertexAttributeType::BYTE4, 3, true)}, vbo_colors});
    }

    if (mesh->Indices)
    {
      unsigned int ibo = srLoadElementBuffer(mesh->Indices, mesh->ElementCount * sizeof(unsigned int));
      vertexArray.IBO = ibo;
    }

    srBindVertexArray(0);
    mesh->VAO = vertexArray;
  }

  R_API void srUnloadMesh(Mesh *mesh)
  {
    srUnloadVertexBuffers(mesh->VAO);
    srDeleteMeshCPUData(mesh);
  }

  R_API void srDeleteMeshCPUData(Mesh *mesh)
  {
    if (mesh->Vertices)
    {
      delete[] mesh->Vertices;
      mesh->Vertices = NULL;
    }
    if (mesh->Normals)
    {
      delete[] mesh->Normals;
      mesh->Normals = NULL;
    }
    if (mesh->TextureCoords0)
    {
      delete[] mesh->TextureCoords0;
      mesh->TextureCoords0 = NULL;
    }
    if (mesh->Indices)
    {
      delete[] mesh->Indices;
      mesh->Indices = NULL;
    }
    if (mesh->Colors)
    {
      delete[] mesh->Colors;
      mesh->Colors = NULL;
    }
  }

  R_API void srUnloadVertexBuffers(const VertexBuffers &vao)
  {
    // Delete Vertex buffers
    for (VertexBuffers::VertBuf buff : vao.VBOs)
    {
      srUnloadBuffer(buff.ID);
    }

    // Delete Index Buffer
    srUnloadBuffer(vao.IBO);

    // Delete VertexArray
    srUnloadVertexArray(vao.VAO);
  }

  R_API RenderBatch srLoadRenderBatch(unsigned int bufferSize)
  {
    RenderBatch result;
    result.CurrentDraw = 0;
    result.DrawCalls = new RenderBatch::DrawCall[SR_BATCH_DRAW_CALLS];
    result.VertexCounter = 0;

    result.DrawBuffer.Vertices = new RenderBatch::Vertex[bufferSize * 4];
    memset(result.DrawBuffer.Vertices, 0, bufferSize * 4 * sizeof(RenderBatch::Vertex));
    result.DrawBuffer.Indices = new unsigned int[bufferSize * 6];
    memset(result.DrawBuffer.Indices, 0, bufferSize * 4 * sizeof(unsigned int));
    result.DrawBuffer.ElementCount = bufferSize * 4;

    int k = 0;

    // Indices can be initialized right now
    for (int j = 0; j < (6 * bufferSize); j += 6)
    {
      result.DrawBuffer.Indices[j] = 4 * k;
      result.DrawBuffer.Indices[j + 1] = 4 * k + 1;
      result.DrawBuffer.Indices[j + 2] = 4 * k + 2;
      result.DrawBuffer.Indices[j + 3] = 4 * k;
      result.DrawBuffer.Indices[j + 4] = 4 * k + 2;
      result.DrawBuffer.Indices[j + 5] = 4 * k + 3;

      k++;
    }

    VertexBuffers glBinding;
    glBinding.VAO = srLoadVertexArray();
    srBindVertexArray(glBinding.VAO);

    unsigned int vbo = srLoadVertexBuffer(result.DrawBuffer.Vertices, bufferSize * 4 * sizeof(RenderBatch::Vertex));
    srEnableVertexAttribute(0);
    srSetVertexAttribute(0, 3, GL_FLOAT, GL_FALSE, sizeof(RenderBatch::Vertex), 0);
    srEnableVertexAttribute(1);
    srSetVertexAttribute(1, 3, GL_FLOAT, GL_FALSE, sizeof(RenderBatch::Vertex), (const void *)offsetof(RenderBatch::Vertex, Normal));
    srEnableVertexAttribute(2);
    srSetVertexAttribute(2, 2, GL_FLOAT, GL_FALSE, sizeof(RenderBatch::Vertex), (const void *)offsetof(RenderBatch::Vertex, UV));
    srEnableVertexAttribute(3);
    srSetVertexAttribute(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(RenderBatch::Vertex), (const void *)offsetof(RenderBatch::Vertex, Color1));
    srEnableVertexAttribute(4);
    srSetVertexAttribute(4, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(RenderBatch::Vertex), (const void *)offsetof(RenderBatch::Vertex, Color2));
    glBinding.VBOs.push_back({{VertexArrayLayoutElement(EVertexAttributeType::FLOAT3, 0),
                               VertexArrayLayoutElement(EVertexAttributeType::FLOAT3, 1),
                               VertexArrayLayoutElement(EVertexAttributeType::FLOAT2, 2),
                               VertexArrayLayoutElement(EVertexAttributeType::BYTE4, 3),
                               VertexArrayLayoutElement(EVertexAttributeType::BYTE4, 4)},
                              vbo});

    unsigned int ibo = srLoadElementBuffer(result.DrawBuffer.Indices, bufferSize * 6 * sizeof(unsigned int));
    glBinding.IBO = ibo;

    srBindVertexArray(0);

    result.DrawBuffer.GlBinding = glBinding;
    return result;
  }

  R_API void srIncreaseRenderBatchCurrentDraw(RenderBatch *batch)
  {
    if (batch->CurrentDraw++ >= SR_BATCH_DRAW_CALLS)
    {
      srDrawRenderBatch(batch);
    }
  }

  R_API bool srCheckRenderBatchLimit(unsigned int numVerts)
  {
    bool overflow = false;
    RenderBatch &rb = SRC->MainRenderBatch;

    if (rb.VertexCounter + numVerts >= rb.DrawBuffer.ElementCount)
    {
      EBatchDrawMode currentMode = rb.DrawCalls[rb.CurrentDraw].Mode;

      srDrawRenderBatch(&rb);

      rb.DrawCalls[rb.CurrentDraw].Mode = currentMode;
      overflow = true;
    }
    return overflow;
  }

  R_API void srDrawRenderBatch(RenderBatch *batch)
  {
    if (!srBindVertexArray(batch->DrawBuffer.GlBinding.VAO))
    {
      for (const auto &buffer : batch->DrawBuffer.GlBinding.VBOs)
      {
        srBindVertexBuffer(buffer.ID);
        unsigned int vertexSize = srGetVertexLayoutSize(buffer.Layout);
        unsigned int currentOffset = 0;
        for (const VertexArrayLayoutElement &elem : buffer.Layout)
        {
          srSetVertexAttribute(elem.Location, srGetVertexAttributeComponentCount(elem.ElementType), srGetGLVertexAttribType(elem.ElementType), elem.Normalized, vertexSize, (const void *)(unsigned long long)currentOffset);
          srEnableVertexAttribute(elem.Location);
          currentOffset += srGetVertexAttributeTypeSize(elem.ElementType);
        }
      }
    }

    srBindVertexBuffer(batch->DrawBuffer.GlBinding.VBOs[0].ID);
    glCall(glBufferSubData(GL_ARRAY_BUFFER, 0, batch->VertexCounter * sizeof(RenderBatch::Vertex), batch->DrawBuffer.Vertices));

    // Draw everything to current draw
    for (unsigned int i = 0, vertexOffset = 0; i <= batch->CurrentDraw; i++)
    {
      RenderBatch::DrawCall &drawCall = batch->DrawCalls[i];
      Shader shader = SRC->DefaultShader;
      if (drawCall.Mat.ShaderProgram.ID != 0)
      {
        shader = drawCall.Mat.ShaderProgram;
      }

      if (!drawCall.Scissor.Enabled)
      {
        glDisable(GL_SCISSOR_TEST);
      }
      else
      {
        glEnable(GL_SCISSOR_TEST);
        glScissor(drawCall.Scissor.X, drawCall.Scissor.Y, drawCall.Scissor.Width, drawCall.Scissor.Height);
      }

      srSetDefaultShaderUniforms(shader);
      if (srShaderGetUniformLocation("UseTexture", shader, false) != -1)
      {
        srShaderSetUniform1b(shader, "UseTexture", drawCall.Mat.Texture0.ID > 0);
      }

      glCall(glActiveTexture(GL_TEXTURE0));
      srBindTexture(drawCall.Mat.Texture0);

      EBatchDrawMode mode = drawCall.Mode;
      switch (mode)
      {
      case EBatchDrawMode::POINTS:
        glCall(glDrawArrays(GL_POINTS, vertexOffset, drawCall.VertexCount));
        break;
      case EBatchDrawMode::LINES:
        glCall(glDrawArrays(GL_LINES, vertexOffset, drawCall.VertexCount));
        break;
      case EBatchDrawMode::TRIANGLES:
        glCall(glDrawArrays(GL_TRIANGLES, vertexOffset, drawCall.VertexCount));
        break;
      case EBatchDrawMode::QUADS:
        glCall(glDrawElements(GL_TRIANGLES, drawCall.VertexCount / 4 * 6, GL_UNSIGNED_INT, (GLvoid *)(vertexOffset / 4 * 6 * sizeof(unsigned int))));
        break;
      case EBatchDrawMode::UNKNOWN:
        break;
      }
      srBindTexture({});
      vertexOffset += drawCall.VertexCount + drawCall.VertexAlignment;
    }

    batch->CurrentDraw = 0;
    batch->DrawCalls[0] = RenderBatch::DrawCall{};
    batch->VertexCounter = 0;
  }

  R_API void srEnableScissor(float x, float y, float width, float height)
  {
    SRC->Scissor.Enabled = true;

    // Calculate Ortho coords to window coords
    float scissor_y = (y + height); // Go to bottom
    scissor_y = SRC->WindowHeight - scissor_y;

    SRC->Scissor.X = x * SRC->ScissorScale;
    SRC->Scissor.Y = scissor_y * SRC->ScissorScale;
    SRC->Scissor.Width = width * SRC->ScissorScale;
    SRC->Scissor.Height = height * SRC->ScissorScale;
  }

  R_API void srDisableScissor()
  {
    SRC->Scissor.Enabled = false;
  }

  R_API void srBegin(EBatchDrawMode mode)
  {
    RenderBatch &rb = SRC->MainRenderBatch;
    if (rb.DrawCalls[rb.CurrentDraw].Mode != mode || rb.DrawCalls[rb.CurrentDraw].Scissor != SRC->Scissor)
    {
      if (rb.DrawCalls[rb.CurrentDraw].Mode == EBatchDrawMode::LINES || rb.DrawCalls[rb.CurrentDraw].Mode == EBatchDrawMode::POINTS)
        rb.DrawCalls[rb.CurrentDraw].VertexAlignment = rb.DrawCalls[rb.CurrentDraw].VertexCount % 4;
      else if (rb.DrawCalls[rb.CurrentDraw].Mode == EBatchDrawMode::TRIANGLES)
        rb.DrawCalls[rb.CurrentDraw].VertexAlignment = 4 - (rb.DrawCalls[rb.CurrentDraw].VertexCount % 4);
      else
        rb.DrawCalls[rb.CurrentDraw].VertexAlignment = 0;

      rb.VertexCounter += rb.DrawCalls[rb.CurrentDraw].VertexAlignment; // Offset vertex counter so it is all nice

      srIncreaseRenderBatchCurrentDraw(&rb);
      rb.DrawCalls[rb.CurrentDraw].Mode = mode;
      rb.DrawCalls[rb.CurrentDraw].VertexCount = 0;
      rb.DrawCalls[rb.CurrentDraw].VertexAlignment = 0;
      rb.DrawCalls[rb.CurrentDraw].Mat = {0};
      rb.DrawCalls[rb.CurrentDraw].Scissor = SRC->Scissor;

      rb.CurrentColor1 = 0xffffffff;
      rb.CurrentColor2 = 0x00000000;
      rb.CurrentNormal = glm::vec3(0.0f, 0.0f, -1.0f);
      rb.CurrentTexCoord = glm::vec2(0.0f);
    }
  }

  R_API void srVertex3f(float x, float y, float z)
  {
    srVertex3f({x, y, z});
  }

  R_API void srVertex3f(const glm::vec3 &vertex)
  {
    srCheckRenderBatchLimit(1);

    SRC->MainRenderBatch.DrawBuffer.Vertices[SRC->MainRenderBatch.VertexCounter].Pos = vertex;
    SRC->MainRenderBatch.DrawBuffer.Vertices[SRC->MainRenderBatch.VertexCounter].UV = SRC->MainRenderBatch.CurrentTexCoord;
    SRC->MainRenderBatch.DrawBuffer.Vertices[SRC->MainRenderBatch.VertexCounter].Normal = SRC->MainRenderBatch.CurrentNormal;
    SRC->MainRenderBatch.DrawBuffer.Vertices[SRC->MainRenderBatch.VertexCounter].Color1 = SRC->MainRenderBatch.CurrentColor1;
    SRC->MainRenderBatch.DrawBuffer.Vertices[SRC->MainRenderBatch.VertexCounter].Color2 = SRC->MainRenderBatch.CurrentColor2;

    SRC->MainRenderBatch.VertexCounter++;
    SRC->MainRenderBatch.DrawCalls[SRC->MainRenderBatch.CurrentDraw].VertexCount++;
  }

  R_API void srVertex2f(float x, float y)
  {
    srVertex3f(glm::vec3(x, y, SRC->MainRenderBatch.CurrentDepth));
  }

  R_API void srVertex2f(const glm::vec2 &vertex)
  {
    srVertex3f(glm::vec3(vertex.x, vertex.y, SRC->MainRenderBatch.CurrentDepth));
  }

  R_API void srNormal3f(float x, float y, float z)
  {
    srNormal3f({x, y, z});
  }

  R_API void srNormal3f(const glm::vec3 &normal)
  {
    SRC->MainRenderBatch.CurrentNormal = normal;
  }

  R_API void srColor13f(float r, float g, float b)
  {
    srColor14f(r, g, b, 1.0f);
  }

  R_API void srColor13f(const glm::vec3 &color)
  {
    srColor14f({color.x, color.y, color.z, 1.0f});
  }

  R_API void srColor14f(float r, float g, float b, float a)
  {
    SRC->MainRenderBatch.CurrentColor1 = srGetColorFromFloat(r, g, b, a);
  }

  R_API void srColor14f(const glm::vec4 &color)
  {
    SRC->MainRenderBatch.CurrentColor1 = srGetColorFromFloat(color);
  }

  R_API void srColor11c(Color color)
  {
    SRC->MainRenderBatch.CurrentColor1 = color;
  }

  R_API void srColor23f(float r, float g, float b)
  {
    srColor24f(r, g, b, 1.0f);
  }

  R_API void srColor23f(const glm::vec3 &color)
  {
    srColor24f({color.x, color.y, color.z, 1.0f});
  }

  R_API void srColor24f(float r, float g, float b, float a)
  {
    SRC->MainRenderBatch.CurrentColor2 = srGetColorFromFloat(r, g, b, a);
  }

  R_API void srColor24f(const glm::vec4 &color)
  {
    SRC->MainRenderBatch.CurrentColor2 = srGetColorFromFloat(color);
  }

  R_API void srColor21c(Color color)
  {
    SRC->MainRenderBatch.CurrentColor2 = color;
  }

  R_API void srTextureCoord2f(float u, float v)
  {
    SRC->MainRenderBatch.CurrentTexCoord = {u, v};
  }

  R_API void srTextureCoord2f(const glm::vec2 &uv)
  {
    SRC->MainRenderBatch.CurrentTexCoord = uv;
  }

  R_API void srEnd()
  {
    SRC->MainRenderBatch.CurrentDepth -= 0.0001f;
  }

  R_API void srDrawRectanglePro(const glm::vec2 &position, const Rectangle &rect, float rotation, float cornerRadius, PathType pathType, PathStyle style)
  {
    RectangleCorners corners = srGetRotatedRectangle(rect, rotation);
    corners += position;

    if (pathType & PathType_Stroke || cornerRadius >= 0.0f)
    {
      srBeginPath(pathType);
      srPathSetStyle(style);
      if (cornerRadius > 0.0f)
      {
        float sin = rotation != 0 ? sinf(rotation * DEG2RAD) : 0;
        float cos = rotation != 0 ? cosf(rotation * DEG2RAD) : 1;

        // Start with top left (right of arc) corner
        cornerRadius = srClamp(cornerRadius, 0.0f, 1.0f);
        const float min_size = (srMin(rect.Width, rect.Height) / 2.0f) * cornerRadius;
        const glm::vec2 arcCenterX = glm::vec2(min_size * cos, min_size * sin);
        const glm::vec2 arcCenterY = glm::vec2(min_size * -sin, min_size * cos);

        srPathArc(corners.TopLeft + arcCenterX - arcCenterY, -90.0f - rotation, -rotation, min_size);
        srPathArc(corners.TopRight - arcCenterX - arcCenterY, 0.0f - rotation, 90.0f - rotation, min_size);
        srPathArc(corners.BottomRight - arcCenterX + arcCenterY, 90.0f - rotation, 180.0f - rotation, min_size);
        srPathArc(corners.BottomLeft + arcCenterX + arcCenterY, 180.0f - rotation, 270.0f - rotation, min_size);
      }
      else
      {
        srPathLineTo(corners.TopLeft);
        srPathLineTo(corners.TopRight);
        srPathLineTo(corners.BottomRight);
        srPathLineTo(corners.BottomLeft);
      }
      srEndPath(true);
    }
    else
    {
      srBegin(EBatchDrawMode::QUADS);
      srColor11c(style.FillColor);

      srVertex2f(corners.TopLeft);
      srVertex2f(corners.TopRight);
      srVertex2f(corners.BottomRight);
      srVertex2f(corners.BottomLeft);

      srEnd();
    }
  }

  R_API void srDrawTexturePro(Texture texture, const glm::vec2 &position, const Rectangle &rect, float rotation)
  {
    RectangleCorners corners = srGetRotatedRectangle(rect, rotation) + position;

    sr::srBegin(EBatchDrawMode::QUADS);
    srPushMaterial({texture, SRC->DefaultShader});

    srTextureCoord2f(0.0f, 0.0f);
    srVertex2f(corners.TopLeft);

    srTextureCoord2f(1.0f, 0.0f);
    srVertex2f(corners.TopRight);

    srTextureCoord2f(1.0f, 1.0f);
    srVertex2f(corners.BottomRight);

    srTextureCoord2f(0.0f, 1.0f);
    srVertex2f(corners.BottomLeft);
    sr::srEnd();
  }

  R_API void srDrawGrid(const glm::vec2 &position, unsigned int columns, unsigned int rows, float cellSizeX, float cellSizeY)
  {
    srBegin(EBatchDrawMode::LINES);
    for (unsigned int y = 0; y < rows; y++)
    {
      srVertex2f(position + glm::vec2{0.0f, y * cellSizeY});
      srVertex2f(position + glm::vec2{columns * cellSizeX, y * cellSizeY});
    }
    for (unsigned int x = 0; x < columns; x++)
    {
      srVertex2f(position + glm::vec2{x * cellSizeX, 0.0f});
      srVertex2f(position + glm::vec2{x * cellSizeX, rows * cellSizeY});
    }
    srEnd();
  }

  void FontTextureInit(FontTexture *result)
  {
    result->Image = srLoadTexture(FONT_TEXTURE_SIZE, FONT_TEXTURE_SIZE, FONT_TEXTURE_DEPTH == 1 ? TextureFormat_R8 : TextureFormat_RGB8);
    // srTextureSetData(result->Texture, FONT_TEXTURE_SIZE, FONT_TEXTURE_SIZE, FONT_TEXTURE_DEPTH == 1 ? TextureFormat_R8 : TextureFormat_RGB8, (unsigned char *)0);
    result->Size = glm::ivec2(FONT_TEXTURE_SIZE);
    result->ImageData = new uint8_t[FONT_TEXTURE_SIZE * FONT_TEXTURE_SIZE * FONT_TEXTURE_DEPTH];
    result->ShelfPack = new mapbox::ShelfPack(FONT_TEXTURE_SIZE, FONT_TEXTURE_SIZE);
    memset(result->ImageData, 0, FONT_TEXTURE_SIZE * FONT_TEXTURE_SIZE * FONT_TEXTURE_DEPTH);
  }

  void FontTextureUnload(FontTexture *font_texture)
  {
    srUnloadTexture(&font_texture->Image);
    delete[] font_texture->ImageData;
    delete ((mapbox::ShelfPack *)font_texture->ShelfPack);
  }

  void FontTexturePushGlyph(FT_GlyphSlot glyph, FontTexture *result)
  {
    mapbox::ShelfPack &packer = *(mapbox::ShelfPack *)result->ShelfPack;
    mapbox::Bin *bin = packer.packOne(-1, glyph->bitmap.width, glyph->bitmap.rows);
    if (!bin)
    {
      SR_TRACE("FontTexturePushGlyph: Could not pack glyph into texture");
      return;
    }

    // Write bitmap data to buffer
    for (int y = 0; y < glyph->bitmap.rows; y++)
    {
      memcpy(result->ImageData + (bin->y + y) * result->Size.x + bin->x, glyph->bitmap.buffer + y * glyph->bitmap.width, glyph->bitmap.width);
    }

    float kerningx = 0.0f;
    float kerningy = 0.0f;

    FontGlyph res = {
        glm::ivec2(glyph->bitmap.width, glyph->bitmap.rows),             // size
        glm::ivec2(glyph->bitmap_left, glyph->bitmap_top),               // offset
        (int)(((float)glyph->advance.x) / 64.0f),                        // advance
        ((float)bin->x) / ((float)result->Size.x),                       // u0;
        ((float)bin->y) / ((float)result->Size.x),                       // v0;
        ((float)bin->x + glyph->bitmap.width) / ((float)result->Size.x), // u1;
        ((float)bin->y + glyph->bitmap.rows) / ((float)result->Size.x),  // v1;
        glyph->glyph_index                                               // Char code
    };

    result->CharMap[glyph->glyph_index] = res;
  }

  const FontGlyph *FontTextureGetGlyph(const Font *font, char c)
  {
    unsigned int char_code = FT_Get_Char_Index(font->Face, c);

    if (font->Texture.CharMap.find(char_code) != font->Texture.CharMap.end())
    {
      return &font->Texture.CharMap.at(char_code);
    }
    return nullptr;
  }

  void LoadGlyph(char c, Font *font)
  {
    unsigned int char_code = FT_Get_Char_Index(font->Face, c);
    FT_Load_Glyph(font->Face, char_code, FT_LOAD_DEFAULT);
    FT_Render_Glyph(font->Face->glyph, FT_RENDER_MODE_SDF);
    FontTexturePushGlyph(font->Face->glyph, &font->Texture);
  }

  void FontGetKerning(Font *font, unsigned int c1, unsigned int c2, int *x, int *y)
  {
    FT_Vector kerning{};
    FT_Get_Kerning(font->Face, c1, c2, FT_KERNING_DEFAULT, &kerning);
    *x = kerning.x >> 6;
    *y = kerning.y >> 6;
  }

  bool FontManagerHasFontLoaded(FontHandle handle)
  {
    return GetFontManager()->LoadedFonts.find(handle) != GetFontManager()->LoadedFonts.end();
  }

  const Font *FontManagerGetFont(FontHandle handle)
  {
    if (!FontManagerHasFontLoaded(handle))
    {
      return nullptr;
    }
    return &GetFontManager()->LoadedFonts.at(handle);
  }

  FontHandle FontManagerLoadFont(Font font)
  {
    FontHandle new_handle = GetFontManager()->LoadedFonts.size();
    while (FontManagerHasFontLoaded(new_handle))
    {
      new_handle++;
    }
    GetFontManager()->LoadedFonts[new_handle] = font;
    return new_handle;
  }

  void FontManagerUnloadFont(FontHandle handle)
  {
    if (!FontManagerHasFontLoaded(handle))
    {
      SR_TRACE("ERROR: Cant unload font with handle (%d). Font does not exist!", handle);
      return;
    }
    Font &font = GetFontManager()->LoadedFonts.at(handle);
    delete font.Face;
    FontTextureUnload(&font.Texture);
    GetFontManager()->LoadedFonts.erase(handle);
  }

  FontHandle srInitializeFont(Font font, unsigned int size)
  {
    FontTextureInit(&font.Texture);
    FT_Set_Char_Size(
        font.Face, // handle to face object
        0,         // char_width in 1/64 of points
        size * 64, // char_height in 1/64 of points
        96,        // horizontal device resolution
        96);
    font.Size = size;
    font.LineHeight = font.Face->size->metrics.height / 64;
    font.LineTop = font.Face->size->metrics.ascender / 64;
    font.LineBottom = font.Face->size->metrics.descender / 64;
    SR_TRACE("Loaded font. Line height = %d", font.LineHeight);
    // Load first 128 chars
    static const char *text = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ -_.:,;#'+*1234567890!\"§$%&/\\[{()}]@€";
    for (char c = 0; c < strlen(text); c++)
    {
      LoadGlyph(text[c], &font);
    }

    srTextureSetData(font.Texture.Image, FONT_TEXTURE_SIZE, FONT_TEXTURE_SIZE, FONT_TEXTURE_DEPTH == 1 ? TextureFormat_R8 : TextureFormat_RGB8, (unsigned char *)font.Texture.ImageData);

    return FontManagerLoadFont(font);
  }

  R_API FontHandle srLoadFont(const char *filePath, unsigned int size)
  {
    Font result{};

    if (FT_New_Face(GetFontManager()->Libary, filePath, 0, &result.Face) != 0)
    {
      SR_TRACE("ERROR: Could not load font from file %s", filePath);
      return -1;
    }
    return srInitializeFont(result, size);
  }

  R_API FontHandle srLoadFontFromMemory(const unsigned char *data, unsigned int data_size, unsigned int font_size)
  {
    Font result{};

    if (FT_New_Memory_Face(GetFontManager()->Libary, data, data_size, 0, &result.Face) != 0)
    {
      SR_TRACE("ERROR: Could not load font from memory");
      return -1;
    }
    return srInitializeFont(result, font_size);
  }

  R_API void srUnloadFont(FontHandle handle)
  {
    FontManagerUnloadFont(handle);
  }

  R_API int srFontGetTextWidth(FontHandle handle, const char *text)
  {
    if (!FontManagerHasFontLoaded(handle))
    {
      SR_TRACE("ERROR: Could not get line height. Font not found with handle %d!", handle);
      return 0;
    }
    const Font &font = *FontManagerGetFont(handle);
    int max_line_width = 0;
    int current_line_width = 0;

    unsigned int prev = 0;

    for (size_t i = 0; i < strlen(text); i++)
    {
      char c = text[i];
      if (c == '\n')
      {
        max_line_width = srMax(max_line_width, current_line_width);
        current_line_width = 0;
        continue;
      }
      const FontGlyph *glyph = FontTextureGetGlyph(&font, c);
      if (glyph)
      {
        unsigned int char_index = glyph->CharCode;
        if (prev)
        {
          FT_Vector kerning{};
          FT_Get_Kerning(font.Face, prev, char_index, FT_KERNING_DEFAULT, &kerning);
          current_line_width += (kerning.x >> 6);
        }
        current_line_width += glyph->advance;
      }
    }

    max_line_width = srMax(max_line_width, current_line_width);

    return max_line_width;
  }

  R_API int srFontGetTextHeight(FontHandle font, const char *text)
  {
    size_t str_len = strlen(text);
    if (str_len == 0)
    {
      return 0;
    }
    int line_count = 1;
    for (size_t i = 0; i < str_len; i++)
    {
      if (text[i] == '\n')
      {
        line_count++;
      }
    }

    return sr::srFontGetLineHeight(font) * line_count;
  }

  R_API glm::ivec2 srFontGetTextSize(FontHandle handle, const char *text)
  {
    return {
        srFontGetTextWidth(handle, text),
        srFontGetTextHeight(handle, text)};
  }

  R_API int srFontGetLineTop(FontHandle handle)
  {
    if (!FontManagerHasFontLoaded(handle))
    {
      SR_TRACE("ERROR: Could not get line top. Font not found with handle %d!", handle);
      return 0;
    }
    return FontManagerGetFont(handle)->LineTop;
  }

  R_API int srFontGetLineBottom(FontHandle handle)
  {
    if (!FontManagerHasFontLoaded(handle))
    {
      SR_TRACE("ERROR: Could not get line bottom. Font not found with handle %d!", handle);
      return 0;
    }
    return FontManagerGetFont(handle)->LineBottom;
  }

  R_API int srFontGetLineHeight(FontHandle handle)
  {
    if (!FontManagerHasFontLoaded(handle))
    {
      SR_TRACE("ERROR: Could not get line height. Font not found with handle %d!", handle);
      return 0;
    }
    return FontManagerGetFont(handle)->LineHeight;
  }

  R_API int srFontGetSize(FontHandle handle)
  {
    if (!FontManagerHasFontLoaded(handle))
    {
      SR_TRACE("ERROR: Could not get font size. Font not found with handle %d!", handle);
      return 0;
    }
    return FontManagerGetFont(handle)->Size;
  }

  R_API const char *srFontGetName(FontHandle handle)
  {
    if (!FontManagerHasFontLoaded(handle))
    {
      SR_TRACE("ERROR: Could not get font size. Font not found with handle %d!", handle);
      return 0;
    }
    return FontManagerGetFont(handle)->Face->family_name;
  }

  R_API unsigned int srFontGetTextureId(FontHandle handle)
  {
    const Font *font_ptr = FontManagerGetFont(handle);
    if (!font_ptr)
    {
      SR_TRACE("ERROR: Could not get line height. Font not found with handle %d!", handle);
      return 0;
    }
    const Font &font = *font_ptr;

    return font.Texture.Image.ID;
  }

  R_API void srDrawText(FontHandle handle, const char *text, const glm::vec2 &position, Color color, float outline_thickness, Color outline_color)
  {
    const Font *font_ptr = FontManagerGetFont(handle);
    if (!font_ptr)
    {
      SR_TRACE("ERROR: Could not draw text. Font not found with handle %d!", handle);
      return;
    }
    const Font &font = *font_ptr;

    const size_t textLen = strlen(text);

    srBegin(EBatchDrawMode::QUADS);
    srColor11c(color);
    srColor21c(outline_color);
    srPushMaterial({font.Texture.Image, SRC->DistanceFieldShader});

    float currentDepth = SRC->MainRenderBatch.CurrentDepth;

    unsigned int prev = 0;

    bool has_kerning = FT_HAS_KERNING(font.Face);

    glm::ivec2 pos = position;
    for (size_t i = 0; i < textLen; i++)
    {
      char c = text[i];
      if (c == '\n')
      {
        pos.x = position.x;
        pos.y = pos.y + font.LineHeight;
        continue;
      }
      const FontGlyph *glyph = FontTextureGetGlyph(font_ptr, c);
      if (glyph)
      {
        unsigned int char_index = glyph->CharCode;
        if (prev && has_kerning)
        {
          FT_Vector kerning{};
          FT_Get_Kerning(font.Face, prev, char_index, FT_KERNING_DEFAULT, &kerning);
          pos.x += (kerning.x >> 6);
        }
        FT_Load_Glyph(font.Face, char_index, FT_LOAD_RENDER);

        float x0 = pos.x + (glyph->Offset.x);
        float y0 = pos.y - (glyph->Offset.y);
        float x1 = x0 + (glyph->Size.x);
        float y1 = y0 + (glyph->Size.y);

        float u0 = glyph->u0;
        float v0 = glyph->v0;
        float u1 = glyph->u1;
        float v1 = glyph->v1;

        srTextureCoord2f(u0, v1);
        srNormal3f(outline_thickness, 0.0f, 0.0f);
        srVertex3f(x0, y1, currentDepth);

        srTextureCoord2f(u0, v0);
        srNormal3f(outline_thickness, 0.0f, 0.0f);
        srVertex3f(x0, y0, currentDepth);

        srTextureCoord2f(u1, v0);
        srNormal3f(outline_thickness, 0.0f, 0.0f);
        srVertex3f(x1, y0, currentDepth);

        srTextureCoord2f(u1, v1);
        srNormal3f(outline_thickness, 0.0f, 0.0f);
        srVertex3f(x1, y1, currentDepth);

        pos.x += glyph->advance;
        prev = char_index;
        currentDepth -= 0.0001f;
      }
    }
    SRC->MainRenderBatch.CurrentDepth = currentDepth;
    srEnd();
  }

  R_API void srDrawCircle(const glm::vec2 &center, float radius, Color color, unsigned int segmentCount)
  {
    srDrawArc(center, 0.0f, 360.0f, radius, color, segmentCount);
  }

  R_API void srDrawCircleOutline(const glm::vec2 &center, float radius, float thickness, Color color, unsigned int segmentCount)
  {
    srBeginPath(PathType_Stroke);
    srPathSetStrokeColor(color);
    srPathSetStrokeWidth(thickness);
    srPathArc(center, 0.0f, 360.0f, radius, segmentCount);
    srEndPath(true);
  }

  R_API void srDrawArc(const glm::vec2 &center, float startAngle, float endAngle, float radius, Color color, unsigned int segmentCount)
  {
    srCheckRenderBatchLimit((segmentCount + 1) * 3);

    // Use angle in radiens. Comes in as deg
    startAngle = startAngle * DEG2RAD;
    endAngle = endAngle * DEG2RAD;

    const float angle = endAngle - startAngle;
    const float degIncrease = angle / segmentCount;
    glm::vec2 lastPosition = glm::vec2(radius * sinf(startAngle), radius * sinf(startAngle));

    srBegin(TRIANGLES);
    srColor11c(color);

    float currentAngle = startAngle + degIncrease;
    for (unsigned int i = 0; i <= segmentCount; i++)
    {
      glm::vec2 currentPosition(radius * sinf(currentAngle), radius * cosf(currentAngle));

      srVertex2f(center);
      srVertex2f(center + lastPosition);
      srVertex2f(center + currentPosition);

      currentAngle += degIncrease;
      lastPosition = currentPosition;
    }

    srEnd();
  }

  // Path builder

  R_API void srBeginPath(PathType type)
  {
    SRC->MainRenderBatch.Path.Styles.clear();
    SRC->MainRenderBatch.Path.Points.clear();
    SRC->MainRenderBatch.Path.RenderType = type;
  }

  R_API void srEndPath(bool closedPath)
  {
    if (SRC->MainRenderBatch.Path.Points.size() == 0)
      return;

    /*if (closedPath && glm::length(SRC->RenderBatch.Path.Points.back() - SRC->RenderBatch.Path.Points[0]) < 0.1f)
    {
      SRC->RenderBatch.Path.Points.pop_back();
    }*/

    if (SRC->MainRenderBatch.Path.RenderType & PathType_Fill)
    {
      srAddPolyFilled(SRC->MainRenderBatch.Path);
    }
    if (SRC->MainRenderBatch.Path.RenderType & PathType_Stroke)
    {
      srAddPolyline(SRC->MainRenderBatch.Path, closedPath);
    }

    SRC->MainRenderBatch.Path.Styles.clear();
    SRC->MainRenderBatch.Path.Points.clear();
  }

  R_API void srPathClose()
  {
    if (SRC->MainRenderBatch.Path.Points.size() > 1)
    {
      SRC->MainRenderBatch.Path.Points.push_back(SRC->MainRenderBatch.Path.Points[0]);
    }

    srEndPath(true);
  }

  R_API void srPathLineTo(const glm::vec2 &position)
  {
    if (SRC->MainRenderBatch.Path.Points.size() == 0 || glm::length(SRC->MainRenderBatch.Path.Points.back() - position) > 0.1f)
    {
    }
    SRC->MainRenderBatch.Path.Points.push_back(position);
  }

  R_API void srPathArc(const glm::vec2 &center, float startAngle, float endAngle, float radius, unsigned int segmentCount)
  {
    // Use angle in radiens. Comes in as deg
    startAngle = (startAngle * DEG2RAD);
    endAngle = (endAngle * DEG2RAD);

    const float angle = endAngle - startAngle;
    const float degIncrease = angle / segmentCount;

    float currentAngle = startAngle;
    for (unsigned int i = 0; i <= segmentCount; i++)
    {
      glm::vec2 currentPosition(radius * sinf(currentAngle), radius * cosf(currentAngle));

      srPathLineTo(center + currentPosition);
      currentAngle += degIncrease;
    }
  }

  // https://www.w3.org/TR/SVG/implnote.html#ArcImplementationNotes
  R_API void srPathEllipticalArc(const glm::vec2 &end_point, float angle, float radius_x, float radius_y, bool large_arc_flag, bool sweep_flag, unsigned int segmentCount)
  {
    assert(SRC->MainRenderBatch.Path.Points.size() > 0);
    glm::vec2 start_point = SRC->MainRenderBatch.Path.Points.back();
    // Use angle in radiens. Comes in as deg
    angle = (angle * DEG2RAD);

    glm::mat2 rotation_matrix = glm::mat2(glm::vec2(cosf(angle), sinf(angle)), glm::vec2(-sinf(angle), cosf(angle)));
    glm::vec2 v = rotation_matrix * ((start_point - end_point) / 2.0f);

    float x1p = v.x;
    float y1p = v.y;

    radius_x = srAbs(radius_x);
    radius_y = srAbs(radius_y);

    float lamba = ((x1p * x1p) / (radius_x * radius_x)) + ((y1p * y1p) / (radius_y * radius_y));
    if (lamba > 1.0f)
    {
      radius_x *= sqrtf(lamba);
      radius_y *= sqrtf(lamba);
    }

    float radius_x_sqr = radius_x * radius_x;
    float radius_y_sqr = radius_y * radius_y;
    float x1p_sqr = x1p * x1p;
    float y1p_sqr = y1p * y1p;

    float sign = (large_arc_flag == sweep_flag) ? -1.0f : 1.0f;
    float c = sign * sqrtf(((radius_x_sqr * radius_y_sqr) - (radius_x_sqr * y1p_sqr) - (radius_y_sqr * x1p_sqr)) / ((radius_x_sqr * y1p_sqr) + (radius_y_sqr * x1p_sqr)));

    glm::vec2 cp = c * glm::vec2((radius_x * y1p) / radius_y, (-radius_y * x1p) / radius_x);
    glm::mat2 cp_rotation_matrix = glm::mat2(glm::vec2(cosf(angle), -sinf(angle)), glm::vec2(sinf(angle), cosf(angle)));

    glm::vec2 center_point = (cp_rotation_matrix * cp) + glm::vec2{(start_point.x + end_point.x) / 2.0f, (start_point.y + end_point.y) / 2.0f};

    glm::vec2 angle_v = {(x1p - cp.x) / radius_x, (y1p - cp.y) / radius_y};

    auto arccos = [](const glm::vec2 &a, const glm::vec2 &b) -> float
    {
      float sign = (a.x * b.y) - (a.y * b.x);
      sign = sign / srAbs(sign);
      return sign * acosf(glm::dot(a, b) / (glm::length(a) * glm::length(b)));
    };

    float start_angle = arccos(glm::vec2{1.0f, 0.0f}, angle_v);

    glm::vec2 angle_range_v = {(-x1p - cp.x) / radius_x, (-y1p - cp.y) / radius_y};
    float angle_range = arccos(angle_v, angle_range_v);

    if (!sweep_flag && angle_range > 0.0f)
    {
      angle_range -= 2.0f * PI;
    }
    else if (sweep_flag && angle_range < 0.0f)
    {
      angle_range += 2.0f * PI;
    }

    const float degIncrease = angle_range / segmentCount;

    float currentAngle = start_angle + degIncrease;

    float rot_angle_sin = sinf(angle);
    float rot_angle_cos = cosf(angle);

    for (unsigned int i = 0; i < segmentCount; i++)
    {
      glm::vec2 currentPosition = cp_rotation_matrix * glm::vec2(radius_x * cosf(currentAngle), radius_y * sinf(currentAngle));

      srPathLineTo(center_point + currentPosition);
      currentAngle += degIncrease;
    }
  }

  R_API void srPathCubicBezierTo(const glm::vec2 &controll1, const glm::vec2 &controll2, const glm::vec2 &endPosition, unsigned int segmentCount)
  {
    assert(SRC->MainRenderBatch.Path.Points.size() > 0);
    glm::vec2 start_point = SRC->MainRenderBatch.Path.Points.back();

    for (unsigned int i = 1; i <= segmentCount; i++)
    {
      float t = (float)i / (float)segmentCount;
      float u = 1.0f - t;
      float tt = t * t;
      float uu = u * u;
      float uuu = uu * u;
      float ttt = tt * t;

      glm::vec2 p = uuu * start_point;
      p += 3 * uu * t * controll1;
      p += 3 * u * tt * controll2;
      p += ttt * endPosition;

      srPathLineTo(p);
    }
  }

  R_API void srPathQuadraticBezierTo(const glm::vec2 &controll, const glm::vec2 &endPosition, unsigned int segmentCount)
  {
    assert(SRC->MainRenderBatch.Path.Points.size() > 0);
    glm::vec2 start_point = SRC->MainRenderBatch.Path.Points.back();

    for (unsigned int i = 1; i <= segmentCount; i++)
    {
      float t = (float)i / (float)segmentCount;
      float u = 1.0f - t;
      float tt = t * t;
      float uu = u * u;

      glm::vec2 p = uu * start_point;
      p += 2 * u * t * controll;
      p += tt * endPosition;

      srPathLineTo(p);
    }
  }

  R_API void srPathSetStrokeEnabled(bool showStroke)
  {
    if (showStroke)
      SRC->MainRenderBatch.Path.RenderType |= PathType_Stroke;
    else
      SRC->MainRenderBatch.Path.RenderType &= ~PathType_Stroke;
  }

  R_API void srPathSetFillEnabled(bool fill)
  {
    if (fill)
      SRC->MainRenderBatch.Path.RenderType |= PathType_Fill;
    else
      SRC->MainRenderBatch.Path.RenderType &= ~PathType_Fill;
  }

  R_API void srPathSetFillColor(Color color)
  {
    PathBuilder::PathStyleIndex &styleIndex = srPathBuilderNewStyle();
    styleIndex.second.FillColor = color;
    SRC->MainRenderBatch.Path.CurrentPathStyle.FillColor = color;
  }

  R_API void srPathSetFillColor(const glm::vec4 &color)
  {
    srPathSetFillColor(srGetColorFromFloat(color));
  }

  R_API void srPathSetStrokeColor(Color color)
  {
    PathBuilder::PathStyleIndex &styleIndex = srPathBuilderNewStyle();
    styleIndex.second.StrokeColor = color;
    SRC->MainRenderBatch.Path.CurrentPathStyle.StrokeColor = color;
  }

  R_API void srPathSetStrokeColor(const glm::vec4 &color)
  {
    srPathSetStrokeColor(srGetColorFromFloat(color));
  }

  R_API void srPathSetStrokeWidth(float width)
  {
    PathBuilder::PathStyleIndex &styleIndex = srPathBuilderNewStyle();
    styleIndex.second.StrokeWidth = width;
    SRC->MainRenderBatch.Path.CurrentPathStyle.StrokeWidth = width;
  }

  R_API void srPathSetStyle(const PathStyle &style)
  {
    PathBuilder::PathStyleIndex &styleIndex = srPathBuilderNewStyle();
    styleIndex.second = style;
    SRC->MainRenderBatch.Path.CurrentPathStyle = style;
  }

  R_API PathBuilder::PathStyleIndex &srPathBuilderNewStyle()
  {
    PathBuilder &pb = SRC->MainRenderBatch.Path;

    if (pb.Styles.size() == 0)
    {
      PathBuilder::PathStyleIndex newStyleIndex;
      newStyleIndex.first = pb.Points.size();
      newStyleIndex.second = pb.CurrentPathStyle;
      pb.Styles.push_back(newStyleIndex);
      pb.PreviousStyleVertexIndex = 0;
      return pb.Styles.back();
    }

    PathBuilder::PathStyleIndex &lastStyle = pb.Styles.back();

    // We have no new points, we can just modify the current one
    if (pb.Points.size() - (pb.PreviousStyleVertexIndex + lastStyle.first) == 0)
    {
      return lastStyle;
    }

    lastStyle.first = pb.Points.size() - pb.PreviousStyleVertexIndex;
    pb.PreviousStyleVertexIndex += lastStyle.first;

    // Create new style index
    PathBuilder::PathStyleIndex newStyleIndex;
    PathStyle newStyle = lastStyle.second;

    pb.Styles.push_back(newStyleIndex);
    return pb.Styles.back();
  }

  // Flushing path
  R_API void srAddPolyline(const PathBuilder &pb, bool closedPath)
  {
    const size_t count = pb.Points.size();
    srCheckRenderBatchLimit(count * 4);

    PathStyle currentStyle = pb.CurrentPathStyle;
    unsigned int nextStyleChange = 0;
    unsigned int currentStyleIndex = 0;
    if (pb.Styles.size() > 0)
    {
      currentStyle = pb.Styles[0].second;
      nextStyleChange = pb.Styles[0].first;
    }

    srBegin(TRIANGLES);
    srColor11c(currentStyle.StrokeColor);

    glm::vec2 lastTop{};
    glm::vec2 lastBottom{};

    closedPath = closedPath && pb.Points[0] != pb.Points.back();

    for (size_t i1 = 0; i1 < count + (closedPath ? 1 : 0); i1++)
    {
      const glm::vec2 &currentPoint = pb.Points[i1 % count];
      const glm::vec2 &previousPoint = pb.Points[i1 == 0 ? (closedPath ? pb.Points.size() - 1 : 0) : (i1 - 1)];
      const glm::vec2 &nextPoint = pb.Points[(i1 + 1) % count];

      const float prevLength = glm::length(currentPoint - previousPoint);
      const float nextLength = glm::length(nextPoint - currentPoint);

      const glm::vec2 dir1 = glm::normalize(currentPoint - previousPoint);
      const glm::vec2 dir2 = glm::normalize(nextPoint - currentPoint);

      // 90° dir vector
      const glm::vec2 normalWidthVector1 = glm::normalize(glm::vec2(dir1.y, -dir1.x));
      const glm::vec2 normalWidthVector2 = glm::normalize(glm::vec2(dir2.y, -dir2.x));

      const glm::vec2 widthVector1 = normalWidthVector1 * currentStyle.StrokeWidth * 0.5f;
      const glm::vec2 widthVector2 = normalWidthVector2 * currentStyle.StrokeWidth * 0.5f;

      glm::vec2 currentConnectedTop = currentPoint + widthVector1;
      glm::vec2 currentConnectedBottom = currentPoint - widthVector1;

      if (!closedPath && i1 == 0)
      {
        currentConnectedTop = currentPoint + widthVector2;
        currentConnectedBottom = currentPoint - widthVector2;
      }

      if ((i1 < count - 1 && i1 > 0) || closedPath)
      {
        // Get calc points
        const glm::vec2 cornerA = currentPoint + widthVector1;
        const glm::vec2 cornerB = currentPoint + widthVector2;

        // Check which point is pos and which negative
        const glm::vec2 center = cornerA + ((cornerB - cornerA) / 2.0f);

        const float connectionLength = glm::length(cornerB - cornerA);
        const float diagonalLength = glm::length(currentPoint - center);

        float a = ((connectionLength / 2.0f) / diagonalLength) * (currentStyle.StrokeWidth / 2.0f);
        // SR_TRACE("SF=%f, f=%f, a=%f", connectionLength, diagonalLength, a);
        float a_clamp = srMin(prevLength, nextLength);
        a = srClamp(a, -a_clamp, a_clamp);

        float dot = glm::dot(dir1, normalWidthVector2);
        float funny = dot / srAbs(dot);
        if (funny == 0)
          funny = 1;
        currentConnectedTop += (dir1 * a) * funny;
        currentConnectedBottom -= (dir1 * a) * funny;
      }

      if (i1 == 0)
      {
        lastTop = currentConnectedTop;       // We use widthVector2 because prev and current is the same point
        lastBottom = currentConnectedBottom; // We want the distance to the next point
        continue;
      }

      // Find connection point for good filling

      srVertex2f(lastBottom);
      srVertex2f(currentConnectedBottom);
      srVertex2f(currentConnectedTop);

      srVertex2f(currentConnectedTop);
      srVertex2f(lastTop);
      srVertex2f(lastBottom);

      // SR_TRACE("Rendering QUAD index %d\nLT(%f, %f)\nCT(%f, %f)\nCB(%f, %f)\nLB(%f, %f)", i1, lastTop.x, lastTop.y, currentConnectedTop.x, currentConnectedTop.y, currentConnectedBottom.x, currentConnectedBottom.y, lastBottom.x, lastBottom.y);

      lastBottom = currentConnectedBottom;
      lastTop = currentConnectedTop;

      if (nextStyleChange <= i1)
      {
        currentStyleIndex++;
        if (currentStyleIndex < pb.Styles.size())
        {
          currentStyle = pb.Styles[currentStyleIndex].second;
          nextStyleChange += pb.Styles[currentStyleIndex].first;
          srColor11c(currentStyle.StrokeColor);
        }
      }
    }
    srEnd();
  }

  R_API void srAddPolyFilled(const PathBuilder &pb)
  {
    if (pb.Points.size() < 3)
    {
      SR_TRACE("Not enough points for fill");
      return;
    }
    const size_t count = pb.Points.size();
    srCheckRenderBatchLimit(count * 3);

    PathStyle currentStyle = pb.CurrentPathStyle;
    unsigned int nextStyleChange = 0;
    unsigned int currentStyleIndex = 0;
    if (pb.Styles.size() > 0)
    {
      currentStyle = pb.Styles[0].second;
      nextStyleChange = pb.Styles[0].first;
    }

    srBegin(TRIANGLES);
    srColor11c(currentStyle.FillColor);

    for (size_t i = 1; i < count; i++)
    {
      const size_t i2 = (i + 1) % count;
      srVertex2f(pb.Points[0]);
      srVertex2f(pb.Points[i]);
      srVertex2f(pb.Points[i2]);

      if (nextStyleChange <= i)
      {
        currentStyleIndex++;
        if (currentStyleIndex < pb.Styles.size())
        {
          currentStyle = pb.Styles[currentStyleIndex].second;
          nextStyleChange += pb.Styles[currentStyleIndex].first;
          srColor11c(currentStyle.FillColor);
        }
      }
    }

    srEnd();
  }
}
