#pragma once

#define SR_BATCH_DRAW_CALLS 256

namespace sr
{

    struct SRContext;

    extern R_API SRContext *SRC;

    // Basics
    typedef void *(*SRLoadProc)(const char *name);

    R_API void srLoad(SRLoadProc loadAddress);
    R_API void srTerminate();
    R_API void srInitGL();

    R_API void srInitContext(SRContext *context);
    R_API SRContext *srGetContext();

    R_API void srNewFrame(int frameWidth, int frameHeight, int windowWidth, int windowHeight);
    R_API void srEndFrame();

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

    // Opengl PolygonMode (Wireframe or not)
    enum PolygonFillMode_
    {
        PolygonFillMode_Line,
        PolygonFillMode_Fill
    };
    // Opengl PolygonMode
    R_API void srSetPolygonFillMode(PolygonFillMode_ mode);

    // Colors
    using Color = uint32_t;

    R_API Color srGetColorFromFloat(float r, float g, float b, float a = 1.0f);
    R_API Color srGetColorFromFloat(const glm::vec4 &color);
    /**
     * @brief Get color as vec4
     *
     * @param c Color to convert
     * @return glm::vec4 RGBA float color (range 0 - 1)
     */
    R_API glm::vec4 srGetFloatFromColor(Color c);

    // +++++++++++++++++++++++++++++++++++++++++++++++++
    // Math lib
    struct Rectangle
    {
        float OriginX;
        float OriginY;
        float Width;
        float Height;
    };

    struct RectangleCorners
    {
        glm::vec2 TopLeft; // If not rotated -x +y
        glm::vec2 TopRight;
        glm::vec2 BottomRight;
        glm::vec2 BottomLeft;
    };

    /**
     * @brief Takes a rectangle and rotates it around the origin
     *
     * @param rect      Rectangle to rotate
     * @param origin
     * @param rotation
     * @return R_API
     */
    R_API RectangleCorners srGetRotatedRectangle(const Rectangle &rect, float rotation);

    constexpr double PI = 3.14159265358979323846;

    constexpr double DEG2RAD = (PI / 180.0);
    constexpr double RAD2DEG = (180.0 / PI);

    template <typename T>
    T srAbs(const T &value)
    {
        return value < 0.0 ? -value : value;
    }

    template <typename T>
    T srMin(const T &a, const T &b)
    {
        return a < b ? a : b;
    }

    template <typename T>
    T srMax(const T &a, const T &b)
    {
        return a > b ? a : b;
    }

    template <typename T>
    T srClamp(const T &value, const T &min, const T &max)
    {
        return srMin(srMax(value, min), max);
    }

    // Shaders
    enum class EUniformLocation : int
    {
        MODEL_MATRIX = 0,
        VIEW_PROJECTION_MATRIX = 1,

        UNIFORM_MAX_SIZE
    };

    struct Shader
    {
        int ID;
        int *UniformLocations;
    };

    /**
     * @brief Creates and compiles vertex and fragment shader together
     *
     * @param vertex_source
     * @param fragment_source
     * @return Shader struct to shader id
     */
    R_API Shader srLoadShader(const char *vertex_source, const char *fragment_source);

    /**
     * @brief creates and compiles shader. check's for errors
     *
     * @param shader_type
     * @param shader_source
     * @return shader id -1 if completed incomplete
     */
    R_API int srCompileShader(int shader_type, const char *shader_source);

    R_API void srDeleteShader(int shader_type, unsigned int id);

    R_API void srUseShader(Shader shader);
    R_API unsigned int srShaderGetUniformLocation(const char *name, Shader shader, bool show_err = false);
    R_API void srShaderSetUniform1b(Shader shader, const char *name, bool value);
    R_API void srShaderSetUniform1i(Shader shader, const char *name, int value);
    R_API void srShaderSetUniform1f(Shader shader, const char *name, float value);
    R_API void srShaderSetUniform2f(Shader shader, const char *name, const glm::vec2 &value);
    R_API void srShaderSetUniform3f(Shader shader, const char *name, const glm::vec3 &value);
    R_API void srShaderSetUniformMat4(Shader shader, const char *name, const glm::mat4 &value);
    R_API void srSetDefaultShaderUniforms(Shader shader); // Binds the shader in this call as well

    // Textures

    struct Texture
    {
        unsigned int ID;
    };

    enum TextureFormat_
    {
        TextureFormat_RGBA8,
        TextureFormat_R8,
        TextureFormat_RGB8
    };

    inline size_t srTextureFormatSize(TextureFormat_ format)
    {
        switch (format)
        {
        case TextureFormat_R8:
            return 1;
        case TextureFormat_RGB8:
            return 3;
        case TextureFormat_RGBA8:
            return 4;
        }
        return 0;
    }

    R_API unsigned int srTextureFormatToGL(TextureFormat_ format);

    R_API Texture srLoadTexture(unsigned int width, unsigned int height, TextureFormat_ format);
    R_API Texture srLoadTextureFromFile(const char *path);
    R_API void srUnloadTexture(Texture *texture);
    R_API void srBindTexture(Texture texture);

    R_API void srTextureSetData(Texture texture, unsigned int width, unsigned int height, TextureFormat_ format, unsigned char *data);
    R_API void srTexturePrintData(Texture texutre, const unsigned int width, const unsigned int height, TextureFormat_ format);

    // Materials
    struct Material
    {
        Texture Texture0;
        Shader ShaderProgram;
    };

    R_API void srPushMaterial(const Material &mat);

    // Vertex Array
    // Used to store all buffers to be able to render
    enum class EVertexAttributeType
    {
        FLOAT,
        FLOAT2,
        FLOAT3,
        FLOAT4,
        INT,
        INT2,
        INT3,
        INT4,
        UINT,
        BYTE4,
        BOOL
    };

    R_API unsigned int srGetVertexAttributeComponentCount(EVertexAttributeType type);
    R_API unsigned int srGetVertexAttributeTypeSize(EVertexAttributeType type); // Return size in bits of type
    R_API unsigned int srGetGLVertexAttribType(EVertexAttributeType type);

    struct VertexArrayLayoutElement
    {
        EVertexAttributeType ElementType;
        unsigned int Location;
        bool Normalized;

        VertexArrayLayoutElement(EVertexAttributeType type, unsigned int location, bool normalized = false) : ElementType(type), Location(location), Normalized(normalized) {}
    };

    typedef std::vector<VertexArrayLayoutElement> VertexArrayLayout;

    R_API size_t srGetVertexLayoutSize(const VertexArrayLayout &layout);

    struct VertexBuffers
    {
        struct VertBuf
        {
            VertexArrayLayout Layout;
            unsigned int ID;
        };
        unsigned int VAO = 0;
        unsigned int IBO = 0;
        std::vector<VertBuf> VBOs;
    };

    struct Mesh
    {
        // Use srLoadMesh() To create a mesh. Will be auto released this way

        glm::vec3 *Vertices = NULL;
        glm::vec3 *Normals = NULL;
        glm::vec2 *TextureCoords0 = NULL;
        Color *Colors = NULL;

        unsigned int *Indices = NULL;

        unsigned int VertexCount = 0;  // Used for size of all vertex data
        unsigned int ElementCount = 0; // Used for size of all index data. (If 0 = no index data)

        VertexBuffers VAO; // Gets inittialized with stUploadMesh()
    };

    struct MeshInit
    {
        std::vector<glm::vec3> Vertices;
        std::vector<glm::vec3> Normals;
        std::vector<glm::vec2> TexCoord1;
        std::vector<Color> Colors;

        std::vector<unsigned int> Indices;
    };

    // Creates new vertex array. Returns 0 if vertex arrays are not supported (TODO)
    R_API unsigned int srLoadVertexArray();
    R_API void srUnloadVertexArray(unsigned int id);

    // Binds Vertex array if possible
    R_API bool srBindVertexArray(unsigned int id);

    R_API void srSetVertexAttribute(unsigned int location, unsigned int numElements, unsigned int type, bool normalized, int stride, const void *pointer);
    R_API void srEnableVertexAttribute(unsigned int location);

    // Creates vbo with static buffer
    R_API unsigned int srLoadVertexBuffer(void *data, size_t data_size);
    R_API unsigned int srLoadElementBuffer(void *data, size_t data_size);
    R_API void srUnloadBuffer(unsigned int id);

    R_API void srBindVertexBuffer(unsigned int id);
    R_API void srBindElementBuffer(unsigned int id);

    R_API Mesh srLoadMesh(const MeshInit &initData); // Loads mesh data to opengl. The data wont be copied to the mesh
    R_API void srDrawMesh(const Mesh &mesh);
    R_API void srUploadMesh(Mesh *mesh);
    R_API void srUnloadMesh(Mesh *mesh);
    R_API void srDeleteMeshCPUData(Mesh *mesh);

    R_API void srUnloadVertexBuffers(const VertexBuffers &vao);

    // Batch renderer
    // Using srBegin(mode) -> which begins new "geometry". it batches all calles with same mode togetcher
    // Using srEnd() -> closes down current "geometry". Increases depth to verify "rendering order"

    enum EBatchDrawMode
    {
        UNKNOWN = 0,
        TRIANGLES = 1,
        QUADS,
        LINES,
        POINTS
    };

    typedef uint32_t PathType;
    enum PathType_
    {
        PathType_Stroke = 1 << 0,
        PathType_Fill = 1 << 1
    };

    struct PathStyle
    {
        float StrokeWidth = 0.01f;
        Color StrokeColor = 0xffffffff;
        Color FillColor = 0xffffffff;
    };

    struct PathBuilder
    {
        std::vector<glm::vec2> Points;
        PathStyle CurrentPathStyle; // Not used right now. Can be removed
        PathType RenderType = 0;

        using PathStyleIndex = std::pair<unsigned int, PathStyle>;
        std::vector<PathStyleIndex> Styles;

        unsigned int PreviousStyleVertexIndex = 0;
    };

    struct RenderBatch
    {
        struct Vertex
        {
            glm::vec3 Pos = glm::vec3();
            glm::vec3 Normal = glm::vec3(0.0f, 0.0f, 1.0f);
            glm::vec2 UV = glm::vec2();
            Color Color1 = 0xffffffff;
            Color Color2 = 0x00000000;
        };

        struct Buffer
        {
            Vertex *Vertices = NULL; // Drawing buffer
            unsigned int *Indices = NULL;
            unsigned int ElementCount = 0;
            VertexBuffers GlBinding; // Rendering buffer
        };

        struct DrawCall
        {
            EBatchDrawMode Mode = EBatchDrawMode::UNKNOWN;
            Material Mat = {0};
            unsigned int VertexCount = 0;
            unsigned int VertexAlignment = 0; // Number for alining (LINE, TRIANGLES) to quads
        };

        Buffer DrawBuffer;
        DrawCall *DrawCalls; // size = SR_BATCH_DRAW_CALLS
        unsigned int CurrentDraw = 0;
        unsigned int VertexCounter = 0;

        double CurrentDepth = 0;

        glm::vec3 CurrentNormal;
        glm::vec2 CurrentTexCoord;
        Color CurrentColor1;
        Color CurrentColor2;

        // Path builder
        PathBuilder Path;
    };

    /**
     * @brief
     *
     * @param bufferSize The count of how many quads we can save (4 vertices, 6 indices)
     * @return srRenderBatch*
     */
    R_API RenderBatch srLoadRenderBatch(unsigned int bufferSize);

    R_API void srIncreaseRenderBatchCurrentDraw(RenderBatch *batch);
    R_API bool srCheckRenderBatchLimit(unsigned int numVerts); // Returns true if batch overflowed
    R_API void srDrawRenderBatch(RenderBatch *batch);

    R_API void srBegin(EBatchDrawMode mode);
    R_API void srVertex3f(float x, float y, float z);
    R_API void srVertex3f(const glm::vec3 &vertex);
    R_API void srVertex2f(float x, float y);
    R_API void srVertex2f(const glm::vec2 &vertex);
    R_API void srNormal3f(float x, float y, float z);
    R_API void srNormal3f(const glm::vec3 &normal);
    R_API void srColor11c(Color color);
    R_API void srColor13f(float r, float g, float b);
    R_API void srColor13f(const glm::vec3 &color);
    R_API void srColor14f(float r, float g, float b, float a);
    R_API void srColor14f(const glm::vec4 &color);
    R_API void srColor21c(Color color);
    R_API void srColor23f(float r, float g, float b);
    R_API void srColor23f(const glm::vec3 &color);
    R_API void srColor24f(float r, float g, float b, float a);
    R_API void srColor24f(const glm::vec4 &color);
    R_API void srTextureCoord2f(float u, float v);
    R_API void srTextureCoord2f(const glm::vec2 &uv);
    R_API void srEnd();

    // Path builder. Begin with srBeginPath(), and end with srEndPath(type). The type can be PathType_Stroke, PathType_Fill. You can also or them together to get stroke and fill

    R_API void srBeginPath(PathType type);
    R_API void srEndPath(bool closedPath = false);
    R_API void srPathLineTo(const glm::vec2 &position);
    R_API void srPathArc(const glm::vec2 &center, float startAngle, float endAngle, float radius, unsigned int segmentCount = 22);
    R_API void srPathEllipticalArc(const glm::vec2 &end_point, float angle, float radius_x, float radius_y, bool large_arc_flag, bool sweep_flag, unsigned int segmentCount = 22);
    R_API void srPathCubicBezierTo(const glm::vec2 &controll1, const glm::vec2 &controll2, const glm::vec2 &endPosition, unsigned int segmentCount = 22);
    R_API void srPathQuadraticBezierTo(const glm::vec2 &controll, const glm::vec2 &endPosition, unsigned int segmentCount = 22);

    R_API void srPathSetStrokeEnabled(bool showStroke);
    R_API void srPathSetFillEnabled(bool fill);
    R_API void srPathSetFillColor(Color color);
    R_API void srPathSetFillColor(const glm::vec4 &color);
    R_API void srPathSetStrokeColor(Color color);
    R_API void srPathSetStrokeColor(const glm::vec4 &color);
    R_API void srPathSetStrokeWidth(float width);
    R_API void srPathSetStyle(const PathStyle &style);

    R_API PathBuilder::PathStyleIndex &srPathBuilderNewStyle();

    // Flushing path
    R_API void srAddPolyline(const PathBuilder &pathBuilder, bool closedPath);
    R_API void srAddPolyFilled(const PathBuilder &pathBuilder);

    // More high level rendering methods

    R_API void srDrawRectanglePro(const glm::vec2 &position, const Rectangle &rect, float rotation, float cornerRadius, PathType pathType, PathStyle style);

    inline void srDrawRectangle(const glm::vec2 &position, const glm::vec2 &size, const glm::vec2 &origin, float thickness = 3.0f, Color color = 0xffffffff) { srDrawRectanglePro(position, {origin.x, origin.y, size.x, size.y}, 0.0f, 0.0f, PathType_Stroke, {thickness, color, 0xffffffff}); }
    inline void srDrawRectangleR(const glm::vec2 &position, const glm::vec2 &size, const glm::vec2 &origin, float rotation, float thickness = 3.0f, Color color = 0xffffffff) { srDrawRectanglePro(position, {origin.x, origin.y, size.x, size.y}, rotation, 0.0f, PathType_Stroke, {thickness, color, 0xffffffff}); }
    inline void srDrawRectangleC(const glm::vec2 &position, const glm::vec2 &size, const glm::vec2 &origin, float cornerRadius, float thickness = 3.0f, Color color = 0xffffffff) { srDrawRectanglePro(position, {origin.x, origin.y, size.x, size.y}, 0.0f, cornerRadius, PathType_Stroke, {thickness, color, 0xffffffff}); }
    inline void srDrawRectangleRC(const glm::vec2 &position, const glm::vec2 &size, const glm::vec2 &origin, float rotation, float cornerRadius, float thickness = 3.0f, Color color = 0xffffffff) { srDrawRectanglePro(position, {origin.x, origin.y, size.x, size.y}, rotation, cornerRadius, PathType_Stroke, {thickness, color, 0xffffffff}); }

    inline void srDrawRectangleFilled(const glm::vec2 &position, const glm::vec2 &size, const glm::vec2 &origin, Color color = 0xffffffff) { srDrawRectanglePro(position, {origin.x, origin.y, size.x, size.y}, 0.0f, 0.0f, PathType_Fill, {0.1f, 0xffffffff, color}); }
    inline void srDrawRectangleFilledR(const glm::vec2 &position, const glm::vec2 &size, const glm::vec2 &origin, float rotation, Color color = 0xffffffff) { srDrawRectanglePro(position, {origin.x, origin.y, size.x, size.y}, rotation, 0.0f, PathType_Fill, {0.1f, 0xffffffff, color}); }
    inline void srDrawRectangleFilledC(const glm::vec2 &position, const glm::vec2 &size, const glm::vec2 &origin, float cornerRadius, Color color = 0xffffffff) { srDrawRectanglePro(position, {origin.x, origin.y, size.x, size.y}, 0.0f, cornerRadius, PathType_Fill, {0.1f, 0xffffffff, color}); }
    inline void srDrawRectangleFilledRC(const glm::vec2 &position, const glm::vec2 &size, const glm::vec2 &origin, float rotation, float cornerRadius, Color color = 0xffffffff) { srDrawRectanglePro(position, {origin.x, origin.y, size.x, size.y}, rotation, cornerRadius, PathType_Fill, {0.1f, 0xffffffff, color}); }

    inline void srDrawRectangleFilledOutline(const glm::vec2 &position, const glm::vec2 &size, const glm::vec2 &origin, Color color = 0xffffffff) { srDrawRectanglePro(position, {origin.x, origin.y, size.x, size.y}, 0.0f, 0.0f, PathType_Fill | PathType_Stroke, {0.1f, 0xffffffff, color}); }
    inline void srDrawRectangleFilledOutlineR(const glm::vec2 &position, const glm::vec2 &size, const glm::vec2 &origin, float rotation, Color color = 0xffffffff) { srDrawRectanglePro(position, {origin.x, origin.y, size.x, size.y}, rotation, 0.0f, PathType_Fill | PathType_Stroke, {0.1f, 0xffffffff, color}); }
    inline void srDrawRectangleFilledOutlineC(const glm::vec2 &position, const glm::vec2 &size, const glm::vec2 &origin, float cornerRadius, Color color = 0xffffffff) { srDrawRectanglePro(position, {origin.x, origin.y, size.x, size.y}, 0.0f, cornerRadius, PathType_Fill | PathType_Stroke, {0.1f, 0xffffffff, color}); }
    inline void srDrawRectangleFilledOutlineRC(const glm::vec2 &position, const glm::vec2 &size, const glm::vec2 &origin, float rotation, float cornerRadius, Color color = 0xffffffff) { srDrawRectanglePro(position, {origin.x, origin.y, size.x, size.y}, rotation, cornerRadius, PathType_Fill | PathType_Stroke, {0.1f, 0xffffffff, color}); }

    R_API void srDrawTexturePro(Texture texture, const glm::vec2 &position, const Rectangle &rect, float rotation);

    R_API void srDrawGrid(const glm::vec2 &position, unsigned int columns, unsigned int rows, float cellSizeX, float cellSizeY);

    // Fonts

    struct FontGlyph
    {
        glm::ivec2 Size;
        glm::ivec2 Offset;
        int advance;
        float u0;
        float v0;
        float u1;
        float v1;
        unsigned int CharCode;
    };
    struct FontTexture
    {
        Texture Image;
        glm::ivec2 Size;
        uint8_t *ImageData;
        void *ShelfPack; // For packing the glyphs
        std::unordered_map<unsigned int, FontGlyph> CharMap;
    };

    struct Font
    {
        int Size;
        int LineHeight;
        int LineTop;
        int LineBottom;
        FT_Face Face;
        FontTexture Texture;
    };

    typedef unsigned int FontHandle;

    R_API FontHandle srLoadFont(const char *filePath, unsigned int size = 24);
    R_API void srUnloadFont(FontHandle font);

    R_API int srFontGetTextWidth(FontHandle font, const char *text);
    R_API int srFontGetTextHeight(FontHandle font, const char *text);
    R_API glm::ivec2 srFontGetTextSize(FontHandle font, const char *text);
    R_API int srFontGetLineTop(FontHandle font);
    R_API int srFontGetLineBottom(FontHandle font);
    R_API int srFontGetLineHeight(FontHandle font);

    R_API int srFontGetSize(FontHandle font);
    R_API const char *srFontGetName(FontHandle font);

    R_API unsigned int srFontGetTextureId(FontHandle font);

    R_API void srDrawText(FontHandle font, const char *text, const glm::vec2 &position, Color color = 0xff000000, float outline_thickness = 0.0f, Color outline_color = 0xff000000);

    R_API void srDrawCircle(const glm::vec2 &center, float radius, Color color = 0xffffffff, unsigned int segmentCount = 36);
    R_API void srDrawCircleOutline(const glm::vec2 &center, float radius, float thickness, Color color = 0xffffffff, unsigned int segmentCount = 36);
    R_API void srDrawArc(const glm::vec2 &center, float startAngle, float endAngle, float radius, Color color = 0xffffffff, unsigned int segmentCount = 22);

    struct SRContext
    {
        RenderBatch MainRenderBatch;
        Shader DefaultShader;
        Shader DistanceFieldShader;
        std::vector<Mesh> AutoReleaseMeshes;
        glm::mat4 CurrentProjection;
        FT_Library Libary;
        std::unordered_map<FontHandle, Font> LoadedFonts;
    };

}
