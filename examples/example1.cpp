#include "texgui.h"
#include "GLFW/glfw3.h"
#include "glad/gl.h"
#include "stdio.h"

#include "stb_image.h"

GLFWwindow* window;
using namespace TexGui;

void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    glViewport(0, 0, width, height);
}

void message_callback(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, GLchar const* message, void const* user_param)
{
    printf("%s \n", message);
}

int SCR_WIDTH;
int SCR_HEIGHT;

bool selection[2];
enum {
    Lollipop = 0,
    Tennis = 1,
};

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
    glDebugMessageCallback(message_callback, nullptr);
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

    glfwSetFramebufferSizeCallback(window,
        [](GLFWwindow* window, int width, int height) { screen->framebufferSizeCallback(width, height); }
    );
    glfwSetCursorPosCallback(window,
        [](GLFWwindow* window, double x, double y) { screen->cursorPosCallback(x, y); }
    );
    glfwSetMouseButtonCallback(window,
        [](GLFWwindow* window, int button, int action, int mods) { screen->mouseButtonCallback(button, action); }
    );
    glfwSetKeyCallback(window,
        [](GLFWwindow* window, int key, int scancode, int action, int mods) { screen->keyCallback(key, scancode, action, mods); }
    );
    glfwSetCharCallback(window,
        [](GLFWwindow* window, unsigned int codepoint) { screen->charCallback(codepoint); }
    );

    //floating text input box
    uictx.addWidget(new TextInput("Enter text here :-)", 500, 200, 200, 32));

    //window 1
    Widget* window1 = uictx.addWidget(new Window("window 1", 400, 600, 400, 200));
    Widget* sendbutton = new Button("send!!", [](){printf("sent lololo\n");});
    Widget* button = new Button("yes", [](){printf("sent lololo\n");});
    Widget* input = new TextInput("Enter text:");

    Widget* row1 = window1->addChild(new Row(70, 0, 0));
    (*row1)[0] = sendbutton;
    (*row1)[1] = input;
    (*row1)[2] = button;

    //window 2
    Widget* window2 = new Window("window 2", 200, 100, 800, 400);

    Widget* row2 = window2->addChild(new Row(180, 0));

    (*row2)[0] = new Box();
    (*row2)[0]->assignTexture("box1");

    (*row2)[1] = new Box();
    (*row2)[1]->assignTexture("box2");
    (*row2)[1]->setFlags(0);

    Row* listrow = (Row*)(*row2)[1]->addChild(new Row());
    listrow->setSize({0, 40});
    listrow->setFlags(WRAPPED);
    for (int i = 0; i < 15; i++)
    {
        Widget* item1 = listrow->addCol(new ListItem("lollipop", "", &selection[0]), -1);
        Widget* item2 = listrow->addCol(new ListItem("tennis", "", &selection[1]), -1);
    }

    uictx.addWidget(window2);

    int lastSel = selection[Lollipop] ? Lollipop : Tennis;

    Button lolbut( "lollipop time!!", [](){printf("lollipop:-)\n");} );
    Button tenbut( "tenis", [](){printf("tenis:-(\n");} );

    Image lolimg("lollipop");
    Image tenimg("tennis");

    Label lollab("a very tasty and red lollipop");
    Label tenlab("a green tennis ball");
    Column col = Column();

    Widget* box1 = (*row2)[0];
    box1->addChild(&col);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        glClearColor(0.2f, 0.3f, 0.3f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        glUseProgram(bg_shader);
        glBindTextureUnit(0, bg);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        if (selection[Lollipop])
        {
            col.clear();
            col.addRow(&lolimg, 100, ALIGN_CENTER_X | ALIGN_CENTER_Y);
            col.addRow(&lollab);
            col.addRow(nullptr);
            col.addRow(&lolbut, 32);
            lastSel = Lollipop;
        }
        else if (selection[Tennis])
        {
            col.clear();
            col.addRow(&tenimg, 100, ALIGN_CENTER_X | ALIGN_CENTER_Y);
            col.addRow(&tenlab);
            col.addRow(nullptr);
            col.addRow(&tenbut, 32);
            lastSel = Tennis;
        }

        uictx.render();

        glfwSwapBuffers(window);
    }
    glfwTerminate();
    return 0;
}
