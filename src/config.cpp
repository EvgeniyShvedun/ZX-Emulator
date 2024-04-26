#include "base.h"

namespace Config {
    CFG cfg, defaults;
    CFG* load(const char *path){
        FILE *fp = fopen(path, "r");
        if (!fp || fread(&cfg, 1, sizeof(cfg), fp) != sizeof(cfg))
            memcpy(&cfg, &defaults, sizeof(cfg));
        if (fp)
            fclose(fp);
        return &cfg;
    }

    void save(const char *path){
        FILE *fp = fopen(path, "w");
        if (!fp || fwrite(&cfg, 1, sizeof(cfg), fp) != sizeof(cfg))
            throw std::runtime_error("Can't write file");
        fclose(fp);
    }
    
    CFG* get_defaults(){
        return &defaults;
    }
}
