#include "src/pch.h"
#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include "src/renderer/renderer.h"


int main()
{
    glfwInit();

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ only
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Required on Mac

    GLFWwindow* window = glfwCreateWindow(1270, 720, "Rendering", NULL, NULL);

    glfwMakeContextCurrent(window);
    sr::srLoad((sr::SRLoadProc)glfwGetProcAddress);



    sr::Mesh mesh;
    mesh.Vertices = {
        {-0.5f, -0.5f, 0.0f},
        { 0.0f,  0.5f, 0.0f},
        { 0.5f, -0.5f, 0.0f}
    };
    mesh.Indices = {
        0, 1, 2
    };


    sr::srUploadMesh(&mesh);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        int width, height;
        glfwGetFramebufferSize(window, &width, &height);
        sr::srViewport(0, 0, (float)width, (float)height);
        sr::srClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        sr::srClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        sr::srDrawMesh(mesh);

        glfwSwapBuffers(window);
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    sr::srTerminate();

    return 0;
}