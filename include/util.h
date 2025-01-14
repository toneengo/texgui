#include <stdio.h>

#ifdef _DEBUG
    #define DBG
#endif

#ifdef _DEBUG
    #define dbg_log(...) printf("%s (%i): ", __FILE__, __LINE__), printf(__VA_ARGS__), printf("\n")
#else
    #define dbg_log(...) do {} while(0)
#endif

#include <vector>
//search for closest number in a sorted unique array, returns (left) index
template <typename T, typename U>
inline int binarySearch(const std::vector<T>& arr, const U val)
{
    int l = 0, r = arr.size() - 1;

    if (r <= 0) return 0;

    if (val >= arr[r]) return r;
    if (val <= arr[l]) return l;

    while (l + 1 < r)
    {
        int m = (l + r)/2;
        if (arr[m] == val) return m;

        if (arr[m] > val)
        {
            r = m;
        }
        else if (arr[m] < val)
        {
            l = m;
        }
    }

    //return abs(val - arr[l]) > abs(val - arr[r]) ? r : l; //for this to work, val needs to be subtracted by 10 (text cursor offset)
    return l;
}

inline void setBit(unsigned int& dest, const unsigned int flag, bool on)
{
    dest = on ? dest | flag : dest & ~(flag);
}

inline bool getBit(const unsigned int dest, const unsigned int flag)
{
    return dest & flag;
}
