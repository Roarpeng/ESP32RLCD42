#include "http_server.h"
#include "photo_manager.h"
#include <esp_log.h>
#include <cJSON.h>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <sys/stat.h>
#include <strings.h>
#include <esp_heap_caps.h>
#include "web_ui_data.h"

static const char* TAG = "PhotoHTTP";

PhotoHttpServer& PhotoHttpServer::GetInstance() {
    static PhotoHttpServer instance;
    return instance;
}

bool PhotoHttpServer::Start(uint16_t port) {
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = port;
    config.max_uri_handlers = 8;
    config.max_resp_headers = 16;
    config.recv_wait_timeout = 30;
    config.send_wait_timeout = 30;
    config.stack_size = 10240;
    config.max_open_sockets = 4;

    esp_err_t ret = httpd_start(&server_, &config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %d", ret);
        return false;
    }

    httpd_uri_t uri = {};

    uri.uri = "/";
    uri.method = HTTP_GET;
    uri.handler = IndexHandler;
    uri.user_ctx = nullptr;
    httpd_register_uri_handler(server_, &uri);

    uri.uri = "/api/upload";
    uri.method = HTTP_POST;
    uri.handler = UploadHandler;
    httpd_register_uri_handler(server_, &uri);

    uri.uri = "/api/photos";
    uri.method = HTTP_GET;
    uri.handler = ListPhotosHandler;
    httpd_register_uri_handler(server_, &uri);

    uri.uri = "/api/photos";
    uri.method = HTTP_DELETE;
    uri.handler = BatchDeleteHandler;
    httpd_register_uri_handler(server_, &uri);

    uri.uri = "/api/config";
    uri.method = HTTP_GET;
    uri.handler = GetConfigHandler;
    httpd_register_uri_handler(server_, &uri);

    uri.uri = "/api/config";
    uri.method = HTTP_POST;
    uri.handler = SetConfigHandler;
    httpd_register_uri_handler(server_, &uri);

    running_ = true;
    ESP_LOGI(TAG, "HTTP server started on port %d", port);
    return true;
}

void PhotoHttpServer::Stop() {
    if (server_) {
        httpd_stop(server_);
        server_ = nullptr;
    }
    running_ = false;
}

// GET / — return embedded WebUI HTML
esp_err_t PhotoHttpServer::IndexHandler(httpd_req_t* req) {
    httpd_resp_set_type(req, "text/html; charset=utf-8");
    httpd_resp_send(req, (const char*)web_ui_html_data, web_ui_html_size);
    return ESP_OK;
}

// POST /api/upload — multipart file upload
esp_err_t PhotoHttpServer::UploadHandler(httpd_req_t* req) {
    int total_len = req->content_len;
    if (total_len > MAX_BODY_SIZE) {
        httpd_resp_send_err(req, HTTPD_413_CONTENT_TOO_LARGE, "Body too large");
        return ESP_FAIL;
    }

    char* body = (char*)heap_caps_malloc(total_len + 1, MALLOC_CAP_SPIRAM);
    if (!body) { 
        body = (char*)malloc(total_len + 1); 
    }
    if (!body) { httpd_resp_send_500(req); return ESP_FAIL; }

    int received = 0;
    while (received < total_len) {
        int ret = httpd_req_recv(req, body + received, total_len - received);
        if (ret <= 0) { free(body); httpd_resp_send_500(req); return ESP_FAIL; }
        received += ret;
    }
    body[total_len] = 0;

    // Parse Content-Type for boundary
    char content_type[128] = {};
    httpd_req_get_hdr_value_str(req, "Content-Type", content_type, sizeof(content_type));
    const char* boundary_prefix = "multipart/form-data; boundary=";
    const char* bp = strstr(content_type, boundary_prefix);
    if (!bp) { free(body); httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Expected multipart"); return ESP_FAIL; }

    std::string boundary = std::string("--") + (bp + strlen(boundary_prefix));

    auto& manager = PhotoManager::GetInstance();
    cJSON* result = cJSON_CreateObject();
    cJSON* uploaded = cJSON_CreateArray();
    cJSON* errors = cJSON_CreateArray();

    const char* pos = body;
    const char* end = body + total_len;
    while (pos < end) {
        const char* bpos = strstr(pos, boundary.c_str());
        if (!bpos) break;
        const char* part_start = bpos + boundary.length();
        if (part_start >= end) break;
        if (*part_start == '\r') part_start += 2;
        else if (*part_start == '-') break; // final boundary

        // Find headers end
        const char* header_end = strstr(part_start, "\r\n\r\n");
        if (!header_end) break;
        const char* data_start = header_end + 4;

        // Extract filename
        const char* fn = strstr(part_start, "filename=\"");
        if (!fn || fn > header_end) { pos = part_start; continue; }
        fn += 10;
        const char* fn_end = strchr(fn, '"');
        if (!fn_end || fn_end > header_end) { pos = part_start; continue; }
        std::string filename(fn, fn_end - fn);

        // Find next boundary
        const char* next_boundary = strstr(data_start, boundary.c_str());
        if (!next_boundary) next_boundary = end;
        const char* data_end = next_boundary;
        // Trim trailing \r\n before boundary
        if (data_end > data_start && *(data_end - 1) == '\n') data_end--;
        if (data_end > data_start && *(data_end - 1) == '\r') data_end--;

        size_t file_size = data_end - data_start;
        if (file_size > MAX_FILE_SIZE) {
            cJSON* err = cJSON_CreateObject();
            cJSON_AddStringToObject(err, "file", filename.c_str());
            cJSON_AddStringToObject(err, "error", "File too large (>2MB)");
            cJSON_AddItemToArray(errors, err);
            pos = next_boundary;
            continue;
        }

        // Validate extension
        const char* ext = strrchr(filename.c_str(), '.');
        if (!ext || (strcasecmp(ext, ".jpg") && strcasecmp(ext, ".jpeg")
                  && strcasecmp(ext, ".png")  && strcasecmp(ext, ".bmp"))) {
            cJSON* err = cJSON_CreateObject();
            cJSON_AddStringToObject(err, "file", filename.c_str());
            cJSON_AddStringToObject(err, "error", "Unsupported format");
            cJSON_AddItemToArray(errors, err);
            pos = next_boundary;
            continue;
        }

        // Write to SD card
        std::string fullpath = "/sdcard/photos/" + filename;
        FILE* f = fopen(fullpath.c_str(), "wb");
        if (!f) {
            cJSON* err = cJSON_CreateObject();
            cJSON_AddStringToObject(err, "file", filename.c_str());
            cJSON_AddStringToObject(err, "error", "SD card write failed");
            cJSON_AddItemToArray(errors, err);
        } else {
            fwrite(data_start, 1, file_size, f);
            fclose(f);
            manager.AddPhoto(fullpath.c_str());
            cJSON_AddItemToArray(uploaded, cJSON_CreateString(filename.c_str()));
        }
        pos = next_boundary;
    }

    free(body);
    cJSON_AddItemToObject(result, "uploaded", uploaded);
    cJSON_AddItemToObject(result, "errors", errors);
    cJSON_AddNumberToObject(result, "total", manager.GetPhotoCount());

    char* resp = cJSON_PrintUnformatted(result);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    free(resp);
    cJSON_Delete(result);
    return ESP_OK;
}

// GET /api/photos — JSON list
esp_err_t PhotoHttpServer::ListPhotosHandler(httpd_req_t* req) {
    auto& manager = PhotoManager::GetInstance();
    cJSON* arr = cJSON_CreateArray();
    for (auto& path : manager.GetPhotos()) {
        cJSON* item = cJSON_CreateObject();
        const char* name = strrchr(path.c_str(), '/');
        name = name ? name + 1 : path.c_str();
        cJSON_AddStringToObject(item, "name", name);
        struct stat st;
        if (stat(path.c_str(), &st) == 0) {
            cJSON_AddNumberToObject(item, "size", st.st_size);
        }
        cJSON_AddItemToArray(arr, item);
    }
    cJSON* resp_json = cJSON_CreateObject();
    cJSON_AddItemToObject(resp_json, "photos", arr);
    cJSON_AddNumberToObject(resp_json, "total", manager.GetPhotoCount());
    cJSON_AddNumberToObject(resp_json, "current", manager.GetCurrentIndex());
    cJSON_AddStringToObject(resp_json, "state",
        manager.GetState() == SlideShowState::PLAYING ? "playing" :
        manager.GetState() == SlideShowState::PAUSED ? "paused" : "idle");

    char* resp = cJSON_PrintUnformatted(resp_json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    free(resp);
    cJSON_Delete(resp_json);
    return ESP_OK;
}

// DELETE /api/photos — single or batch delete
esp_err_t PhotoHttpServer::BatchDeleteHandler(httpd_req_t* req) {
    char query[256] = {};
    if (httpd_req_get_url_query_str(req, query, sizeof(query)) == ESP_OK) {
        char name[128];
        if (httpd_query_key_value(query, "name", name, sizeof(name)) == ESP_OK) {
            auto& manager = PhotoManager::GetInstance();
            std::string fullpath = "/sdcard/photos/" + std::string(name);
            bool ok = manager.RemovePhoto(fullpath.c_str());
            cJSON* r = cJSON_CreateObject();
            cJSON_AddBoolToObject(r, "success", ok);
            char* resp = cJSON_PrintUnformatted(r);
            httpd_resp_set_type(req, "application/json");
            httpd_resp_send(req, resp, strlen(resp));
            free(resp);
            cJSON_Delete(r);
            return ESP_OK;
        }
    }

    // Batch delete: JSON body
    int total_len = req->content_len;
    if (total_len > 0 && total_len < 4096) {
        char* body = (char*)malloc(total_len + 1);
        httpd_req_recv(req, body, total_len);
        body[total_len] = 0;
        cJSON* json = cJSON_Parse(body);
        free(body);
        if (json) {
            cJSON* names_arr = cJSON_GetObjectItem(json, "names");
            if (names_arr && cJSON_IsArray(names_arr)) {
                std::vector<std::string> names;
                int count = cJSON_GetArraySize(names_arr);
                for (int i = 0; i < count; i++) {
                    cJSON* item = cJSON_GetArrayItem(names_arr, i);
                    if (cJSON_IsString(item))
                        names.push_back("/sdcard/photos/" + std::string(item->valuestring));
                }
                auto& manager = PhotoManager::GetInstance();
                int removed = manager.RemovePhotos(names);
                cJSON* r = cJSON_CreateObject();
                cJSON_AddNumberToObject(r, "removed", removed);
                cJSON_AddNumberToObject(r, "total", manager.GetPhotoCount());
                char* resp = cJSON_PrintUnformatted(r);
                httpd_resp_set_type(req, "application/json");
                httpd_resp_send(req, resp, strlen(resp));
                free(resp);
                cJSON_Delete(r);
                cJSON_Delete(json);
                return ESP_OK;
            }
            cJSON_Delete(json);
        }
    }
    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing 'name' or JSON body");
    return ESP_FAIL;
}

// GET /api/config
esp_err_t PhotoHttpServer::GetConfigHandler(httpd_req_t* req) {
    auto& conf = PhotoManager::GetInstance().GetConfig();
    cJSON* json = cJSON_CreateObject();
    cJSON_AddNumberToObject(json, "interval", conf.interval);
    cJSON_AddStringToObject(json, "transition", conf.transition.c_str());
    cJSON_AddStringToObject(json, "mode", conf.mode.c_str());
    cJSON_AddStringToObject(json, "order", conf.order.c_str());
    char* resp = cJSON_PrintUnformatted(json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    free(resp);
    cJSON_Delete(json);
    return ESP_OK;
}

// POST /api/config
esp_err_t PhotoHttpServer::SetConfigHandler(httpd_req_t* req) {
    int total_len = req->content_len;
    if (total_len <= 0 || total_len > 4096) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid body");
        return ESP_FAIL;
    }
    char* body = (char*)malloc(total_len + 1);
    httpd_req_recv(req, body, total_len);
    body[total_len] = 0;
    cJSON* json = cJSON_Parse(body);
    free(body);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    auto& manager = PhotoManager::GetInstance();
    auto& conf = manager.GetConfig();
    cJSON* item;
    if ((item = cJSON_GetObjectItem(json, "interval")) && cJSON_IsNumber(item)) {
        int v = item->valueint;
        if (v >= 3 && v <= 300) conf.interval = v;
    }
    if ((item = cJSON_GetObjectItem(json, "transition")) && cJSON_IsString(item))
        conf.transition = item->valuestring;
    if ((item = cJSON_GetObjectItem(json, "mode")) && cJSON_IsString(item))
        conf.mode = item->valuestring;
    if ((item = cJSON_GetObjectItem(json, "order")) && cJSON_IsString(item))
        conf.order = item->valuestring;
    manager.SaveConfig();

    cJSON* resp_json = cJSON_CreateObject();
    cJSON_AddBoolToObject(resp_json, "success", true);
    char* resp = cJSON_PrintUnformatted(resp_json);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_send(req, resp, strlen(resp));
    free(resp);
    cJSON_Delete(resp_json);
    cJSON_Delete(json);
    return ESP_OK;
}
