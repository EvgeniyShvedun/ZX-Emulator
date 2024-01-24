template<class T>
void DELETE(T*& p_ptr){
    if (p_ptr){
        delete p_ptr;
        p_ptr = NULL;
    }       
}   

template<class T>
void DELETE_ARRAY(T*& p_ptr){
    if (p_ptr){
        delete[] p_ptr;
        p_ptr = NULL;
    }
}

#define RGBA32_4444(rgba)(\
   ((unsigned short)((((rgba) >> 24) & 0xFF) / 255.0 * 0x0F) << (4+4+4)) | \
   ((unsigned short)((((rgba) >> 16) & 0xFF) / 255.0 * 0x0F) << (4+4)) | \
   ((unsigned short)((((rgba) >> 8) & 0xFF) / 255.0 * 0x0F) << 4) | \
   (unsigned short)(((rgba) & 0xFF) / 255.0 * 0x0F))

/*
#define MIN(x, y) ((y) ^ (((x) ^ (y)) & -((x) < (y))))
#define MAX(x, y) ((x) ^ (((x) ^ (y)) & -((x) < (y))))
#define RANGE(x, l, h) (MIN(MAX(x, l), h))
*/
