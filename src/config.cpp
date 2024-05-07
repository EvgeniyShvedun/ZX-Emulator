#include <limits.h>
#include <stdexcept>
#include <stdio.h>
#include <string.h>
#include "config.h"
#include <stdint.h>

namespace Config {
    Cfg data;
    Cfg defaults;
    Cfg& load(const char *path){
        FILE *fp = fopen(path, "r");
        if (!fp || fread(&data, 1, sizeof(data), fp) != sizeof(data) || strcmp(data.format_id, CONFIG_H_MODIFIED)){
            printf("WARNING: Can't read onfig data, the settings are set to default.\n");
            memcpy(&data, &defaults, sizeof(data));
            strcpy(data.format_id, CONFIG_H_MODIFIED);
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
