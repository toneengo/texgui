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

struct GLFWwindow;
NAMESPACE_BEGIN(TexGui);

enum {
    INHERIT = 0
};

enum TexGui_flags : uint32_t
{
    CENTER_X = 0x01,
    CENTER_Y = 0x04,
    SLICE_9  = 0x08,
    SLICE_3  = 0x10,
    BLUR     = 0x40,
    WRAPPED  = 0x80,

    ALIGN_NONE = 0x100,
    ALIGN_LEFT = 0x200,
    ALIGN_CENTER_Y = 0x400,
    ALIGN_CENTER_X = 0x800,
    ALIGN_RIGHT = 0x1000,
    ALIGN_TOP = 0x2000,
    ALIGN_BOTTOM = 0x4000,
};

enum TexGui_state : uint8_t 
{
    STATE_NONE   = 0b00000001,
    STATE_ACTIVE = 0b00000010,
    STATE_HOVER  = 0b00000100,
    STATE_PRESS  = 0b00001000,
};

NAMESPACE_BEGIN(Math);

//Barebones version of glm vec2, vec3 and vec4.
template <typename T>
struct vec2
{
    union { T x, r; };
    union { T y, g; };

    vec2()
        : x(0),
          y(0)
    {};

    vec2(const vec2<T>& _xy)
        : x(static_cast<T>(_xy.x)),
          y(static_cast<T>(_xy.y))
    {};

    vec2(const int _x)
        : x(static_cast<T>(_x)),
          y(static_cast<T>(_x))
    {};

    vec2(const float _x)
        : x(static_cast<T>(_x)),
          y(static_cast<T>(_x))
    {};

    vec2(const double _x)
        : x(static_cast<T>(_x)),
          y(static_cast<T>(_x))
    {};

    template <typename v>
    vec2(const v& _xy)
        : x(static_cast<T>(_xy.x)),
          y(static_cast<T>(_xy.y))
    {};

    template <typename v>
    vec2(v&& _xy)
        : x(static_cast<T>(_xy.x)),
          y(static_cast<T>(_xy.y))
    {};

    vec2(T _x, T _y)
        : x(static_cast<T>(_x)),
          y(static_cast<T>(_y))
    {};

    template <typename v>
    vec2<T> operator-(const v& vec) const
    {
        vec2<T> val(x - vec.x, y - vec.y);
        return val;
    }

    template <typename v>
    void operator-=(const v& vec)
    {
        x -= vec.x;
        y -= vec.y;
    }

    //#TODO: test if othe way works?
    template <typename v>
    vec2<T> operator+(const v& vec) const
    {
        vec2<T> val(x + vec.x, y + vec.y);
        return val;
    }

    template <typename v>
    void operator+=(const v& vec)
    {
        x += vec.x;
        y += vec.y;
    }

    //multiplication
    template <typename v>
    vec2<T> operator*(const v& num) const
    {
        vec2<T> val(x * static_cast<T>(num), y * static_cast<T>(num));
        return val;
    }

    //division
    template <typename v>
    vec2<T> operator/(const v& num) const
    {
        vec2<T> val(x / static_cast<T>(num), y / static_cast<T>(num));
        return val;
    }

    /*
    template <typename v>
    void operator-(const v& vec)
    {
        x -= vec.x;
        y -= vec.y;
    }
    */

};

template <typename T>
struct vec3
{
    union
    {
        struct 
        {
            union { T x, r; };
            union
            {
                struct
                {
                    union { T y, g; };
                    union { T z, b;};
                };
                union { vec2<T> yz, gb; };
            };
        };
        union { vec2<T> xy, rg; };
    };

    vec3(T _x, T _y, T _z)
        : x(static_cast<T>(_x)),
          y(static_cast<T>(_y)),
          z(static_cast<T>(_z))
    {};

    //#TODO: other initialisers. likely dont need them though
};

template <typename T>
struct vec4
{
    union {
        struct {
            union { T x, r, top; };
            union {
                struct {
                    union { T y, g, right; };
                    union {
                        struct {
                            union { T z, b, bottom; };
                            union { T w, a, left; };
                        };
                        union { vec2<T> zw, ba; };
                    };
                };
                union { vec3<T> yzw, gba; };
                union { vec2<T> yz, gb; };
            };
        };
        union { vec3<T> xyz, rgb; };
        union { vec2<T> xy, rg; };
    };

    vec4()
        : x(0),
          y(0),
          z(0),
          w(0)
    {}
    
    vec4(T _x, T _y, T _z, T _w)
        : x(static_cast<T>(_x)),
          y(static_cast<T>(_y)),
          z(static_cast<T>(_z)),
          w(static_cast<T>(_w))
    {}

    vec4(T _val)
        : x(static_cast<T>(_val)),
          y(static_cast<T>(_val)),
          z(static_cast<T>(_val)),
          w(static_cast<T>(_val))
    {}

    vec4(const vec4<T>& _v)
        : x(static_cast<T>(_v.x)),
          y(static_cast<T>(_v.y)),
          z(static_cast<T>(_v.z)),
          w(static_cast<T>(_v.w))
    {}

    vec4(vec2<T> _xy, T _z, T _w)
        : x(static_cast<T>(_xy.x)),
          y(static_cast<T>(_xy.y)),
          z(static_cast<T>(_z)),
          w(static_cast<T>(_w))
    {}

    vec4(vec2<T> _xy, vec2<T> _zw)
        : x(static_cast<T>(_xy.x)),
          y(static_cast<T>(_xy.y)),
          z(static_cast<T>(_zw.x)),
          w(static_cast<T>(_zw.y))
    {}

    vec4(T _x, T _y, vec2<T> _zw)
        : x(static_cast<T>(_x)),
          y(static_cast<T>(_y)),
          z(static_cast<T>(_zw.x)),
          w(static_cast<T>(_zw.y))
    {}

    vec4(T _x, vec2<T> _yz, T _w)
        : x(static_cast<T>(_x)),
          y(static_cast<T>(_yz.x)),
          z(static_cast<T>(_yz.y)),
          w(static_cast<T>(_w))
    {}

    //#TODO: other initialisers. likely dont need them though. need to finish vec3 first
};

typedef vec4<bool> bvec4;

typedef vec4<float> fvec4;
typedef vec3<float> fvec3;
typedef vec2<float> fvec2;

typedef vec4<int> ivec4;
typedef vec3<int> ivec3;
typedef vec2<int> ivec2;
typedef vec2<unsigned int> uivec2;

// Template box type with xpos, ypos, width, and height, supporting vec2 containers.
template <typename T>
struct box
{
    union
    {
        struct
        {
            T x;
            T y;
        };
        union { vec2<T> pos, xy; };
    };
    union
    {
        struct
        {
            union { T w; T width; };
            union { T h; T height; };
        };
        union { vec2<T> size, wh; };
    };

    box()
        : x(0),
          y(0),
          w(0),
          h(0)
    {}

    box(const box<T>& _box)
        : x(_box.x),
          y(_box.y),
          w(_box.w),
          h(_box.h)
    {}

    box(T _x, T _y, T _w, T _h)
        : x(static_cast<T>(_x)),
          y(static_cast<T>(_y)),
          w(static_cast<T>(_w)),
          h(static_cast<T>(_h))
    {}

    box(const int _val)
        : x(static_cast<T>(_val)),
          y(static_cast<T>(_val)),
          w(static_cast<T>(_val)),
          h(static_cast<T>(_val))
    {}

    box(const float _val)
        : x(static_cast<T>(_val)),
          y(static_cast<T>(_val)),
          w(static_cast<T>(_val)),
          h(static_cast<T>(_val))
    {}

    template <typename v>
    box(const v& _box)
        : x(static_cast<T>(_box.x)),
          y(static_cast<T>(_box.y)),
          w(static_cast<T>(_box.w)),
          h(static_cast<T>(_box.h))
    {}

    template <typename v>
    box(v&& _box)
        : x(static_cast<T>(_box.x)),
          y(static_cast<T>(_box.y)),
          w(static_cast<T>(_box.w)),
          h(static_cast<T>(_box.h))
    {}

    template <typename v>
    box(const v& _pos, const v& _size)
        : x(static_cast<T>(_pos.x)),
          y(static_cast<T>(_pos.y)),
          w(static_cast<T>(_size.x)),
          h(static_cast<T>(_size.y))
    {}

    //expand or shrink box
    static box expand(box _b, T _val)
    {
        return {_b.x - _val, _b.y - _val, _b.width + _val*2, _b.height + _val*2};
    }

    void translate(T _x, T _y)
    {
        x += _x;
        y += _y;
    }
    inline void translate(vec2<T> _vec)
    {
        x += _vec.x;
        y += _vec.y;
    }

    void pad(T _top, T _right, T _bottom, T _left)
    {
        x += _left;
        y += _top;
        width -= _right + _left;
        height -= _top + _bottom;
    }

    void pad(vec4<T> _vec)
    {
        x += _vec.left;
        y += _vec.top;
        width -= _vec.right + _vec.left;
        height -= _vec.top + _vec.bottom;
    }

    static box pad(box _b, vec4<T> _vec)
    {
        return {_b.x + _vec.left, _b.y + _vec.top, _b.width - (_vec.right + _vec.left), _b.height - (_vec.top + _vec.bottom)};
    }

    static box margin(box _b, vec4<T> _vec)
    {
        return {_b.x - _vec.left, _b.y - _vec.top, _b.width + (_vec.right + _vec.left), _b.height + (_vec.top + _vec.bottom)};
    }

    bool contains(const vec2<T>& _vec)
    {
        vec2<T> d = _vec - pos;
        return d.x <= width && d.x >= 0 &&
               d.y <= height && d.y >= 0;
    }

};

typedef box<float> fbox;
typedef box<int>   ibox;
typedef box<bool>  bbox;

NAMESPACE_END(Math);

struct RenderData;
bool initGlfwOpenGL(GLFWwindow* window);

struct TexEntry;

class Container
{
    friend struct Arrangers;

public:
    using ArrangeFunc = Math::fbox(*)(Container* parent, Math::fbox in);
        
    RenderData* rs;
    Math::fbox bounds;

    Container Window(const char* name, float xpos, float ypos, float width, float height, uint32_t flags = 0);
    bool      Button(const char* text, TexEntry* texture = nullptr);
    Container Box(float xpos, float ypos, float width, float height, TexEntry* texture = nullptr);
    void      TextInput(const char* name, std::string& buf);
    void      Image(TexEntry* texture);

    // Similar to radio buttons - the id of the selected one is stored in the *selected pointer.
    Container ListItem(uint32_t* selected, uint32_t id);

    Container Grid();
        
    template <uint32_t N>
    std::array<Container, N> Row(const float (&widths)[N], float height = 0, uint32_t flags = 0)
    {
        std::array<Container, N> out;
        Row_Internal(&out[0], widths, N, height, flags);
        return out;
    }

    template <uint32_t N>
    std::array<Container, N> Column(const float (&heights)[N], float width = 0)
    {
        std::array<Container, N> out;
        Column_Internal(&out[0], heights, N, width);
        return out;
    }

private:
    void Row_Internal(Container* out, const float* widths, uint32_t n, float height, uint32_t flags);
    void Column_Internal(Container* out, const float* widths, uint32_t n, float height);
    std::unordered_map<std::string, uint32_t>* buttonStates;

    Container* parent;
    ArrangeFunc arrange;

    union
    {
        struct
        {
            float x, y, rowHeight;
        } grid;
        struct
        {
            uint32_t* selected;
            uint32_t id;
        } listItem;
    };


    inline Container withBounds(Math::fbox bounds, ArrangeFunc arrange = nullptr)
    {
        Container copy = *this;
        copy.bounds = bounds;
        copy.parent = this;
        copy.arrange = arrange;
        return copy;
    }
};


struct Arrangers
{
    static Math::fbox Arrange(Container* o, Math::fbox child);
    static Math::fbox ArrangeListItem(Container* listItem, Math::fbox child);
    static Math::fbox ArrangeGrid(Container* grid, Math::fbox child);
};


inline Container Base;

void render();
void loadFont(const char* font);
void loadTextures(const char* dir);
void clear();

struct TexEntry;

// Get a specific texture that was read by loadTextures
TexEntry* texByName(const char* name);
// Prepare a reference to a texture already present in an OpenGL array texture.
TexEntry* customTexture(unsigned int glTexID, unsigned int layer, Math::ibox pixelBounds);

NAMESPACE_BEGIN(Defaults);

inline int PixelSize  = 1;
inline uint32_t Flags = SLICE_9;

//#TODO: use font px instead of sacle, uising msdf

namespace Font {
    inline Math::fvec4 Color = {1, 1, 1, 1};
    inline int Size = 20;
    inline int MsdfPxRange = 1;
}

namespace Window {
    inline std::string Texture = "window";
    inline Math::fvec4 Padding(12);
}

namespace Button {
    inline std::string Texture = "button";
    inline Math::fvec4 Padding(12);
    inline uint32_t    Flags   = SLICE_9 | CENTER_X | CENTER_Y;
    inline Math::fvec2 POffset = 0;
}

namespace TextInput {
    inline std::string Texture = "textinput";
    inline Math::fvec4 Padding(8);
}

namespace ListItem {
    inline std::string Texture = "listitem";
    inline Math::fvec4 Padding(4);
}

namespace Box {
    inline Math::fvec4 Padding(12);
}

namespace Row {
    inline int Height = INHERIT;
    inline int Spacing = 4;
}

namespace Column {
    inline int Height = INHERIT;
    inline int Spacing = 4;
}

NAMESPACE_END(Defaults);
NAMESPACE_END(TexGui);
