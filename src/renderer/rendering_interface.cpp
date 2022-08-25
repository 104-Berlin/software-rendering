#include "../pch.h"
#include "renderer.h"
#include "glad/glad.h"

sr::SRContext* sr::SRC = nullptr;

const char* basicMeshVertexShader = R"(
  #version 330 core

  layout(location = 0) in vec3 vPosition;
  layout(location = 1) in vec3 vNormal;
  layout(location = 2) in vec2 vTexCoord;

  void main()
  {
    gl_Position = vec4(vPosition, 1.0);
  }
)";

const char* basicMeshFragmentShader = R"(
  #version 330 core

  layout(location = 0) out vec4 fragColor;

  void main()
  {
    fragColor = vec4(1.0, 0.0, 0.0, 1.0);
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
    SR_TRACE("OpenGL-Context: %s", glGetString(GL_VERSION));
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

  R_API void srInitContext(SRContext* context)
  {
    if (context->DefaultShader.ID == 0)
    {
      SRC->DefaultShader = srLoadShader(basicMeshVertexShader, basicMeshFragmentShader);
    }
    context->RenderBatch = srLoadRenderBatch(5000);
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
    srUseShader(SRC->DefaultShader);


    if (!srBindVertexArray(mesh.VAO.VAO))
    {
      for (const auto& buffer : mesh.VAO.VBOs)
      {
        srBindVertexBuffer(buffer.ID);
        srSetVertexAttribute(buffer.Layout.Location, srGetVertexAttributeComponentCount(buffer.Layout.ElementType), srGetGLVertexAttribType(buffer.Layout.ElementType), buffer.Layout.Normalized, 0, 0);
        srEnableVertexAttribute(buffer.Layout.Location);
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
    vertexArray.VBOs.push_back({VertexArrayLayoutElement(EVertexAttributeType::FLOAT3, 0), vbo_position});

    if (mesh->Normals)
    {
      unsigned int vbo_normal = srLoadVertexBuffer(mesh->Normals, mesh->VertexCount * sizeof(glm::vec3));
      srEnableVertexAttribute(1);
      srSetVertexAttribute(1, 3, GL_FLOAT, GL_FALSE, 0, 0);
      vertexArray.VBOs.push_back({VertexArrayLayoutElement(EVertexAttributeType::FLOAT3, 1), vbo_normal});
    }

    if (mesh->TextureCoords0)
    {
      unsigned int vbo_textCoords = srLoadVertexBuffer(mesh->TextureCoords0, mesh->VertexCount * sizeof(glm::vec2));
      srEnableVertexAttribute(2);
      srSetVertexAttribute(2, 2, GL_FLOAT, GL_FALSE, 0, 0);
      vertexArray.VBOs.push_back({VertexArrayLayoutElement(EVertexAttributeType::FLOAT2, 2), vbo_textCoords});
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
    
    result.DrawBuffer.Vertices = new glm::vec3[bufferSize * 4];
    memset(result.DrawBuffer.Vertices, 0, bufferSize * 4 * sizeof(glm::vec3));
    result.DrawBuffer.Indices = new unsigned int[bufferSize * 6];
    memset(result.DrawBuffer.Indices, 0, bufferSize * 4 * sizeof(unsigned int));
    result.DrawBuffer.ElementCount = bufferSize * 4;

    VertexBuffers glBinding;
    glBinding.VAO = srLoadVertexArray();
    srBindVertexArray(glBinding.VAO);

    unsigned int vbo_position = srLoadVertexBuffer(result.DrawBuffer.Vertices, bufferSize * 4 * sizeof(glm::vec3));
    srEnableVertexAttribute(0);
    srSetVertexAttribute(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glBinding.VBOs.push_back({VertexArrayLayoutElement(EVertexAttributeType::FLOAT3, 0), vbo_position});

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
    srUseShader(SRC->DefaultShader);

    if (!srBindVertexArray(batch->DrawBuffer.GlBinding.VAO))
    {
      for (const auto& buffer : batch->DrawBuffer.GlBinding.VBOs)
      {
        srBindVertexBuffer(buffer.ID);
        srSetVertexAttribute(buffer.Layout.Location, srGetVertexAttributeComponentCount(buffer.Layout.ElementType), srGetGLVertexAttribType(buffer.Layout.ElementType), buffer.Layout.Normalized, 0, 0);
        srEnableVertexAttribute(buffer.Layout.Location);
      }
    }

    srBindVertexBuffer(batch->DrawBuffer.GlBinding.VBOs[0].ID);
    glCall(glBufferSubData(GL_ARRAY_BUFFER, 0, batch->VertexCounter * sizeof(glm::vec3), batch->DrawBuffer.Vertices));

    // Draw everything to current draw
    for (unsigned int i = 0, vertexOffset = 0; i <= batch->CurrentDraw; i++)
    {
      EBatchDrawMode mode = batch->DrawCalls[i].Mode;
      switch (mode)
      {
      case EBatchDrawMode::LINES:     glCall(glDrawArrays(GL_LINES, vertexOffset, batch->DrawCalls[i].VertexCount)); break;
      case EBatchDrawMode::TRIANGLES: glCall(glDrawArrays(GL_TRIANGLES, vertexOffset, batch->DrawCalls[i].VertexCount)); break;
      case EBatchDrawMode::QUADS:     SR_TRACE("QUADS NO IMPLEMENTED YET!"); break;
      }
      vertexOffset += batch->DrawCalls[i].VertexCount;
    }
  }



  R_API void srBegin(EBatchDrawMode mode)
  {
    RenderBatch& rb = SRC->RenderBatch;
    if (rb.DrawCalls[rb.CurrentDraw].Mode != mode)
    {
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

    SRC->RenderBatch.DrawBuffer.Vertices[SRC->RenderBatch.VertexCounter] = vertex;

    SRC->RenderBatch.VertexCounter++;
    SRC->RenderBatch.DrawCalls[SRC->RenderBatch.CurrentDraw].VertexCount++;
  }

  R_API void srEnd()
  {

  }




}
