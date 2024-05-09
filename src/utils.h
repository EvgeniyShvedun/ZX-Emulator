template<class T>
void DELETE(T*& ptr){
    if (ptr){
        delete ptr;
        ptr = NULL;
    }       
}   

template<class T>
void DELETE_ARRAY(T*& ptr){
    if (ptr){
        delete[] ptr;
        ptr = NULL;
    }
}
/*
#define DELETE(ptr)(\
    if (ptr)\
        delete ptr;\
    ptr = NULL;

*/

#define DeleteArray(ptr)\
    if (ptr)\
        delete[] ptr;\
    ptr = NULL;

#define RGB5551(r, g, b, a) (((r) << 11) | (((g) & 0x1F) << 6) | (((b) & 0x1F) << 1) | ((a) & 0x01))
#define RGB4444(r, g, b, a) (((r) << 12) | (((g) & 0x0F) << 8) | (((b) & 0x0F) << 4) | ((a) & 0x0F))
#define RGB565(r, g, b)     (((r) << 11) | ((g) << 5) | ((b) & 0x1F))

/*
#define min(x, y) ((y) ^ (((x) ^ (y)) & -((x) < (y))))
#define max(x, y) ((x) ^ (((x) ^ (y)) & -((x) < (y))))
#define range(x, l, h) (min(max(x, l), h))
*/
