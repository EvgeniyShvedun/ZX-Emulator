#include <limits.h>
#include <stdexcept>
#include <stdio.h>
#include <string.h>
#include "config.h"

namespace Config {
    Cfg data;
    Cfg defaults;

    Cfg& load(const char *path){
        FILE *fp = fopen(path, "r");
        if (!fp || fread(&data, 1, sizeof(data), fp) != sizeof(data))
            memcpy(&data, &defaults, sizeof(data));
        if (fp)
            fclose(fp);
        return data;
    }

    void save(const char *path){
        FILE *fp = fopen(path, "w");
        if (!fp || fwrite(&data, 1, sizeof(data), fp) != sizeof(data))
            throw std::runtime_error("Can't write file");
        fclose(fp);
    }
    
    Cfg& get(){
        return data;
    }

    Cfg& get_defaults(){
        return defaults;
    }
}
