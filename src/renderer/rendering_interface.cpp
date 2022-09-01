#include "../pch.h"
#include "renderer.h"
#include "glad/glad.h"

sr::SRContext* sr::SRC = nullptr;

const char* basicMeshVertexShader = R"(
  #version 330 core

  layout(location = 0) in vec3 vPosition;
  layout(location = 1) in vec3 vNormal;
  layout(location = 2) in vec2 vTexCoord;
  layout(location = 3) in vec4 vColor;

  uniform mat4 ProjectionMatrix = mat4(1.0);

  out vec4 Color;

  void main()
  {
    Color = vColor;
    gl_Position = ProjectionMatrix * vec4(vPosition, 1.0);
  }
)";

const char* basicMeshFragmentShader = R"(
  #version 330 core

  layout(location = 0) out vec4 fragColor;

  in vec4 Color;

  void main()
  {
    fragColor = Color;
  }
)";



char const* gl_error_string(GLenum const err) 
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

#define glCall(x)                                                                            \
    x;                                                                                       \
    {GLenum glob_err = 0;                                                                     \
    while ((glob_err = glGetError()) != GL_NO_ERROR)                                         \
    {                                                                                        \
        printf("GL_ERROR calling \"%s\": %s %s\n", #x, gl_error_string(glob_err), __FILE__); \
    }}




namespace sr {

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
      for (Mesh& mesh : SRC->AutoReleaseMeshes)
      {
        srUnloadMesh(&mesh);
      }

      delete SRC;
      SRC = NULL;
    }
  }

  R_API void srInitGL()
  {
    SR_TRACE("OpenGL-Context: %s", glGetString(GL_VERSION));
    glCall(glEnable(GL_DEPTH_TEST));
    glCall(glEnable(GL_BLEND));
    glCall(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));
  }

  R_API void srInitContext(SRContext* context)
  {
    if (context->DefaultShader.ID == 0)
    {
      SRC->DefaultShader = srLoadShader(basicMeshVertexShader, basicMeshFragmentShader);
    }
    context->RenderBatch = srLoadRenderBatch(5000);
  }

  R_API void srNewFrame(int frameWidth, int frameHeight)
  {
    srViewport(0, 0, (float)frameWidth, (float)frameHeight);
    srClearColor(0.5f, 0.4f, 0.8f, 1.0f);
    srClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    const float halfWidth = frameWidth / 2.0f;
    const float halfHeight = frameHeight / 2.0f;

    SRC->CurrentProjection = glm::orthoLH(-halfWidth, halfWidth, -halfHeight, halfHeight, -1.0f, 1.0f);
    SRC->RenderBatch.CurrentDepth = 0.0f;
  }
  
  R_API void srEndFrame()
  {
    srDrawRenderBatch(&SRC->RenderBatch);
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
      case PolygonFillMode_Fill: glCall(glPolygonMode(GL_FRONT_AND_BACK, GL_FILL)); break;
      case PolygonFillMode_Line: glCall(glPolygonMode(GL_FRONT_AND_BACK, GL_LINE)); break;
    }
  }

  R_API Color srGetColorFromFloat(float r, float g, float b, float a)
  {
    // Clip values between 0 and 1
    r = srClamp(r, 0.0f, 1.0f);
    g = srClamp(g, 0.0f, 1.0f);
    b = srClamp(b, 0.0f, 1.0f);
    a = srClamp(a, 0.0f, 1.0f);

    return  (((unsigned char) (a * 255)) << 24) 
          | (((unsigned char) (b * 255)) << 16) 
          | (((unsigned char) (g * 255)) << 8) 
          |  ((unsigned char) (r * 255));
  }

  R_API Color srGetColorFromFloat(const glm::vec4& color)
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


  R_API Shader srLoadShader(const char* vertSrc, const char* fragSrc)
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
              char *log = (char*) malloc(maxLength * sizeof(char));
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

  R_API int srCompileShader(int shader_type, const char* shader_source)
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
        case GL_VERTEX_SHADER: SR_TRACE("SHADER: Failed Compiling Vertex Shader [ID %i]", result); break;
        case GL_FRAGMENT_SHADER: SR_TRACE("SHADER: Failed Compiling Fragment Shader [ID %i]", result); break;
        case GL_GEOMETRY_SHADER: SR_TRACE("SHADER: Failed Compiling Geometry Shader [ID %i]", result); break;
        

        default:
            break;
        }

        int maxLength = 0;
        glCall(glGetShaderiv(result, GL_INFO_LOG_LENGTH, &maxLength));

        if (maxLength > 0)
        {
            int length = 0;
            char *log = (char*) malloc(maxLength * sizeof(char));
            glCall(glGetShaderInfoLog(result, maxLength, &length, log));
            SR_TRACE("SHADER: [ID %i] Compile error: %s", result, log);
            free(log);
        }
    }
    else
    {
        switch (shader_type)
        {
        case GL_VERTEX_SHADER: SR_TRACE("SHADER: Successfully compiled Vertex Shader [ID %i]", result); break;
        case GL_FRAGMENT_SHADER: SR_TRACE("SHADER: Successfully compiled Fragment Shader [ID %i]", result); break;
        case GL_GEOMETRY_SHADER: SR_TRACE("SHADER: Successfully compiled Geometry Shader [ID %i]", result); break;
        default: break;
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

  R_API void srSetDefaultShaderUniforms(Shader shader)
  {
    srUseShader(shader);

    unsigned int projectionMatrixId = glGetUniformLocation(shader.ID, "ProjectionMatrix");
    if (projectionMatrixId == -1)
    {
      SR_TRACE("Could not find projection matrix uniform location!");
    }
    else
    {
      glUniformMatrix4fv(projectionMatrixId, 1, GL_FALSE, glm::value_ptr(SRC->CurrentProjection));
    }
  }



  // Vertex Arrays

  R_API unsigned int srGetVertexAttributeComponentCount(EVertexAttributeType type)
  {
    switch (type)
    {
    case EVertexAttributeType::INT:
    case EVertexAttributeType::UINT:
    case EVertexAttributeType::BOOL:
    case EVertexAttributeType::FLOAT:   return 1;
    case EVertexAttributeType::FLOAT2:
    case EVertexAttributeType::INT2:    return 2;
    case EVertexAttributeType::FLOAT3:
    case EVertexAttributeType::INT3:    return 3;
    case EVertexAttributeType::FLOAT4:
    case EVertexAttributeType::INT4:  
    case EVertexAttributeType::BYTE4:   return 4;
    }
    SR_TRACE("ERROR: Could not get vertex attrib type component count");

    return 0;
  }

  R_API unsigned int srGetVertexAttributeTypeSize(EVertexAttributeType type)
  {
    switch (type)
    {
    case EVertexAttributeType::INT:     return sizeof(int);
    case EVertexAttributeType::UINT:    return sizeof(unsigned int);
    case EVertexAttributeType::BOOL:    return sizeof(bool);
    case EVertexAttributeType::FLOAT:   return sizeof(float);
    case EVertexAttributeType::FLOAT2:  return sizeof(float) * 2;
    case EVertexAttributeType::INT2:    return sizeof(int) * 2;
    case EVertexAttributeType::FLOAT3:  return sizeof(float) * 3;
    case EVertexAttributeType::INT3:    return sizeof(int) * 3;
    case EVertexAttributeType::FLOAT4:  return sizeof(float) * 4;
    case EVertexAttributeType::INT4:    return sizeof(int) * 4;
    case EVertexAttributeType::BYTE4:   return sizeof(unsigned char) * 4;
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
      case EVertexAttributeType::FLOAT4: return GL_FLOAT;
      case EVertexAttributeType::INT:
      case EVertexAttributeType::INT2:
      case EVertexAttributeType::INT3:
      case EVertexAttributeType::INT4: return GL_INT;
      case EVertexAttributeType::UINT: return GL_UNSIGNED_INT;
      case EVertexAttributeType::BOOL: return GL_BOOL;
      case EVertexAttributeType::BYTE4: return GL_UNSIGNED_BYTE;
    }
    SR_TRACE("ERROR: Could not convert vertex attrib type");
    return 0;
  }

  R_API size_t srGetVertexLayoutSize(const VertexArrayLayout& layout)
  {
    size_t size = 0;
    for (const VertexArrayLayoutElement& elem : layout)
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

  R_API void srSetVertexAttribute(unsigned int location, unsigned int numElements, unsigned int type, bool normalized, int stride, const void* pointer)
  {
    glVertexAttribPointer(location, numElements, type, normalized, stride, pointer);
  }
  
  R_API void srEnableVertexAttribute(unsigned int location)
  {
    glEnableVertexAttribArray(location);
  }

  R_API Mesh srLoadMesh(const MeshInit& initData)
  {
    Mesh result{};
    if (initData.Vertices.size() == 0)
    {
      SR_TRACE("No Vertices provided for LoadingMesh");
      return result;
    }
    assert(!(initData.Vertices.size() 
            == (initData.Normals.size() == 0 ? initData.Vertices.size() : initData.Normals.size())
            == (initData.TexCoord1.size() == 0 ? initData.Vertices.size() : initData.TexCoord1.size())));

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


  R_API unsigned int srLoadVertexBuffer(void* data, size_t data_size)
  {
    unsigned int result = 0;
    glCall(glGenBuffers(1, &result));
    glCall(glBindBuffer(GL_ARRAY_BUFFER, result));

    glCall(glBufferData(GL_ARRAY_BUFFER, data_size, data, GL_STATIC_DRAW));
    return result;
  }

  R_API unsigned int srLoadElementBuffer(void* data, size_t data_size)
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

  
  R_API void srDrawMesh(const Mesh& mesh)
  {
    if (mesh.VAO.VAO <= 0)
    {
      SR_TRACE("ERROR: DrawMesh failed. VertexArray not initialized!");
      return;
    }
    srSetDefaultShaderUniforms(SRC->DefaultShader);

    if (!srBindVertexArray(mesh.VAO.VAO))
    {
      for (const auto& buffer : mesh.VAO.VBOs)
      {
        srBindVertexBuffer(buffer.ID);
        
        unsigned int vertexSize = srGetVertexLayoutSize(buffer.Layout);
        unsigned int currentOffset = 0;
        for (const VertexArrayLayoutElement& elem : buffer.Layout)
        {
          srSetVertexAttribute(elem.Location, srGetVertexAttributeComponentCount(elem.ElementType), srGetGLVertexAttribType(elem.ElementType), elem.Normalized, vertexSize, (const void*) currentOffset);
          srEnableVertexAttribute(elem.Location);
          currentOffset += srGetVertexAttributeTypeSize(elem.ElementType);
        }
      }
    }

    if (mesh.VAO.IBO != 0)
    {
      //srBindElementBuffer(mesh.VAO.IBO); // In case mac does not work use this here
      glCall(glDrawElements(GL_TRIANGLES, mesh.ElementCount, GL_UNSIGNED_INT, NULL));
    }
    else
    {
      glCall(glDrawArrays(GL_TRIANGLES, 0, mesh.VertexCount));
    }
  }

  R_API void srUploadMesh(Mesh* mesh)
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

  R_API void srUnloadMesh(Mesh* mesh)
  {
    srUnloadVertexBuffers(mesh->VAO);
    srDeleteMeshCPUData(mesh);
  }

  R_API void srDeleteMeshCPUData(Mesh* mesh)
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



  R_API void srUnloadVertexBuffers(const VertexBuffers& vao)
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
    for (int j = 0; j < (6*bufferSize); j += 6)
    {
        result.DrawBuffer.Indices[j] = 4*k;
        result.DrawBuffer.Indices[j + 1] = 4*k + 1;
        result.DrawBuffer.Indices[j + 2] = 4*k + 2;
        result.DrawBuffer.Indices[j + 3] = 4*k;
        result.DrawBuffer.Indices[j + 4] = 4*k + 2;
        result.DrawBuffer.Indices[j + 5] = 4*k + 3;

        k++;
    }


    VertexBuffers glBinding;
    glBinding.VAO = srLoadVertexArray();
    srBindVertexArray(glBinding.VAO);

    unsigned int vbo = srLoadVertexBuffer(result.DrawBuffer.Vertices, bufferSize * 4 * sizeof(RenderBatch::Vertex));
    srEnableVertexAttribute(0);
    srSetVertexAttribute(0, 3, GL_FLOAT, GL_FALSE, sizeof(RenderBatch::Vertex), 0);
    srEnableVertexAttribute(1);
    srSetVertexAttribute(1, 3, GL_FLOAT, GL_FALSE, sizeof(RenderBatch::Vertex), (const void*) offsetof(RenderBatch::Vertex, RenderBatch::Vertex::Normal));
    srEnableVertexAttribute(2);
    srSetVertexAttribute(2, 2, GL_FLOAT, GL_FALSE, sizeof(RenderBatch::Vertex), (const void*) offsetof(RenderBatch::Vertex, RenderBatch::Vertex::UV));
    srEnableVertexAttribute(3);
    srSetVertexAttribute(3, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(RenderBatch::Vertex), (const void*) offsetof(RenderBatch::Vertex, RenderBatch::Vertex::Color));
    glBinding.VBOs.push_back({
                            {VertexArrayLayoutElement(EVertexAttributeType::FLOAT3, 0),
                            VertexArrayLayoutElement(EVertexAttributeType::FLOAT3, 1),
                            VertexArrayLayoutElement(EVertexAttributeType::FLOAT2, 2),
                            VertexArrayLayoutElement(EVertexAttributeType::BYTE4, 3)}
                            , vbo});

    unsigned int ibo = srLoadElementBuffer(result.DrawBuffer.Indices, bufferSize * 6 * sizeof(unsigned int));
    glBinding.IBO = ibo;

    srBindVertexArray(0);

    result.DrawBuffer.GlBinding = glBinding;
    return result;
  }

  R_API void srIncreaseRenderBatchCurrentDraw(RenderBatch* batch)
  {
    if (batch->CurrentDraw++ >= SR_BATCH_DRAW_CALLS)
    {
      srDrawRenderBatch(batch);
    }
  }

  R_API bool srCheckRenderBatchLimit(unsigned int numVerts)
  {
    bool overflow = false;
    RenderBatch& rb = SRC->RenderBatch;

    if (rb.VertexCounter + numVerts >= rb.DrawBuffer.ElementCount)
    {
      EBatchDrawMode currentMode = rb.DrawCalls[rb.CurrentDraw].Mode;

      srDrawRenderBatch(&rb);

      rb.DrawCalls[rb.CurrentDraw].Mode = currentMode;
      overflow = true;
    }
    return overflow;
  }

  R_API void srDrawRenderBatch(RenderBatch* batch)
  {
    srSetDefaultShaderUniforms(SRC->DefaultShader);

    if (!srBindVertexArray(batch->DrawBuffer.GlBinding.VAO))
    {
      for (const auto& buffer : batch->DrawBuffer.GlBinding.VBOs)
      {
        srBindVertexBuffer(buffer.ID);
        unsigned int vertexSize = srGetVertexLayoutSize(buffer.Layout);
        unsigned int currentOffset = 0;
        for (const VertexArrayLayoutElement& elem : buffer.Layout)
        {
          srSetVertexAttribute(elem.Location, srGetVertexAttributeComponentCount(elem.ElementType), srGetGLVertexAttribType(elem.ElementType), elem.Normalized, vertexSize, (const void*) currentOffset);
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
      EBatchDrawMode mode = batch->DrawCalls[i].Mode;
      switch (mode)
      {
      case EBatchDrawMode::LINES:     glCall(glDrawArrays(GL_LINES, vertexOffset, batch->DrawCalls[i].VertexCount)); break;
      case EBatchDrawMode::TRIANGLES: glCall(glDrawArrays(GL_TRIANGLES, vertexOffset, batch->DrawCalls[i].VertexCount)); break;
      case EBatchDrawMode::QUADS:     glCall(glDrawElements(GL_TRIANGLES, batch->DrawCalls[i].VertexCount/4*6, GL_UNSIGNED_INT, (GLvoid*) (vertexOffset / 4 * 6 * sizeof(unsigned int)))); break;
      }
      vertexOffset += batch->DrawCalls[i].VertexCount + batch->DrawCalls[i].VertexAlignment;
    }

    batch->CurrentDraw = 0;
    batch->DrawCalls[0] = RenderBatch::DrawCall{};
    batch->VertexCounter = 0;
  }



  R_API void srBegin(EBatchDrawMode mode)
  {
    RenderBatch& rb = SRC->RenderBatch;
    if (rb.DrawCalls[rb.CurrentDraw].Mode != mode)
    {
      if (rb.DrawCalls[rb.CurrentDraw].Mode == EBatchDrawMode::LINES) 
        rb.DrawCalls[rb.CurrentDraw].VertexAlignment = rb.DrawCalls[rb.CurrentDraw].VertexCount % 4;
      else if (rb.DrawCalls[rb.CurrentDraw].Mode == EBatchDrawMode::TRIANGLES)
        rb.DrawCalls[rb.CurrentDraw].VertexAlignment = 4 - (rb.DrawCalls[rb.CurrentDraw].VertexCount % 4);
      else rb.DrawCalls[rb.CurrentDraw].VertexAlignment = 0;

      rb.VertexCounter += rb.DrawCalls[rb.CurrentDraw].VertexAlignment; // Offset vertex counter so it is all nice

      srIncreaseRenderBatchCurrentDraw(&rb);
      rb.DrawCalls[rb.CurrentDraw].Mode = mode;
      rb.DrawCalls[rb.CurrentDraw].VertexCount = 0;
      rb.DrawCalls[rb.CurrentDraw].VertexAlignment = 0;
    }
  }

  R_API void srVertex3f(float x, float y, float z)
  {
    srVertex3f({x, y, z});
  }

  R_API void srVertex3f(const glm::vec3& vertex)
  {
    srCheckRenderBatchLimit(1);

    SRC->RenderBatch.DrawBuffer.Vertices[SRC->RenderBatch.VertexCounter].Pos = vertex;
    SRC->RenderBatch.DrawBuffer.Vertices[SRC->RenderBatch.VertexCounter].Normal = SRC->RenderBatch.CurrentNormal;
    SRC->RenderBatch.DrawBuffer.Vertices[SRC->RenderBatch.VertexCounter].Color = SRC->RenderBatch.CurrentColor;


    SRC->RenderBatch.VertexCounter++;
    SRC->RenderBatch.DrawCalls[SRC->RenderBatch.CurrentDraw].VertexCount++;
  }

  R_API void srVertex2f(float x, float y)
  {
    srVertex3f(glm::vec3(x, y, SRC->RenderBatch.CurrentDepth));
  }

  R_API void srVertex2f(const glm::vec2& vertex)
  {
    srVertex3f(glm::vec3(vertex.x, vertex.y, SRC->RenderBatch.CurrentDepth));
  }


  R_API void srColor4f(float r, float g, float b, float a)
  {
    SRC->RenderBatch.CurrentColor = srGetColorFromFloat(r, g, b, a);
  }

  R_API void srColor4f(const glm::vec4& color)
  {
    SRC->RenderBatch.CurrentColor = srGetColorFromFloat(color);
  }

  R_API void srColor1c(Color color)
  {
    SRC->RenderBatch.CurrentColor = color;
  }

  R_API void srEnd()
  {
    SRC->RenderBatch.CurrentDepth -= 0.0001f;
  }

  R_API void srDrawRectanglePro(const Rectangle& rect, const glm::vec2& origin, float rotation, float cornerRadius, PathType pathType, PathStyle style)
  {
    glm::vec2 topLeft;
    glm::vec2 topRight;
    glm::vec2 bottomLeft;
    glm::vec2 bottomRight;
    // Draw rect with simple corners
    float sin = rotation != 0 ? sinf(rotation*DEG2RAD) : /*sin(0) =*/0;
    float cos = rotation != 0 ? cosf(rotation*DEG2RAD) : /*cos(0) =*/1;

    if (rotation == 0.0f)
    {
      float x = rect.X - origin.x;
      float y = rect.Y - origin.y;
      topLeft = glm::vec2(x, y + rect.Height);
      topRight = glm::vec2(x + rect.Width, y + rect.Height);
      bottomLeft = glm::vec2(x, y);
      bottomRight = glm::vec2(x + rect.Width, y);
    } 
    else
    {
      float x = rect.X;
      float y = rect.Y;
      float dx = -origin.x;
      float dy = -origin.y;

      topLeft.x = x + dx*cos - (dy + rect.Height)*sin;
      topLeft.y = y + dx*sin + (dy + rect.Height)*cos;

      topRight.x = x + (dx + rect.Width)*cos - (dy + rect.Height)*sin;
      topRight.y = y + (dx + rect.Width)*sin + (dy + rect.Height)*cos;

      bottomLeft.x = x + dx*cos - dy*sin;
      bottomLeft.y = y + dx*sin + dy*cos;

      bottomRight.x = x + (dx + rect.Width)*cos - dy*sin;
      bottomRight.y = y + (dx + rect.Width)*sin + dy*cos;
    }

    if (pathType & PathType_Stroke || cornerRadius >= 0.0f)
    {
      srBeginPath(pathType);
      srPathSetStyle(style);
      if (cornerRadius > 0.0f)
      {
          // Start with top left (right of arc) corner
          cornerRadius = srClamp(cornerRadius, 0.0f, 1.0f);
          const float min_size = (srMin(rect.Width, rect.Height) / 2.0f) * cornerRadius;
          const glm::vec2 arcCenterX = glm::vec2(min_size * cos, min_size * sin);
          const glm::vec2 arcCenterY = glm::vec2(min_size * -sin, min_size * cos);

          srPathArc(topLeft + arcCenterX - arcCenterY, -90.0f - rotation, -rotation, min_size);
          srPathArc(topRight - arcCenterX - arcCenterY, 0.0f - rotation, 90.0f - rotation, min_size);
          srPathArc(bottomRight -arcCenterX + arcCenterY, 90.0f - rotation, 180.0f - rotation, min_size);
          srPathArc(bottomLeft + arcCenterX + arcCenterY, 180.0f - rotation, 270.0f - rotation, min_size);
      }
      else
      {
        srPathLineTo(topLeft);
        srPathLineTo(topRight);
        srPathLineTo(bottomRight);
        srPathLineTo(bottomLeft);
      }
      srEndPath(true);
    }
    else
    {
      srBegin(EBatchDrawMode::QUADS);
      srColor1c(style.FillColor);

      srVertex2f(topLeft);
      srVertex2f(topRight);
      srVertex2f(bottomRight);
      srVertex2f(bottomLeft);

      srEnd();
    }
  }

  R_API void srDrawCircle(const glm::vec2& center, float radius, Color color, unsigned int segmentCount)
  {
    srDrawArc(center, 0.0f, 360.0f, radius, color, segmentCount);
  }

  R_API void srDrawArc(const glm::vec2& center, float startAngle, float endAngle, float radius, Color color, unsigned int segmentCount)
  {
    srCheckRenderBatchLimit((segmentCount + 1) * 3);

    // Use angle in radiens. Comes in as deg
    startAngle = startAngle * DEG2RAD;
    endAngle = endAngle * DEG2RAD;

    const float angle = endAngle - startAngle;
    const float degIncrease = angle / segmentCount;
    glm::vec2 lastPosition = glm::vec2(radius * sinf(startAngle), radius * sinf(startAngle));

    srBegin(TRIANGLES);
    srColor1c(color);

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
    SRC->RenderBatch.Path.Styles.clear();
    SRC->RenderBatch.Path.Points.clear();
    SRC->RenderBatch.Path.RenderType = type;
  }

  R_API void srEndPath(bool closedPath)
  {
    if (SRC->RenderBatch.Path.Points.size() == 0) return;

    if (SRC->RenderBatch.Path.RenderType & PathType_Fill)
    {
      srAddPolyFilled(SRC->RenderBatch.Path);
    }
    if (SRC->RenderBatch.Path.RenderType & PathType_Stroke)
    {
      srAddPolyline(SRC->RenderBatch.Path, closedPath);
    }

    SRC->RenderBatch.Path.Styles.clear();
    SRC->RenderBatch.Path.Points.clear();
  }

  R_API void srPathClose()
  {
    if (SRC->RenderBatch.Path.Points.size() > 1)
    {
      SRC->RenderBatch.Path.Points.push_back(SRC->RenderBatch.Path.Points[0]);
    }

    srEndPath();
  }

  R_API void srPathLineTo(const glm::vec2& position)
  {
    SRC->RenderBatch.Path.Points.push_back(position);
  }

  R_API void srPathArc(const glm::vec2& center, float startAngle, float endAngle, float radius, unsigned int segmentCount)
  {
        // Use angle in radiens. Comes in as deg
    startAngle = startAngle * DEG2RAD;
    endAngle = endAngle * DEG2RAD;

    const float angle = endAngle - startAngle;
    const float degIncrease = angle / segmentCount;

    float currentAngle = startAngle;
    for (unsigned int i = 0; i <= segmentCount; i++)
    {
      glm::vec2 currentPosition(radius * sinf(currentAngle), radius * cosf(currentAngle));
      
      SRC->RenderBatch.Path.Points.push_back(center + currentPosition);
      currentAngle += degIncrease;
    }
  }

  R_API void srPathRectangle(const Rectangle& rect, const glm::vec2& origin, float rotation, float cornerRadius)
  {
    // We want fresh path, so nothing connects to the previus
    /*srEndPath();
    srBeginPath(PathType_Stroke);

    glm::vec2 topLeft;
    glm::vec2 topRight;
    glm::vec2 bottomLeft;
    glm::vec2 bottomRight;
      // Draw rect with simple corners

    

    float sin = rotation != 0 ? sinf(rotation*DEG2RAD) : 0;
    float cos = rotation != 0 ? cosf(rotation*DEG2RAD) : 1;
    if (rotation == 0.0f)
    {
      float x = rect.X - origin.x;
      float y = rect.Y - origin.y;
      topLeft = glm::vec2(x, y + rect.Height);
      topRight = glm::vec2(x + rect.Width, y + rect.Height);
      bottomLeft = glm::vec2(x, y);
      bottomRight = glm::vec2(x + rect.Width, y);
    }
    else 
    {

      float x = rect.X;
      float y = rect.Y;
      float dx = -origin.x;
      float dy = -origin.y;

      topLeft.x = x + dx*cos - (dy + rect.Height)*sin;
      topLeft.y = y + dx*sin + (dy + rect.Height)*cos;

      topRight.x = x + (dx + rect.Width)*cos - (dy + rect.Height)*sin;
      topRight.y = y + (dx + rect.Width)*sin + (dy + rect.Height)*cos;

      bottomLeft.x = x + dx*cos - dy*sin;
      bottomLeft.y = y + dx*sin + dy*cos;

      bottomRight.x = x + (dx + rect.Width)*cos - dy*sin;
      bottomRight.y = y + (dx + rect.Width)*sin + dy*cos;
    }


    if (cornerRadius > 0.0f)
    {
      // Start with top left (right of arc) corner
      cornerRadius = srClamp(cornerRadius, 0.0f, 1.0f);
      const float min_size = (srMin(rect.Width, rect.Height) / 2.0f) * cornerRadius;
      const glm::vec2 arcCenterX = glm::vec2(min_size * cos, min_size * sin);
      const glm::vec2 arcCenterY = glm::vec2(min_size * -sin, min_size * cos);

      srPathArc(topLeft + arcCenterX - arcCenterY, -90.0f - rotation, -rotation, min_size);
      srPathArc(topRight - arcCenterX - arcCenterY, 0.0f - rotation, 90.0f - rotation, min_size);
      srPathArc(bottomRight -arcCenterX + arcCenterY, 90.0f - rotation, 180.0f - rotation, min_size);
      srPathArc(bottomLeft + arcCenterX + arcCenterY, 180.0f - rotation, 270.0f - rotation, min_size);
    }
    else
    {
      srPathLineTo(topLeft);
      srPathLineTo(topRight);
      srPathLineTo(bottomRight);
      srPathLineTo(bottomLeft);
    }

    srEndPath();*/
  }

  R_API void srPathSetStrokeEnabled(bool showStroke)
  {
    if (showStroke) SRC->RenderBatch.Path.RenderType |= PathType_Stroke;
    else SRC->RenderBatch.Path.RenderType &= ~PathType_Stroke;
  }

  R_API void srPathSetFillEnabled(bool fill)
  {
    if (fill) SRC->RenderBatch.Path.RenderType |= PathType_Fill;
    else SRC->RenderBatch.Path.RenderType &= ~PathType_Fill;
  }

  R_API void srPathSetFillColor(Color color)
  {
    PathBuilder::PathStyleIndex& styleIndex = srPathBuilderNewStyle();
    styleIndex.second.FillColor = color;
    SRC->RenderBatch.Path.CurrentPathStyle.FillColor = color;
  }

  R_API void srPathSetFillColor(const glm::vec4& color)
  {
    srPathSetFillColor(srGetColorFromFloat(color));
  }

  R_API void srPathSetStrokeColor(Color color)
  {
    PathBuilder::PathStyleIndex& styleIndex = srPathBuilderNewStyle();
    styleIndex.second.StrokeColor = color;
    SRC->RenderBatch.Path.CurrentPathStyle.StrokeColor = color;
  }

  R_API void srPathSetStrokeColor(const glm::vec4& color)
  {
    srPathSetStrokeColor(srGetColorFromFloat(color));
  }



  R_API void srPathSetStrokeWidth(float width)
  {
    PathBuilder::PathStyleIndex& styleIndex = srPathBuilderNewStyle();
    styleIndex.second.StrokeWidth = width;
    SRC->RenderBatch.Path.CurrentPathStyle.StrokeWidth = width;
  }

  R_API void srPathSetStyle(const PathStyle& style)
  {
    PathBuilder::PathStyleIndex& styleIndex = srPathBuilderNewStyle();
    styleIndex.second = style;
    SRC->RenderBatch.Path.CurrentPathStyle = style;
  }

  R_API PathBuilder::PathStyleIndex& srPathBuilderNewStyle()
  {
    PathBuilder& pb = SRC->RenderBatch.Path;

    if (pb.Styles.size() == 0)
    {
      PathBuilder::PathStyleIndex newStyleIndex;
      newStyleIndex.first = pb.Points.size();
      newStyleIndex.second = pb.CurrentPathStyle;
      pb.Styles.push_back(newStyleIndex);
      pb.PreviousStyleVertexIndex = 0;
      return pb.Styles.back();
    }

    PathBuilder::PathStyleIndex& lastStyle = pb.Styles.back();

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
  R_API void srAddPolyline(const PathBuilder& pb, bool closedPath)
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
    srColor1c(currentStyle.StrokeColor);

    glm::vec2 lastTop {};
    glm::vec2 lastBottom {};

    for (size_t i1 = 0; i1 < count + (closedPath ? 1 : 0); i1++)
    {
      const glm::vec2& currentPoint = pb.Points[i1 % count];
      const glm::vec2& previousPoint = pb.Points[i1 == 0 ? (closedPath ? pb.Points.size() - 1 : 0) : (i1 - 1)];
      const glm::vec2& nextPoint = pb.Points[(i1 + 1) % count];

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
      if (i1 < count - 1 || closedPath)
      {
        // Get calc points
        const glm::vec2 cornerA = currentPoint + widthVector1;
        const glm::vec2 cornerB = currentPoint + widthVector2;

        // Check which point is pos and which negative
        const glm::vec2 center = cornerA + ((cornerB - cornerA) / 2.0f);


        const float connectionLength = glm::length(cornerB - cornerA);
        const float diagonalLength = glm::length(currentPoint - center);

        float a = ((connectionLength / 2.0f) / diagonalLength) * (currentStyle.StrokeWidth / 2.0f);
        //SR_TRACE("SF=%f, f=%f, a=%f", connectionLength, diagonalLength, a);
        float a_clamp = srMin(prevLength, nextLength);
        a = srClamp(a, -a_clamp, a_clamp);

        float dot = glm::dot(dir1, normalWidthVector2);
        float funny = dot/srAbs(dot);
        if (funny == 0)  funny = 1;
        currentConnectedTop += (dir1 * a) * funny;
        currentConnectedBottom -= (dir1 * a) * funny;
      }
      
      if (i1 == 0)
      {
        lastTop = currentConnectedTop; // We use widthVector2 because prev and current is the same point
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

      //SR_TRACE("Rendering QUAD index %d\nLT(%f, %f)\nCT(%f, %f)\nCB(%f, %f)\nLB(%f, %f)", i1, lastTop.x, lastTop.y, currentConnectedTop.x, currentConnectedTop.y, currentConnectedBottom.x, currentConnectedBottom.y, lastBottom.x, lastBottom.y);


      lastBottom = currentConnectedBottom;
      lastTop = currentConnectedTop;

      if (nextStyleChange <= i1)
      {
        currentStyleIndex++;
        if (currentStyleIndex < pb.Styles.size())
        {
          currentStyle = pb.Styles[currentStyleIndex].second;
          nextStyleChange += pb.Styles[currentStyleIndex].first;
          srColor1c(currentStyle.StrokeColor);
        }
      }
    }
    srEnd();
  }

  R_API void srAddPolyFilled(const PathBuilder& pb)
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
    srColor1c(currentStyle.FillColor);

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
          srColor1c(currentStyle.FillColor);
        }
      }

    }

    srEnd();
  }
}
