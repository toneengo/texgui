#pragma once
namespace TexGui {

namespace Math {

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

    template <typename v>
    void operator/=(const v& vec)
    {
        x /= vec.x;
        y /= vec.y;
    }

    //multiplication
    template <typename v>
    vec2<T> operator*(const v& num) const
    {
        vec2<T> val(x * static_cast<T>(num), y * static_cast<T>(num));
        return val;
    }

    template <typename v>
    vec2<T> operator*(const vec2<v>& num) const
    {
        vec2<T> val(x * static_cast<T>(num.x), y * static_cast<T>(num.y));
        return val;
    }

    //division
    template <typename v>
    vec2<T> operator/(const v& num) const
    {
        vec2<T> val(x / static_cast<T>(num), y / static_cast<T>(num));
        return val;
    }

    template <typename v>
    vec2<T> operator/(const vec2<v>& num) const
    {
        vec2<T> val(x / static_cast<T>(num.x), y / static_cast<T>(num.y));
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
    vec3()
        : x(0),
          y(0),
          z(0)
    {}
    vec3(const vec3<T>& _v)
        : x(static_cast<T>(_v.x)),
          y(static_cast<T>(_v.y)),
          z(static_cast<T>(_v.z))
    {}

    vec3(T _x, T _y, T _z)
        : x(static_cast<T>(_x)),
          y(static_cast<T>(_y)),
          z(static_cast<T>(_z))
    {}

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

    template <typename v>
    vec4<T> operator-(const v& vec)
    {
        vec4<T> val;
        val.x = x - vec.x;
        val.y = y - vec.y;
        val.z = z - vec.z;
        val.w = w - vec.w;
        return val;
    }

    template <typename v>
    vec4<T> operator+(const v& vec)
    {
        vec4<T> val;
        val.x = x + vec.x;
        val.y = y + vec.y;
        val.z = z + vec.z;
        val.w = w + vec.w;
        return val;
    }

    template <typename v>
    void operator+=(const v& vec)
    {
        x += vec.x;
        y += vec.y;
        z += vec.z;
        w += vec.w;
    }

    template <typename v>
    vec4<T> operator*(v num)
    {
        vec4<T> val;
        val.x = x * static_cast<T>(num);
        val.y = y * static_cast<T>(num);
        val.z = z * static_cast<T>(num);
        val.w = w * static_cast<T>(num);
        return val;
    }

    template <typename v>
    void operator*=(v num)
    {
        x *= static_cast<T>(num);
        y *= static_cast<T>(num);
        z *= static_cast<T>(num);
        w *= static_cast<T>(num);
    }

    template <typename v>
    void operator*=(const vec4<v>& vec)
    {
        x *= static_cast<T>(vec.x);
        y *= static_cast<T>(vec.y);
        z *= static_cast<T>(vec.z);
        w *= static_cast<T>(vec.w);
    }

    //#TODO: other initialisers. likely dont need them though. need to finish vec3 first
};

template <typename T>
struct mat4
{
    T data[16];
    // 0 1 2 3
    // 4 5 6 7 ...

    T* operator[](int idx)
    {
        //returns pointer which is array of size 4
        return &data[idx * 4];
    }
};

typedef mat4<float> fmat4;
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
    box(const box<v>& _box)
        : x(static_cast<T>(_box.x)),
          y(static_cast<T>(_box.y)),
          w(static_cast<T>(_box.w)),
          h(static_cast<T>(_box.h))
    {}

    template <typename v>
    box(box<v>&& _box)
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

    bool contains(const vec2<T>& _vec) const
    {
        vec2<T> d = _vec - pos;
        return d.x <= width && d.x >= 0 &&
               d.y <= height && d.y >= 0;
    }

    bool isValid() const
    {
        return !(width <= 0 || height <= 0);
    }

    bool contains(const box& _box)
    {
        if (((_box.x >= x && _box.x <= x + width) ||
            (_box.x + _box.width >= x && _box.x + _box.width <= x + width))
            &&
            ((_box.y >= y && _box.y <= y + height) ||
            (_box.y + _box.height >= y && _box.y + _box.height <= y + height))) return true;
        else return false;
    }

};

typedef box<float> fbox;
typedef box<int>   ibox;
typedef box<bool>  bbox;

template <typename T>
inline T clamp(T val, T minVal, T maxVal)
{
    return val > maxVal ? maxVal : (val < minVal ? minVal : val);
}

}

}
