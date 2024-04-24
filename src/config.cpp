#include "base.h"

namespace Config {
    CFG data_default, data;
    CFG* load(const char *path){
        FILE *fp = fopen(path, "r");
        if (!fp || fread(&data, 1, sizeof(data), fp) != sizeof(data))
            memcpy(&data, &data_default, sizeof(data));
        if (fp)
            fclose(fp);
        return &data;
    }

    void save(const char *path){
        FILE *fp = fopen(path, "w");
        if (!fp || fwrite(&data, 1, sizeof(data), fp) != sizeof(data))
            throw std::runtime_error("Can't write file");
        fclose(fp);
    }
}
