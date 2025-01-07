#include "texgui.h"
#include "GLFW/glfw3.h"

#ifndef __gl_h_
#include "glad/gl.h"
#endif
#include "stdio.h"

#include "stb_image.h"

#include "immediate.h"

GLFWwindow* window;
using namespace TexGui;


int SCR_WIDTH;
int SCR_HEIGHT;

bool selection[2];
enum {
    Lollipop = 0,
    Tennis = 1,
};

static InputState input;
Screen* screen;
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

    glDisable(GL_DEPTH_TEST);
    glDisable(GL_STENCIL_TEST);

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
    TexGui::Defaults::PixelSize = 2;

    UIContext uictx(window);
    uictx.loadFont("resources/fonts/unifont.ttf");
    uictx.preloadTextures("resources/sprites");

    screen = uictx.screenPtr();

    GLContext* glctx = uictx.m_gl_context;

    glfwSetFramebufferSizeCallback(window,
        [](GLFWwindow* window, int width, int height) { screen->framebufferSizeCallback(width, height); }
    );
    glfwSetCursorPosCallback(window,
        [](GLFWwindow* window, double x, double y) { 
            input.cursor_pos = Math::ivec2(x, y);
            screen->cursorPosCallback(x, y); 
        }
    );
    glfwSetMouseButtonCallback(window,
        [](GLFWwindow* window, int button, int action, int mods) {
            if (button == GLFW_MOUSE_BUTTON_1)
            {
                if (action == GLFW_RELEASE)    input.lmb = KEY_Release;
                else if (input.lmb == KEY_Off) input.lmb = KEY_Press;
            }

            screen->mouseButtonCallback(button, action); 
        }
    );
    glfwSetKeyCallback(window,
        [](GLFWwindow* window, int key, int scancode, int action, int mods) { screen->keyCallback(key, scancode, action, mods); }
    );
    glfwSetCharCallback(window,
        [](GLFWwindow* window, unsigned int codepoint) { screen->charCallback(codepoint); }
    );

    while (!glfwWindowShouldClose(window))
    {
        if (input.lmb == KEY_Release) input.lmb = KEY_Off;
        if (input.lmb == KEY_Press) input.lmb = KEY_On;

        glfwPollEvents();
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(bg_shader);
        glBindTextureUnit(0, bg);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        RenderState immState = {};
        ImmCtx baseCtx = { &input, &immState, Math::fbox(0,0, SCR_WIDTH, SCR_HEIGHT) }; 

        auto win = imm_window(baseCtx, "bob", 0, 400, 100);

        // Split the window into 2 columns
        auto cells = imm_row(win, {140, 0});
        // Put a button in the left cell
        if (imm_button(cells[0], "pigeon balls"))
        {
            std::printf("pigeon gang\n");
        }

        auto state = uictx.draw();
        glctx->render(immState);

        glfwSwapBuffers(window);
    }
    glfwTerminate();
    return 0;
}
