#pragma once
namespace TexGui {

namespace Math {

//Barebones version of glm vec2, vec3 and vec4.
template <typename T>
struct vec2
{
    union {
        struct {
            T x; T y;
        };
        struct {
            T width; T height;
        };
    };
};

template <typename T>
struct vec4
{
    union {
        struct {
            T x; T y; T z; T w;
        };
        struct {
            T top; T right; T bottom; T left;
        };
    };
};

typedef vec4<float> fvec4;
typedef vec2<float> fvec2;

typedef vec4<int> ivec4;
typedef vec2<int> ivec2;
typedef vec2<unsigned int> uivec2;

// Template box type with xpos, ypos, width, and height, supporting vec2 containers.
template <typename T>
struct box
{
    vec2<T> pos;
    vec2<T> size;

    //expand or shrink box
    void expand(box _b, T _val)
    {
        pos.x -= _val;
        pos.y -= _val;
        size.width += _val * 2;
        size.height += _val * 2;
    }

    void translate(T _x, T _y)
    {
        pos.x += _x;
        pos.y += _y;
    }

    static box pad(box _b, vec4<T> _vec)
    {
        return {_b.pos.x + _vec.left, _b.pos.y + _vec.top, _b.size.width - (_vec.right + _vec.left), _b.size.height - (_vec.top + _vec.bottom)};
    }

    static box margin(box _b, vec4<T> _vec)
    {
        return {_b.pos.x - _vec.left, _b.pos.y - _vec.top, _b.size.width + (_vec.right + _vec.left), _b.size.height + (_vec.top + _vec.bottom)};
    }

    bool contains(const vec2<T>& _vec) const
    {
        T x = _vec.x - pos.x;
        T y = _vec.y - pos.y;
        return x <= size.width && x >= 0 &&
               y <= size.height && y >= 0;
    }

    bool isValid() const
    {
        return !(size.width <= 0 || size.height <= 0);
    }

    bool contains(const box& _box)
    {
        if (((_box.pos.x >= pos.x && _box.pos.x <= pos.x + size.width) ||
            (_box.pos.x + _box.size.width >= pos.x && _box.pos.x + _box.size.width <= pos.x + size.width))
            &&
            ((_box.pos.y >= pos.y && _box.pos.y <= pos.y + size.height) ||
            (_box.pos.y + _box.size.height >= pos.y && _box.pos.y + _box.size.height <= pos.y + size.height))) return true;
        else return false;
    }

};

typedef box<float> fbox;
typedef box<int>   ibox;

template <typename T>
inline T clamp(T val, T minVal, T maxVal)
{
    return val > maxVal ? maxVal : (val < minVal ? minVal : val);
}

}

}
