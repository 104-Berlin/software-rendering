#include "src/pch.h"
#include "glad/glad.h"
#include <SDL.h>

#include "src/renderer/renderer.h"

#include "imgui.h"
#include "backends/imgui_impl_sdl.h"
#include "backends/imgui_impl_opengl3.h"

int draw_width, draw_height, window_width, window_height;
static glm::vec2 MousePosition{};
static std::vector<glm::vec2> EditPath;
static int EditGrabIndex = -1;
static bool ControllPressed = false;

void drawVector(const glm::vec2 &vector, const glm::vec2 &offset = glm::vec2(0.0f))
{
    sr::srColor14f(0.3f, 0.3f, 0.3f, 1.0f);
    sr::srVertex2f(offset);

    sr::srColor14f(0.8f, 0.3f, 0.3f, 1.0f);
    sr::srVertex2f(vector + offset);
}

/*
void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    if (action == GLFW_PRESS)
    {
        if (key == GLFW_KEY_LEFT_CONTROL)
        {
            ControllPressed = true;
            SR_TRACE("Pressing Controll");
        }
    }
    else if (action == GLFW_RELEASE)
    {
        if (key == GLFW_KEY_LEFT_CONTROL)
        {
            ControllPressed = false;
        }
    }
}

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    if (action == GLFW_PRESS)
    {
        if (button == 0 && ControllPressed)
        {
            EditGrabIndex = EditPath.size();
            EditPath.push_back(MousePosition);
            SR_TRACE("Inserting Point (%f, %f)", MousePosition.x, MousePosition.y);
        }
    }
    else if (action == GLFW_RELEASE)
    {
        if (button == 0)
        {
            EditGrabIndex = -1;
        }
    }
}

void mousePositionCallback(GLFWwindow* window, double xpos, double ypos)
{
    MousePosition.x = xpos - frameWidth / 2;
    MousePosition.y = frameHeight / 2 - ypos;

    if (EditGrabIndex > -1)
    {
        EditPath[EditGrabIndex] = MousePosition;
    }
}*/

int main(int argc, char *argv[])
{
    SR_TRACE("19 mod 9 = %d", 19 % 9);
    SR_TRACE("19 mod 4 = %d", 19 % 4);
    SR_TRACE("19 mod 2 = %d", 19 % 2);
    SR_TRACE("19 mod 5 = %d", 19 % 5);
    SR_TRACE("19 mod 8 = %d", 19 % 8);

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_GAMECONTROLLER) != 0)
    {
        printf("Error: %s\n", SDL_GetError());
        return -1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_FORWARD_COMPATIBLE_FLAG); // Always required on Mac
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 2);

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLEBUFFERS, 1);
    SDL_GL_SetAttribute(SDL_GL_MULTISAMPLESAMPLES, 8);

    SDL_WindowFlags window_flags = (SDL_WindowFlags)(SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    SDL_Window *window = SDL_CreateWindow("Software Rendering", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 1280, 720, window_flags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1);
    sr::srLoad((sr::SRLoadProc)SDL_GL_GetProcAddress);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO &io = ImGui::GetIO();
    ImGui::StyleColorsDark();

    ImGui_ImplSDL2_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init();

    sr::MeshInit initData;
    initData.Vertices = {
        {-0.5f, -0.5f, 0.0f},
        {0.0f, 0.5f, 0.0f},
        {0.5f, -0.5f, 0.0f}};
    initData.Colors = {
        0xffff00ff,
        0xffff00ff,
        0xffff00ff};
    initData.Indices = {
        0, 1, 2};

    sr::Mesh mesh = sr::srLoadMesh(initData);

    // 4 * 4 big texture
    uint32_t data[] = {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
                       0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
                       0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
                       0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};

    /*sr::Texture texture = sr::srLoadTexture(4, 4, sr::TextureFormat_RGBA8);
    sr::srTextureSetData(texture, 4, 4, sr::TextureFormat_RGBA8, (unsigned char*)data);

    sr::srTexturePrintData(texture, 4, 4, sr::TextureFormat_RGBA8);*/
    // sr::Texture texture = sr::srLoadTextureFromFile("G:\\repos\\software-rendering\\texture.png");
    sr::FontHandle font = sr::srLoadFont("/Users/lucaherzke/Documents/DEV/software-rendering/Roboto.ttf", 24);

    float ddpi = 0.0f;
    float hdpi = 0.0f;
    float vdpi = 0.0f;

    SDL_GetDisplayDPI(0, &ddpi, &hdpi, &vdpi);

    SR_TRACE("DPI: %f, %f, %f", ddpi, hdpi, vdpi);

    float displayAngle = 0;

    bool drawGrid = false;

    bool drawLines = false;

    bool drawRect = false;
    bool drawRectLine = false;
    float rectRotation = 0.0f;
    float cornerRadius = 0.0f;
    float lineWidth = 5.0f;
    glm::vec2 rectSize = {100.0f, 100.0f};
    glm::vec4 currentMeshColor(1.0f, 1.0f, 1.0f, 1.0f);
    glm::vec4 currentLineColor(1.0f, 0.0f, 0.0f, 1.0f);

    char text[255] = "";
    float glyph_center = 0.5f;
    float smoothing = 0.04f;
    float glyph_outline_width = 0.0f;

    bool drawArcs = false;
    int numArcSegments = 5;
    glm::vec2 end_point = {100.0f, 0.0f};
    float angle = 0.0f;
    float radius_x = 100.0f;
    float radius_y = 100.0f;
    bool large_arc_flag = false;
    bool sweep_flag = false;

    // Bezier Curve
    bool drawBezier = false;
    int numBezierSegments = 5;
    glm::vec2 bezier_controll_point = {0.0f, 100.0f};
    glm::vec2 bezier_end_point = {100.0f, 0.0f};

    // Bezier Curve
    bool drawBezierCube = false;
    int numBezierCubeSegments = 5;
    glm::vec2 beziercube_controll_point_1 = {0.0f, 100.0f};
    glm::vec2 beziercube_controll_point_2 = {100.0f, 100.0f};
    glm::vec2 beziercube_end_point = {100.0f, 0.0f};

    bool done = false;
    while (!done)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL2_ProcessEvent(&event);
            if (event.type == SDL_QUIT)
                done = true;
            if (event.type == SDL_WINDOWEVENT && event.window.event == SDL_WINDOWEVENT_CLOSE && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        // ImGui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL2_NewFrame();
        ImGui::NewFrame();

        // ImGui calls
        ImGui::Begin("Settings");
        if (ImGui::Checkbox("Wireframe", &drawLines))
        {
            sr::srSetPolygonFillMode(drawLines ? sr::PolygonFillMode_Line : sr::PolygonFillMode_Fill);
        }

        ImGui::Separator();

        ImGui::Checkbox("Draw Grid", &drawGrid);

        if (ImGui::CollapsingHeader("Rect"))
        {
            ImGui::Checkbox("Draw Rect", &drawRect);
            ImGui::Checkbox("Draw Rect lines", &drawRectLine);
            ImGui::SliderFloat("Rect Rotation", &rectRotation, -360.0f, 360.0f);
            ImGui::SliderFloat("Corner Radius", &cornerRadius, 0.0f, 1.0f);
            ImGui::SliderFloat("Line Width", &lineWidth, 1.0f, 100.0f);
            ImGui::SliderFloat2("Rect Size", glm::value_ptr(rectSize), 0.1f, 500.0f);
            ImGui::ColorEdit4("LineColor", glm::value_ptr(currentLineColor));
            ImGui::ColorEdit4("CurrentColor", glm::value_ptr(currentMeshColor));
        }
        if (ImGui::CollapsingHeader("Arcs"))
        {
            ImGui::Checkbox("Draw arcs", &drawArcs);

            ImGui::DragFloat2("End Point", glm::value_ptr(end_point), 0.1f, -1000.0f, 1000.0f);
            ImGui::DragFloat("Angle", &angle, 0.1f, -360.0f, 360.0f);
            ImGui::DragFloat("Radius X", &radius_x, 0.1f, 0.0f, 500.0f);
            ImGui::DragFloat("Radius Y", &radius_y, 0.1f, 0.0f, 500.0f);
            ImGui::Checkbox("Large Arc Flag", &large_arc_flag);
            ImGui::Checkbox("Sweep Flag", &sweep_flag);

            ImGui::SliderInt("Arc segments", &numArcSegments, 5, 75);

            ImGui::Text("Current Angle %f", displayAngle);
        }
        if (ImGui::CollapsingHeader("Bezier"))
        {
            ImGui::Checkbox("Draw Curve", &drawBezier);

            ImGui::DragFloat2("Controll Point", glm::value_ptr(bezier_controll_point), 0.1f, -1000.0f, 1000.0f);
            ImGui::DragFloat2("End Point", glm::value_ptr(bezier_end_point), 0.1f, -1000.0f, 1000.0f);

            ImGui::SliderInt("Bezier segments", &numBezierSegments, 5, 75);
        }

        if (ImGui::CollapsingHeader("Bezier Cube"))
        {
            ImGui::Checkbox("Draw Cube Curve", &drawBezierCube);

            ImGui::DragFloat2("Cube Controll Point 1", glm::value_ptr(beziercube_controll_point_1), 0.1f, -1000.0f, 1000.0f);
            ImGui::DragFloat2("Cube Controll Point 2", glm::value_ptr(beziercube_controll_point_2), 0.1f, -1000.0f, 1000.0f);
            ImGui::DragFloat2("Cube End Point", glm::value_ptr(beziercube_end_point), 0.1f, -1000.0f, 1000.0f);

            ImGui::SliderInt("Cube Bezier segments", &numBezierCubeSegments, 5, 75);
        }
        if (ImGui::CollapsingHeader("Text"))
        {
            ImGui::InputTextMultiline("Content", text, 255);

            ImGui::DragFloat("Glyph Center", &glyph_center, 0.001f, 0.0f, 1.0f);
            ImGui::DragFloat("Smoothing", &smoothing, 0.001f, 0.0f, 1.0f);

            ImGui::DragFloat("Outline Width", &glyph_outline_width, 0.001f, 0.0f, 0.5f);
            ImGui::Image((ImTextureID)(unsigned long long)sr::srFontGetTextureId(font), ImGui::GetContentRegionAvail());
        }
        ImGui::End();

        ImGui::Render();

        // SR Rendering
        SDL_GL_GetDrawableSize(window, &draw_width, &draw_height);
        SDL_GetWindowSize(window, &window_width, &window_height);

        glm::vec2 half_size = glm::vec2(window_width, window_height) / 2.0f;

        // Start new frame for rendering
        sr::srNewFrame(draw_width, draw_height, window_width, window_height);

        // Update text shader uniforms for testing
        sr::srUseShader(sr::srGetContext()->DistanceFieldShader);
        sr::srShaderSetUniform1f(sr::srGetContext()->DistanceFieldShader, "glyph_center", glyph_center);
        sr::srShaderSetUniform1f(sr::srGetContext()->DistanceFieldShader, "smoothing", smoothing);

        const glm::vec2 &halfRectSize = rectSize / 2.0f;

        sr::srBeginPath(sr::PathType_Stroke);
        sr::srPathSetStrokeWidth(lineWidth);
        sr::srPathSetStrokeColor(currentMeshColor);
        for (const glm::vec2 &point : EditPath)
        {
            sr::srPathLineTo(point);
        }
        sr::srEndPath();

        /*sr::srBegin(sr::EBatchDrawMode::LINES);
        sr::srVertex2f(0.0f, 0.0f);
        sr::srVertex2f(200.0f, 200.0f);
        sr::srEnd();*/

        if (drawGrid)
        {
            sr::srDrawGrid({0.0f, 0.0f}, 10, 10, 100, 100);
        }
        // sr::srDrawText(font, "0, 0", {0.0f, 0.0f});

        // sr::srDrawText(font, "Wer das liest ist doof\nasdgfHüöo", glm::vec2(0.0f, 0.0f), sr::srGetColorFromFloat(currentLineColor), drawLines);

        // sr::srDrawRectangle(sr::Rectangle{0.0f, 0.0f, 1.0f, 1.0f}, glm::vec2{0.5f, 0.5f}, rectRotation, sr::srGetColorFromFloat(currentColor.x, currentColor.y, currentColor.z, currentColor.w));

        // sr::srDrawCircle(glm::vec2(0.0f, 0.0f), 0.2f, sr::srGetColorFromFloat(currentColor));

        // sr::srPathSetStrokeEnabled(false);
        // sr::srPathSetFillEnabled(true);
        //
        // sr::srPathSetStrokeColor(currentLineColor);
        // sr::srPathSetFillColor(currentMeshColor);
        // sr::srPathRectangle({0.0f, 0.0f, rectSize.x, rectSize.y}, rectSize / 2.0f, 0.0f, 0.0f);

        if (drawRect)
        {
            sr::srEnableScissor(half_size.x - 100.0f, half_size.y - 100.0f, 200.0f, 200.0f);
            sr::srDrawRectangleFilledRC(half_size, {rectSize.x, rectSize.y}, rectSize / 2.0f, rectRotation, cornerRadius, sr::srGetColorFromFloat(currentMeshColor));
            sr::srDrawRectangleRC(half_size, {rectSize.x, rectSize.y}, rectSize / 2.0f, rectRotation, cornerRadius, lineWidth, sr::srGetColorFromFloat(currentLineColor));
            sr::srDisableScissor();
        }

        if (drawArcs)
        {
            sr::srDrawCircle(half_size, 5.0f, 0xff0000ff);
            sr::srDrawCircle(half_size + end_point, 5.0f, 0xff0000ff);

            sr::srBeginPath(sr::PathType_Stroke);
            sr::srPathSetStrokeWidth(4.0f);
            sr::srPathSetStrokeColor(currentLineColor);
            sr::srPathLineTo(half_size);
            //  sr::srPathLineTo(half_size + glm::vec2(100.0f, 0.0f));

            sr::srPathEllipticalArc(half_size + end_point, angle, radius_x, radius_y, large_arc_flag, sweep_flag, numArcSegments);
            // sr::srPathArc(half_size, 0.0f, 90.0f, 250, numArcSegments);
            //    sr::srPathArc(half_size, 90.0f, 180.0f, 250, numArcSegments);
            sr::srEndPath();
        }

        if (drawBezier)
        {
            sr::srDrawCircle(half_size, 5.0f, 0xff0000ff);
            sr::srDrawCircle(half_size + bezier_controll_point, 5.0f, 0xff0000ff);
            sr::srDrawCircle(half_size + bezier_end_point, 5.0f, 0xff0000ff);

            sr::srBeginPath(sr::PathType_Stroke);
            sr::srPathSetStrokeWidth(4.0f);
            sr::srPathSetStrokeColor(currentLineColor);
            sr::srPathLineTo(half_size);
            sr::srPathQuadraticBezierTo(half_size + bezier_controll_point, half_size + bezier_end_point, numBezierSegments);
            sr::srEndPath();
        }

        if (drawBezierCube)
        {
            sr::srDrawCircle(half_size, 5.0f, 0xff0000ff);
            sr::srDrawCircle(half_size + beziercube_controll_point_1, 5.0f, 0xff0000ff);
            sr::srDrawCircle(half_size + beziercube_controll_point_2, 5.0f, 0xff0000ff);
            sr::srDrawCircle(half_size + beziercube_end_point, 5.0f, 0xff0000ff);

            sr::srBeginPath(sr::PathType_Stroke);
            sr::srPathSetStrokeWidth(4.0f);
            sr::srPathSetStrokeColor(currentLineColor);
            sr::srPathLineTo(half_size);
            sr::srPathCubicBezierTo(half_size + beziercube_controll_point_1, half_size + beziercube_controll_point_2, half_size + beziercube_end_point, numBezierCubeSegments);
            sr::srEndPath();
        }

        if (strlen(text) > 0)
        {
            sr::srDrawRectangle(half_size - glm::vec2(0.0f, sr::srFontGetLineTop(font)), sr::srFontGetTextSize(font, text), {0.0f, 0.0f});
            sr::srDrawText(font, text, {half_size.x, half_size.y}, sr::srGetColorFromFloat(currentLineColor), glyph_outline_width);
        }

        /*sr::srBeginPath(sr::PathType_Stroke);
        sr::srPathSetStrokeWidth(lineWidth);
        sr::srPathSetStrokeColor(currentLineColor);
        sr::srPathLineTo(point1);
        sr::srPathLineTo(point2);
        sr::srPathLineTo(point3);
        sr::srEndPath();

        sr::srDrawCircle(point1, 5.0f, 0xff0000ff);
        sr::srDrawCircle(point2, 5.0f, 0xff00ff00);
        sr::srDrawCircle(point3, 5.0f, 0xffff0000);

        sr::srBegin(sr::EBatchDrawMode::LINES);
        sr::srColor1c(0xff000000);
        sr::srVertex2f(point1);
        sr::srVertex2f(point2);

        sr::srVertex2f(point2);
        sr::srVertex2f(point3);
        sr::srEnd();*/

        // sr::srDrawMesh(mesh);

        sr::srEndFrame();

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL2_Shutdown();

    ImGui::DestroyContext();
    sr::srTerminate();

    SDL_GL_DeleteContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}