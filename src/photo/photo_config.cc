#include "photo_config.h"
#include <cJSON.h>
#include <esp_log.h>
#include <cstdio>

static const char* TAG = "PhotoConfig";

void PhotoConfig::SetDefaults() {
    interval = 10;
    transition = "fade";
    mode = "loop";
    order = "sequential";
}

bool PhotoConfig::Load(const char* filepath) {
    SetDefaults();
    FILE* f = fopen(filepath, "r");
    if (!f) {
        ESP_LOGW(TAG, "Config file not found, using defaults: %s", filepath);
        return Save(filepath);
    }
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    if (sz <= 0 || sz > 4096) { fclose(f); return false; }
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc(sz + 1);
    if (!buf) { fclose(f); return false; }
    fread(buf, 1, sz, f);
    buf[sz] = 0;
    fclose(f);

    cJSON* json = cJSON_Parse(buf);
    free(buf);
    if (!json) return false;

    cJSON* item;
    if ((item = cJSON_GetObjectItem(json, "interval")) && cJSON_IsNumber(item))
        interval = item->valueint;
    if ((item = cJSON_GetObjectItem(json, "transition")) && cJSON_IsString(item))
        transition = item->valuestring;
    if ((item = cJSON_GetObjectItem(json, "mode")) && cJSON_IsString(item))
        mode = item->valuestring;
    if ((item = cJSON_GetObjectItem(json, "order")) && cJSON_IsString(item))
        order = item->valuestring;

    if (interval < 3) interval = 3;
    if (interval > 300) interval = 300;

    cJSON_Delete(json);
    ESP_LOGI(TAG, "Config loaded: interval=%d, transition=%s, mode=%s, order=%s",
             interval, transition.c_str(), mode.c_str(), order.c_str());
    return true;
}

bool PhotoConfig::Save(const char* filepath) const {
    cJSON* json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "interval", interval);
    cJSON_AddStringToObject(json, "transition", transition.c_str());
    cJSON_AddStringToObject(json, "mode", mode.c_str());
    cJSON_AddStringToObject(json, "order", order.c_str());
    char* str = cJSON_PrintUnformatted(json);
    cJSON_Delete(json);

    FILE* f = fopen(filepath, "w");
    if (!f) { free(str); return false; }
    fputs(str, f);
    fclose(f);
    free(str);
    return true;
}
