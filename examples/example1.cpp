#include "texgui.h"
#include "texgui_opengl.hpp"
#include "glad/gl.h"

#include "texgui_glfw.hpp"
#include "stdio.h"

#include "stb_image.h"
#include "stb_image.h"

GLFWwindow* window;

int SCR_WIDTH;
int SCR_HEIGHT;

bool selection[2];
enum {
    Lollipop = 0,
    Tennis = 1,
};

int main()
{
    glfwInit();
    // OpenGL version
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Create window 
    window = glfwCreateWindow(800, 600, "TexGui example 1", nullptr, nullptr);

    // Load OpenGL
    glfwMakeContextCurrent(window);
    gladLoadGL(glfwGetProcAddress);

    glfwGetWindowSize(window, &SCR_WIDTH, &SCR_HEIGHT);

    float xscale, yscale;
    glfwGetWindowContentScale(window, &xscale, &yscale);

    SCR_WIDTH *= xscale;
    SCR_HEIGHT *= yscale;

    // Setup debug messages
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(
        [](GLenum src, GLenum t, GLuint id, GLenum sev, GLsizei ln, auto* message, auto* user_param)
        {
            printf("%s \n", message);
        }, nullptr);

    glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);

    unsigned int VAO;
    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    //glDisable(GL_DEPTH_TEST);
    //glDisable(GL_STENCIL_TEST);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    //background
    int width, height, channels;
    unsigned char* im = stbi_load("resources/windows.jpg", &width, &height, &channels, 4);

    GLuint bg;
    glCreateTextures(GL_TEXTURE_2D, 1, &bg);

    glTextureStorage2D(bg, 1, GL_RGBA8, width, height);
    glTextureSubImage2D(bg, 0, 0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, im);

    stbi_image_free(im);

    GLuint vsh = glCreateShader(GL_VERTEX_SHADER); GLuint fsh = glCreateShader(GL_FRAGMENT_SHADER);

    const char* vsh_src = R"#(
#version 460 core
out vec2 uv;
void main() {
    ivec2 quad[6] = ivec2[6](
        ivec2(0, 0),
        ivec2(1, 0),
        ivec2(0, 1),
        ivec2(1, 1),
        ivec2(0, 1),
        ivec2(1, 0)
    ); 
    uv = quad[gl_VertexID];
    vec2 pos = quad[gl_VertexID] * 2 - 1;
    pos.y *= -1;
    gl_Position = vec4(pos, 0.0, 1.0);
}
    )#";
    const char* fsh_src = R"#(
#version 460 core
out vec4 frag;
in vec2 uv;
layout (location = 0) uniform sampler2D bg;
void main() {
    frag = texture(bg, uv);
}
    )#";

    glShaderSource(vsh, 1, (const GLchar**)&vsh_src, NULL);
    glShaderSource(fsh, 1, (const GLchar**)&fsh_src, NULL);
    glCompileShader(vsh); glCompileShader(fsh);

    int logLength;
    glGetShaderiv(vsh, GL_INFO_LOG_LENGTH, &logLength);
    
    GLuint bg_shader = glCreateProgram();
    glAttachShader(bg_shader, vsh); glAttachShader(bg_shader, fsh);
    glLinkProgram(bg_shader);

    glDeleteShader(vsh); glDeleteShader(fsh);

    // ************** ALL GUI STUFF ****************
    //TexGui::Defaults::PixelSize = 2;
    //TexGui::Defaults::Font::Size = 20;
    //TexGui::Defaults::Font::MsdfPxRange = 2;

    TexGui::init();
    TexGui::initOpenGL();
    TexGui::initGlfw(window);
    TexGui::loadFont("resources/fonts/PixelOperator.ttf");
    TexGui::loadTextures("resources/sprites");

    //glEnable(GL_DEPTH_TEST);
    std::string str;

    uint32_t selected = 0;

    TexGui::RenderData data; 
    TexGui::RenderData copy; 

    char charbuf[128] = "\0";
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(bg_shader);
        glBindTextureUnit(0, bg);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        data.Base.Window("not bob", 400, 500, 200, 200, TexGui::CENTER_X | TexGui::CENTER_Y);
        auto win = data.Base.Window("bob", 200, 100, 400, 600, TexGui::RESIZABLE);

        auto row = win.Row({200, 0});

        static bool a = false;
        static bool b = false;
        static bool c = true;

        static uint32_t selected2 = 0;
        auto box0 = row[0].Column({24, 24, 24, 24, 24});
        auto checkBoxCon = box0[0].Row({24, 0});
        checkBoxCon[0].CheckBox(&a);
        checkBoxCon[1].Align(TexGui::ALIGN_CENTER_Y).Text("value a");

        checkBoxCon = box0[1].Row({24, 0});
        checkBoxCon[0].RadioButton(&selected2, 0);
        checkBoxCon[1].Align(TexGui::ALIGN_CENTER_Y).Text("value b");

        checkBoxCon = box0[2].Row({24, 0});
        checkBoxCon[0].RadioButton(&selected2, 1);
        checkBoxCon[1].Align(TexGui::ALIGN_CENTER_Y).Text("value c");

        box0[4].TextInput("char input...", charbuf, 128);

        auto box1 = row[1].ScrollPanel("panel1", TexGui::getTexture("box2"));
        auto grid = box1.Grid();

        for (uint32_t i = 0; i < 10; i++)
        {
            grid
                .ListItem(&selected, i)
                .Image(TexGui::getTexture("lollipop"));
        }

        copy.copy(data);
        TexGui::render(copy);
        data.clear();

        TexGui::clear();

        glfwSwapBuffers(window);
    }
    glfwTerminate();
    return 0;
}