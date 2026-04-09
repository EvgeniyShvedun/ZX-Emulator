#include <stdexcept>
#include <stdio.h>
#include <string.h>
#include "config.h"

namespace Config {
    Cfg data, defaults;
    Cfg& load(const char *path){
        FILE *fp = fopen(path, "r");
        if (!fp || fread(&data, 1, sizeof(data), fp) != sizeof(data) || strcmp(data.format_id, CONFIG_MODIFIED)){
            printf("WARN: Set the settings to default.\n");
            memcpy(&data, &defaults, sizeof(data));
            strcpy(data.format_id, CONFIG_MODIFIED);
        }
        if (fp)
            fclose(fp);
        return data;
    }

    void save(const char *path){
        FILE *fp = fopen(path, "w");
        if (!fp || fwrite(&data, 1, sizeof(data), fp) != sizeof(data))
            throw std::runtime_error("Write config file");
        fclose(fp);
    }

    Cfg& get(){
        return data;
    }

    Cfg& get_defaults(){
        return defaults;
    }
}
