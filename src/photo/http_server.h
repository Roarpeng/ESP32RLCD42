#ifndef PHOTO_HTTP_SERVER_H
#define PHOTO_HTTP_SERVER_H

#include <esp_http_server.h>

class PhotoHttpServer {
public:
    static PhotoHttpServer& GetInstance();

    bool Start(uint16_t port = 80);
    void Stop();
    bool IsRunning() const { return running_; }

private:
    PhotoHttpServer() = default;
    httpd_handle_t server_ = nullptr;
    bool running_ = false;

    static esp_err_t IndexHandler(httpd_req_t* req);
    static esp_err_t UploadHandler(httpd_req_t* req);
    static esp_err_t ListPhotosHandler(httpd_req_t* req);
    static esp_err_t BatchDeleteHandler(httpd_req_t* req);
    static esp_err_t GetConfigHandler(httpd_req_t* req);
    static esp_err_t SetConfigHandler(httpd_req_t* req);

    static constexpr int MAX_FILE_SIZE = 2 * 1024 * 1024;
    static constexpr int MAX_BODY_SIZE = 5 * 1024 * 1024;
};

#endif
