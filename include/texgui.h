#pragma once

#if !defined(NAMESPACE_BEGIN) || defined(DOXYGEN_DOCUMENTATION_BUILD)
    #define NAMESPACE_BEGIN(name) namespace name {
#endif

#if !defined(NAMESPACE_END) || defined(DOXYGEN_DOCUMENTATION_BUILD)
    #define NAMESPACE_END(name) }
#endif

#include <array>
#include <cstring>
#include <string>
#include <cstdint>
#include <unordered_map>
#include <vector>
#include "texgui_math.hpp"
#include "texgui_settings.hpp"
#include "texgui_settings.hpp"
#include <vulkan/vulkan_core.h>

struct GLFWwindow;
NAMESPACE_BEGIN(TexGui);

inline bool CapturingMouse = false;

class RenderData;
struct VulkanInitInfo;
bool initGlfwOpenGL(GLFWwindow* window);
bool initGlfwVulkan(GLFWwindow* window, const VulkanInitInfo& info);

struct Texture;
struct TextSegment;
struct Paragraph
{
    TextSegment* data;
    uint32_t count;

    Paragraph() = default;

    Paragraph(std::vector<TextSegment>& s);

    Paragraph(TextSegment* ptr, uint32_t n);
};

using LazyData = int64_t;

using LazyIconFunc = Texture*(*)(LazyData);

// Text chunks are used to describe the layout of a text block. They get cached into Paragraphs.
struct TextChunk
{
    enum Type : uint8_t {
        TEXT,
        ICON,        // An icon to render inline with the text
        TOOLTIP,     // Marks the beginning/end of a tooltip's boundary
        INDIRECT,    // include a dynamic part while still processing the rest at compile time
        PLACEHOLDER, // Gets substituted for another chunk at draw time
        LAZY_ICON,   // Lazily fetch an icon
    } type;

    union
    {
        const char* text;
        Texture* icon;

        struct {
            Paragraph base;
            Paragraph tooltip;
        } tooltip;

        TextChunk* indirect;

        struct {
            LazyData data;
            LazyIconFunc func;
        } lazyIcon;
    };

    TextChunk() = default;

    constexpr TextChunk(const char* text) :
        type(TEXT), text(text) {}

    constexpr TextChunk(Texture* icon) :
        type(ICON), icon(icon) {}

    constexpr TextChunk(Paragraph baseText, Paragraph tooltip) :
        type(TOOLTIP), tooltip({ baseText, tooltip }) {}

    constexpr TextChunk(TextChunk* chunk) :
        type(INDIRECT), indirect(chunk) {}

    constexpr TextChunk(LazyData d, LazyIconFunc f) :
        type(LAZY_ICON), lazyIcon({d, f}) {}
};
inline TextChunk Placeholder() { TextChunk c; c.type = TextChunk::PLACEHOLDER; return c; };

struct TextSegment
{
    union
    {
        struct {
            const char* data;
            float tw; // Width of the text (not including whitespace)
            int16_t len;
        } word;

        Texture* icon;

        struct {
            Paragraph base;
            Paragraph tooltip;
        } tooltip;

        struct {
            LazyData data;
            LazyIconFunc func;
        } lazyIcon;
    };

    float width; // Width of the segment pre-scaling
        // Pt for text, pixels for icons. Includes whitespace for text

    enum Type : uint8_t {
        WORD,        // Denotes text followed by whitespace
        ICON,        // An icon to render inline with the text
        NEWLINE,     // Line break
        TOOLTIP,     // Marks the beginning/end of a tooltip's boundary
        PLACEHOLDER, // Gets substituted for another chunk at draw time
        LAZY_ICON,
    } type;

    // Doesn't break a chunk into words so you don't get text wrapping.
    static TextSegment FromChunkFast(TextChunk chunk);
};



// initialiser lists can't be implicitly converted to spans until c++26 so just make our own span type.
struct TextDecl
{
    const TextChunk* data;
    uint32_t count;

    TextDecl() = default;

    TextDecl(TextChunk* d, uint32_t n);

    TextDecl(std::initializer_list<TextChunk> s);

    const TextChunk* begin() const { return data; }
    const TextChunk* end() const { return data + count; }
};
    
using CharacterFilter = bool(*)(unsigned int c);

class Container
{
    friend struct Arrangers;
    friend class RenderData;

public:
    using ArrangeFunc = Math::fbox(*)(Container* parent, Math::fbox in);
        
    RenderData* rs;
    Math::fbox bounds;
    Math::fbox scissorBox = {-1, -1, -1, -1};

    Container Window(const char* name, float xpos, float ypos, float width, float height, uint32_t flags = 0,  Texture* texture = nullptr);
    bool      Button(const char* text, Texture* texture = nullptr, Container* out = nullptr);
    Container Box(float xpos, float ypos, float width, float height, Texture* texture = nullptr);
    void      CheckBox(bool* val);
    void      RadioButton(uint32_t* selected, uint32_t id);
    Container ScrollPanel(const char* name, Texture* texture = nullptr, Texture* bartex = nullptr);
    Container Slider(const char* name, Texture* texture = nullptr, Texture* bartex = nullptr);
    void      Image(Texture* texture, int scale = Defaults::PixelSize);

    void      TextInput(const char* name, std::string& buf, CharacterFilter filter = nullptr);
    void      TextInput(const char* name, char* buf, uint32_t bufsize, CharacterFilter filter = nullptr);
    void      Text(Paragraph text, int32_t scale = 0, TextDecl parameters = {});
    void      Text(const char* text, int32_t scale = 0, TextDecl parameters = {});

    Container Align(uint32_t flags = 0, Math::fvec4 padding = Math::fvec4(0,0,0,0));

    void      Divider(float padding = 0);

    // Similar to radio buttons - the id of the selected one is stored in the *selected pointer.
    // If you don't want them to be clickable - set selected to nullptr, and 0 or 1 for whether it is active in id
    Container ListItem(uint32_t* selected, uint32_t id);

    // Arranges children in a bento-grid layout.
    Container Grid();
    // Arranges children in a vertical stack.
    Container Stack(float padding = -1);
        
    void Row(Container* out, const float* widths, uint32_t n, float height, uint32_t flags = 0);
    template <uint32_t N>
    std::array<Container, N> Row(const float (&widths)[N], float height = 0, uint32_t flags = 0)
    {
        std::array<Container, N> out;
        Row(&out[0], widths, N, height, flags);
        return out;
    }

    void Column(Container* out, const float* widths, uint32_t n, float height);
    template <uint32_t N>
    std::array<Container, N> Column(const float (&heights)[N], float width = 0)
    {
        std::array<Container, N> out;
        Column(&out[0], heights, N, width);
        return out;
    }

private:
    std::unordered_map<std::string, uint32_t>* buttonStates;

    Container* parent;
    ArrangeFunc arrange;

    union
    {
        struct
        {
            float x, y, rowHeight;
            int n;
        } grid;
        struct
        {
            uint32_t* selected;
            uint32_t id;
        } listItem;

        struct 
        {
            uint32_t flags;
            float top, right, bottom, left;
        } align;

        struct
        {
            float y;
            float padding;
        } stack;
    };

    void* scrollPanelState = nullptr;


    inline Container withBounds(Math::fbox bounds, ArrangeFunc arrange = nullptr)
    {
        Container copy = *this;
        copy.bounds = bounds;
        copy.parent = this;
        copy.arrange = arrange;
        return copy;
    }

    static Math::fbox Arrange(Container* o, Math::fbox child);
};

struct IconSheet
{
    uint32_t glID;
    int32_t iw, ih; // Size of a single icon
    int32_t w, h;
    Texture* getIcon(uint32_t x, uint32_t y);
};

inline Container Base;

void render();
void render(RenderData& rs);
RenderData* newRenderData();
void loadFont(const char* font);
void loadTextures(const char* dir);
IconSheet loadIcons(const char* dir, int32_t iconWidth, int32_t iconHeight);
void clear();
void clear(RenderData& rs);

struct Texture;

// Get a specific texture that was read by loadTextures
Texture* texByName(const char* name);
// Prepare a reference to a texture already present in an OpenGL 2D texture object.
Texture* customTexture(unsigned int glTexID, Math::ibox pixelBounds);
// Prepare a reference to a texture already present in a Vulkan image.
Texture* customTexture(VkImageView imageView, VkSampler sampler, Math::ibox pixelBounds);

Math::ivec2 getTexSize(Texture* tex);

std::vector<TextSegment> cacheText(TextDecl text);
// Caches text into a buffer. Returns -1 on failure (if there's not enough buffer space), or the amount otherwise
int32_t cacheText(TextDecl text, TextSegment* buffer, uint32_t capacity);

float computeTextWidth(const char* text, size_t numchars);

class RenderData
{
    friend class GLContext;
public:
    RenderData()
    {
        Base = TexGui::Base;
        Base.rs = this;
        Base.bounds = Math::fbox(0, 0, 8192, 8192);
        Base.scissorBox = Math::fbox(0, 0, 8192, 8192);
    }
    Container Base;

    Container drawTooltip(Math::fvec2 size);

    void drawQuad(const Math::fbox& rect, const Math::fvec4& col, int32_t zLayer = 0);
    void drawTexture(Math::fbox rect, Texture* e, int state, int pixel_size, uint32_t flags, const Math::fbox& bounds, int32_t zLayer = 0);
    int drawText(const char* text, Math::fvec2 pos, const Math::fvec4& col, int size, uint32_t flags, int32_t len = -1, int32_t zLayer = 0);
    //void scissor(int x, int y, int width, int height);
    void scissor(Math::fbox bounds);
    void descissor();

    void clear() {
        objects.clear();
        commands.clear();

        objects2.clear();
        commands2.clear();
    }

    void swap(RenderData& other) {
        objects.swap(other.objects);
        commands.swap(other.commands);

        objects2.swap(other.objects2);
        commands2.swap(other.commands2);
    }

    void copy(const RenderData& other)
    {
        static auto copy = [](auto& a, auto& other, size_t sz)
        {
            size_t n = other.size();
            a.resize(n);
            memcpy(a.data(), other.data(), sz * n);
        };

        copy(objects, other.objects, sizeof(Object));
        copy(objects2, other.objects2, sizeof(Object));
        copy(commands, other.commands, sizeof(Command));
        copy(commands2, other.commands2, sizeof(Command));
    }

private:

    int prevObjCount = 0;
    int prevComCount = 0;
    // Renderable objects
    struct alignas(16) Character
    {
        Math::fbox rect; //xpos, ypos, width, height
        Math::ibox texBounds;
        Math::fvec4 colour = Math::fvec4(1.0);
        int size;
    };

    struct alignas(16) Quad
    {
        Math::fbox rect; //xpos, ypos, width, height
        Math::ibox texBounds; //xpos, ypos, width, height
        Math::fvec4 colour = Math::fvec4(1.0);
        int pixelSize;
    };

    struct alignas(16) ColQuad
    {
        Math::fbox rect; //xpos, ypos, width, height
        Math::fvec4 col;
        int padding;
    };

    struct alignas(16) Object
    {
        Object()
        {
        }

        Object(const Object& o)
        {
            *this = o;
        }

        Object(Character c)
        {
            ch = c;
        }
        Object(Quad q)
        {
            quad = q;
        }

        Object(ColQuad q)
        {
            cq = q;
        }
        union {
            Character ch;
            Quad quad;
            ColQuad cq;
        };
    };

    struct Command
    {
        enum {
            QUAD,
            CHARACTER,
            COLQUAD,
            SCISSOR,
            DESCISSOR,
        } type;

        uint32_t number;
        uint32_t flags = 0;
        Texture * texture;
        Math::fbox scissorBox;
        int state = 0;
    };

    std::vector<Object> objects;
    std::vector<Command> commands;

    // This is a lazy approach until we have some proper draw ordering/z filtering etc
    std::vector<Object> objects2;
    std::vector<Command> commands2;
};

Math::fvec2 calculateTextBounds(Paragraph text, int32_t scale, float maxWidth);
Math::fvec2 calculateTextBounds(const char* text, float maxWidth, int32_t scale = Defaults::Font::Size);

NAMESPACE_END(TexGui);
