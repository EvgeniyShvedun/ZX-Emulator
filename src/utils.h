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


#define XCHG(x, y) ((x) ^ (y) ^ (x) ^ (y))
#define MIN(x, y) ((y) ^ (((x) ^ (y)) & -((x) < (y))))
#define MAX(x, y) ((x) ^ (((x) ^ (y)) & -((x) < (y))))
#define LIMIT(x, lower, upper) (min(max(x, lower), upper))
