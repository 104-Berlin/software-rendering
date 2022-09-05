#include "src/pch.h"
#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "src/renderer/renderer.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

void drawVector(const glm::vec2& vector, const glm::vec2& offset = glm::vec2(0.0f))
{
    sr::srColor4f(0.3f, 0.3f, 0.3f, 1.0f);
    sr::srVertex2f(offset);

    sr::srColor4f(0.8f, 0.3f, 0.3f, 1.0f);
    sr::srVertex2f(vector + offset);
}

int main()
{
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac
    glfwWindowHint(GLFW_SAMPLES, 8);

    GLFWwindow* window = glfwCreateWindow(800, 800, "Rendering", NULL, NULL);

    glfwMakeContextCurrent(window);
    sr::srLoad((sr::SRLoadProc)glfwGetProcAddress);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init();

    sr::MeshInit initData;
    initData.Vertices = {
        {-0.5f, -0.5f, 0.0f},
        { 0.0f,  0.5f, 0.0f},
        { 0.5f, -0.5f, 0.0f}
    };
    initData.Colors = {
        0xffff00ff,
        0xffff00ff,
        0xffff00ff
    };
    initData.Indices = {
        0, 1, 2
    };

    sr::Mesh mesh = sr::srLoadMesh(initData);

    // 4 * 4 big texture
    uint32_t data[] =      {0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
                            0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
                            0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff,
                            0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff};

    /*sr::Texture texture = sr::srLoadTexture(4, 4, sr::TextureFormat_RGBA8);
    sr::srTextureSetData(texture, 4, 4, sr::TextureFormat_RGBA8, (unsigned char*)data);

    sr::srTexturePrintData(texture, 4, 4, sr::TextureFormat_RGBA8);*/
    sr::Texture texture = sr::srLoadTextureFromFile("G:\\repos\\software-rendering\\texture.png");
    sr::Font font = sr::srLoadFont("G:\\repos\\software-rendering\\Pacifico-Regular.ttf", 96);

    float displayAngle = 0;

    bool drawLines = false;
    float rectRotation = 0.0f;
    float cornerRadius = 0.0f;
    float lineWidth = 5.0f;
    glm::vec2 rectSize = {100.0f, 100.0f};
    glm::vec4 currentMeshColor(1.0f, 1.0f, 1.0f, 1.0f);
    glm::vec4 currentLineColor(1.0f, 0.0f, 0.0f, 1.0f);

    glm::vec2 point1(0, 0);
    glm::vec2 point2( 100,  0);
    glm::vec2 point3( 100, -200);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        

        // ImGui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ImGui calls
        ImGui::Begin("Settings");
        if (ImGui::Checkbox("Wireframe", &drawLines))
        {
            sr::srSetPolygonFillMode(drawLines ? sr::PolygonFillMode_Line : sr::PolygonFillMode_Fill);
        }

        ImGui::SliderFloat("Rect Rotation", &rectRotation, -360.0f, 360.0f);
        ImGui::SliderFloat("Corner Radius", &cornerRadius, 0.0f, 1.0f);
        ImGui::SliderFloat("Line Width", &lineWidth, 1.0f, 100.0f);
        ImGui::SliderFloat2("Rect Size", glm::value_ptr(rectSize), 0.1f, 500.0f);
        ImGui::ColorEdit4("LineColor", glm::value_ptr(currentLineColor));
        ImGui::ColorEdit4("CurrentColor", glm::value_ptr(currentMeshColor));

        ImGui::SliderFloat2("Point 1", glm::value_ptr(point1), -1000.0f, 1000.0f);
        ImGui::SliderFloat2("Point 2", glm::value_ptr(point2), -1000.0f, 1000.0f);
        ImGui::SliderFloat2("Point 3", glm::value_ptr(point3), -1000.0f, 1000.0f);

        ImGui::Text("Current Angle %f", displayAngle);
        ImGui::End();   

        ImGui::Render();


        // SR Rendering
        sr::srNewFrame(width, height);

        const glm::vec2& halfRectSize = rectSize / 2.0f;

       
        /*sr::srBegin(sr::EBatchDrawMode::LINES);
        sr::srVertex2f(0.0f, 0.0f);
        sr::srVertex2f(200.0f, 200.0f);
        sr::srEnd();*/

        sr::srDrawGrid({0.0f, 0.0f}, 10, 10, 100, 100);
        //sr::srDrawText(font, "0, 0", {0.0f, 0.0f});


        sr::srDrawText(font, "Wer das liest ist doof\nasdgfHüöo", glm::vec2(0.0f, 0.0f), sr::srGetColorFromFloat(currentLineColor), drawLines);

        //sr::srDrawRectangle(sr::Rectangle{0.0f, 0.0f, 1.0f, 1.0f}, glm::vec2{0.5f, 0.5f}, rectRotation, sr::srGetColorFromFloat(currentColor.x, currentColor.y, currentColor.z, currentColor.w));

        //sr::srDrawCircle(glm::vec2(0.0f, 0.0f), 0.2f, sr::srGetColorFromFloat(currentColor));

        //sr::srPathSetStrokeEnabled(false);        
        //sr::srPathSetFillEnabled(true);
//
        //sr::srPathSetStrokeColor(currentLineColor);
        //sr::srPathSetFillColor(currentMeshColor);
        //sr::srPathRectangle({0.0f, 0.0f, rectSize.x, rectSize.y}, rectSize / 2.0f, 0.0f, 0.0f);


        //sr::srDrawRectangleFilledRC({0.0f, 0.0f}, {rectSize.x, rectSize.y}, rectSize / 2.0f, rectRotation, cornerRadius, sr::srGetColorFromFloat(currentMeshColor));
        //sr::srDrawRectangleRC({0.0f, 0.0f}, {rectSize.x, rectSize.y}, rectSize / 2.0f, rectRotation, cornerRadius, lineWidth, sr::srGetColorFromFloat(currentLineColor));

        

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



        //sr::srDrawMesh(mesh);

        sr::srEndFrame();


        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    sr::srTerminate();
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}