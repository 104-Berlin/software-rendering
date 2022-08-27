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
    float cornerRadius = 0.0f;
    glm::vec2 rectSize = {100.0f, 100.0f};
    glm::vec4 currentMeshColor(1.0f, 1.0f, 1.0f, 1.0f);
    glm::vec4 currentLineColor(1.0f, 0.0f, 0.0f, 1.0f);

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
        ImGui::SliderFloat("Rect Rotation", &rectRotation, -360.0f, 360.0f);
        ImGui::SliderFloat("Corner Radius", &cornerRadius, 0.0f, 1.0f);
        ImGui::SliderFloat2("Rect Size", glm::value_ptr(rectSize), 0.1f, 500.0f);
        ImGui::ColorEdit4("LineColor", glm::value_ptr(currentLineColor));
        ImGui::ColorEdit4("CurrentColor", glm::value_ptr(currentMeshColor));
        ImGui::End();   

        ImGui::Render();



        // SR Rendering
        sr::srNewFrame(width, height);

        const glm::vec2& halfRectSize = rectSize / 2.0f;


        //sr::srDrawRectangle(sr::Rectangle{0.0f, 0.0f, 1.0f, 1.0f}, glm::vec2{0.5f, 0.5f}, rectRotation, sr::srGetColorFromFloat(currentColor.x, currentColor.y, currentColor.z, currentColor.w));

        //sr::srDrawCircle(glm::vec2(0.0f, 0.0f), 0.2f, sr::srGetColorFromFloat(currentColor));

        sr::srPathSetStrokeEnabled(false);        
        sr::srPathSetFillEnabled(true);

        sr::srPathSetStrokeColor(currentLineColor);
        sr::srPathSetFillColor(currentMeshColor);
        sr::srPathRectangle({0.0f, 0.0f, rectSize.x, rectSize.y}, rectSize / 2.0f, 0.0f, 0.0f);
        
        sr::srPathSetStrokeEnabled(true);
        sr::srPathSetFillEnabled(false);
        
        sr::srPathSetStrokeColor(currentLineColor);
        sr::srPathSetFillColor(currentMeshColor);
        sr::srPathRectangle({0.0f, 0.0f, rectSize.x, rectSize.y}, rectSize / 2.0f, rectRotation, cornerRadius);

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