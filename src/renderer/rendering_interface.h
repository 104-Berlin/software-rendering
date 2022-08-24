#pragma once

namespace sr {

    struct SRContext;

    extern R_API SRContext* SRC;

    // Basics
    typedef void* (* SRLoadProc)(const char *name);

    R_API void srLoad(SRLoadProc loadAddress);

    R_API void srTerminate();

    R_API void srInitContext(SRContext* context);

    /**
     * @brief Clears framebuffer
     * 
     * @param mask Clear color mask
     */
    R_API void srClear(int mask);

    /**
     * @brief Sets clear color for next Clear calls
     * 
     * @param r Red
     * @param g Green
     * @param b Blue
     * @param a Alpha
     */
    R_API void srClearColor(float r, float g, float b, float a);

    R_API void srViewport(float x, float y, float width, float height);

    // Shaders
    enum class EUniformLocation : int {
        MODEL_MATRIX            = 0,
        VIEW_PROJECTION_MATRIX  = 1,

        UNIFORM_MAX_SIZE
    };

    struct Shader {
        int ID;
        int* UniformLocations;
    };

    /**
     * @brief Creates and compiles vertex and fragment shader together
     * 
     * @param vertex_source 
     * @param fragment_source 
     * @return Shader struct to shader id
     */
    R_API Shader srLoadShader(const char* vertex_source, const char* fragment_source);

    /**
     * @brief creates and compiles shader. check's for errors
     * 
     * @param shader_type 
     * @param shader_source 
     * @return shader id -1 if completed incomplete
     */
    R_API int srCompileShader(int shader_type, const char* shader_source);
    

    R_API void srDeleteShader(int shader_type, unsigned int id);

    R_API void srUseShader(Shader shader);


    // Vertex Array
    // Used to store all buffers to be able to render
    enum class EVertexAttributeType {
        FLOAT, FLOAT2, FLOAT3, FLOAT4,
        INT, INT2, INT3, INT4, UINT, 
        BYTE4, 
        BOOL
    };

    R_API unsigned int srGetVertexAttributeComponentCount(EVertexAttributeType type);
    R_API unsigned int srGetGLVertexAttribType(EVertexAttributeType type);

    struct VertexArrayLayoutElement {
        EVertexAttributeType    ElementType;
        unsigned int            Location;
        bool                    Normalized;

        VertexArrayLayoutElement(EVertexAttributeType type, unsigned int location, bool normalized = false) : ElementType(type), Location(location), Normalized(normalized) {}
    };

    typedef std::vector<VertexArrayLayoutElement> VertexArrayLayout;


    struct VertexArray {
        struct VertBuf {
            VertexArrayLayoutElement Layout;
            unsigned int ID;
        };
        unsigned int                VAO = 0;
        unsigned int                IBO = 0;
        std::vector<VertBuf>        VBOs;
    };

    struct Mesh {
        std::vector<glm::vec3> Vertices;
        std::vector<glm::vec3> Normals;
        std::vector<glm::vec2> TextureCoords0;

        std::vector<unsigned int> Indices;

        VertexArray VAO;
    };

    // Creates new vertex array. Returns 0 if vertex arrays are not supported (TODO)
    R_API unsigned int srLoadVertexArray();

    // Binds Vertex array if possible
    R_API bool srBindVertexArray(unsigned int id);


    R_API void srSetVertexAttribute(unsigned int location, unsigned int numElements, unsigned int type, bool normalized, int stride, const void* pointer);
    R_API void srEnableVertexAttribute(unsigned int location);


    // Creates vbo with static buffer
    R_API unsigned int srLoadVertexBuffer(void* data, size_t data_size);
    R_API unsigned int srLoadElementBuffer(void* data, size_t data_size);

    R_API void srBindVertexBuffer(unsigned int id);
    R_API void srBindElementBuffer(unsigned int id);

    R_API void srDrawMesh(Mesh mesh);

    R_API void srUploadMesh(Mesh* mesh);
 



    struct SRContext
    {
        Shader DefaultShader;
    };



}