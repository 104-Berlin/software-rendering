#include "src/pch.h"
#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "src/renderer/renderer.h"

#include "imgui.h"
#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_opengl3.h"

int main()
{
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac

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

    float rectRotation = 0.0f;
    glm::vec4 currentColor(1.0f, 1.0f, 1.0f, 1.0f);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        sr::srViewport(0, 0, (float)width, (float)height);
        sr::srClearColor(0.5f, 0.4f, 0.8f, 1.0f);
        sr::srClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ImGui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ImGui calls
        ImGui::Begin("Settings");
        ImGui::SliderFloat("Rect Rotation", &rectRotation, -360.0f, 360.0f);
        ImGui::ColorPicker4("CurrentColor", glm::value_ptr(currentColor));
        ImGui::End();   

        ImGui::Render();

        //sr::srDrawRectangle(sr::Rectangle{0.0f, 0.0f, 1.0f, 1.0f}, glm::vec2{0.5f, 0.5f}, rectRotation, sr::srGetColorFromFloat(currentColor.x, currentColor.y, currentColor.z, currentColor.w));

        //sr::srDrawCircle(glm::vec2(0.0f, 0.0f), 0.2f, sr::srGetColorFromFloat(currentColor));
        sr::srPathSetStrokeEnabled(true);        
        sr::srPathSetFillEnabled(false);

        sr::srPathSetStrokeColor(sr::srGetColorFromFloat(1.0f, 0.0f, 0.0f, 1.0f));
        sr::srPathSetFillColor(sr::srGetColorFromFloat(1.0f, 0.0f, 0.0f, 1.0f));
        sr::srPathRectangle({0.0f, 0.0f, 0.5f, 1.0f}, {0.25f, 0.25f}, 0.0f, 0.0f);
        
        sr::srPathSetStrokeEnabled(false);
        sr::srPathSetFillEnabled(true);
        
        sr::srPathSetStrokeColor(sr::srGetColorFromFloat(0.0f, 1.0f, 0.0f, 1.0f));
        sr::srPathSetFillColor(sr::srGetColorFromFloat(0.0f, 1.0f, 0.0f, 1.0f));
        sr::srPathRectangle({0.0f, 0.0f, 0.5f, 1.0f}, {0.25f, 0.25f}, 0.0f, 1.0f);


        sr::srDrawRenderBatch(&sr::SRC->RenderBatch);
        //sr::srDrawMesh(mesh);

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