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


/*
#define MIN(x, y) ((y) ^ (((x) ^ (y)) & -((x) < (y))))
#define MAX(x, y) ((x) ^ (((x) ^ (y)) & -((x) < (y))))
#define RANGE(x, l, h) (MIN(MAX(x, l), h))
*/
