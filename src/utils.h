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

#define MIN(x, y) ((y) ^ (((x) ^ (y)) & -((x) < (y))))
#define MAX(x, y) ((x) ^ (((x) ^ (y)) & -((x) < (y))))
#define ABS(x)((x) < 0 ? -(x) : (x))
