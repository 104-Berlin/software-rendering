#pragma once


#define SR_BATCH_DRAW_CALLS 256


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


    struct VertexBuffers {
        struct VertBuf {
            VertexArrayLayoutElement Layout;
            unsigned int ID;
        };
        unsigned int                VAO = 0;
        unsigned int                IBO = 0;
        std::vector<VertBuf>        VBOs;
    };

    struct Mesh {
        // Use srLoadMesh() To create a mesh. Will be auto released this way

        glm::vec3* Vertices = NULL;
        glm::vec3* Normals = NULL;
        glm::vec2* TextureCoords0 = NULL;

        unsigned int* Indices = NULL;

        unsigned int VertexCount = 0;   // Used for size of all vertex data
        unsigned int ElementCount = 0;  // Used for size of all index data. (If 0 = no index data)

        VertexBuffers VAO;    // Gets inittialized with stUploadMesh()
    };

    struct MeshInit {
        std::vector<glm::vec3> Vertices;
        std::vector<glm::vec3> Normals;
        std::vector<glm::vec2> TexCoord1;

        std::vector<unsigned int> Indices;
    };


    // Creates new vertex array. Returns 0 if vertex arrays are not supported (TODO)
    R_API unsigned int srLoadVertexArray();
    R_API void srUnloadVertexArray(unsigned int id);

    // Binds Vertex array if possible
    R_API bool srBindVertexArray(unsigned int id);


    R_API void srSetVertexAttribute(unsigned int location, unsigned int numElements, unsigned int type, bool normalized, int stride, const void* pointer);
    R_API void srEnableVertexAttribute(unsigned int location);


    // Creates vbo with static buffer
    R_API unsigned int srLoadVertexBuffer(void* data, size_t data_size);
    R_API unsigned int srLoadElementBuffer(void* data, size_t data_size);
    R_API void srUnloadBuffer(unsigned int id);

    R_API void srBindVertexBuffer(unsigned int id);
    R_API void srBindElementBuffer(unsigned int id);

    
    R_API Mesh srLoadMesh(const MeshInit& initData); // Loads mesh data to opengl. The data wont be copied to the mesh
    R_API void srDrawMesh(const Mesh& mesh);
    R_API void srUploadMesh(Mesh* mesh);
    R_API void srUnloadMesh(Mesh* mesh);
    R_API void srDeleteMeshCPUData(Mesh* mesh);
 
    R_API void srUnloadVertexBuffers(const VertexBuffers& vao);



    // Batch renderer
    // Using srBegin(mode) -> which begins new "geometry". it batches all calles with same mode togetcher
    // Using srEnd() -> closes down current "geometry". Increases depth to verify "rendering order"


    enum EBatchDrawMode
    {
        UNKNOWN = 0,
        TRIANGLES = 1,
        QUADS,
        LINES
    };

    struct RenderBatch
    {
        struct Buffer
        {
            glm::vec3*      Vertices = NULL;   // Drawing buffer
            unsigned int*   Indices = NULL;    
            unsigned int    ElementCount = 0;
            VertexBuffers   GlBinding;    // Rendering buffer
        };

        struct DrawCall
        {
            EBatchDrawMode  Mode = EBatchDrawMode::UNKNOWN;
            unsigned int    VertexCount = 0;
            unsigned int    VertexAlignment = 0; // Number for alining (LINE, TRIANGLES) to quads
        };

        Buffer          DrawBuffer;
        DrawCall*       DrawCalls;       // size = SR_BATCH_DRAW_CALLS
        unsigned int    CurrentDraw = 0;
        unsigned int    VertexCounter = 0;
    };

    /**
     * @brief 
     * 
     * @param bufferSize The count of how many quads we can save (4 vertices, 6 indices)
     * @return srRenderBatch* 
     */
    R_API RenderBatch srLoadRenderBatch(unsigned int bufferSize);

    R_API void srIncreaseRenderBatchCurrentDraw(RenderBatch* batch);
    R_API bool srCheckRenderBatchLimit(unsigned int numVerts); // Returns true if batch overflowed
    R_API void srDrawRenderBatch(RenderBatch* batch);

    R_API void srBegin(EBatchDrawMode mode);
    R_API void srVertex3f(float x, float y, float z);
    R_API void srVertex3f(const glm::vec3& vertex);
    R_API void srEnd();
    

    struct SRContext
    {
        RenderBatch RenderBatch;
        Shader DefaultShader;
        std::vector<Mesh> AutoReleaseMeshes;
    };



}