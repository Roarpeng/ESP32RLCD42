#ifndef PHOTO_CONFIG_H
#define PHOTO_CONFIG_H

#include <string>

struct PhotoConfig {
    int interval = 10;
    std::string transition = "fade";
    std::string mode = "loop";
    std::string order = "sequential";

    void SetDefaults();
    bool Load(const char* filepath);
    bool Save(const char* filepath) const;
};

#endif
