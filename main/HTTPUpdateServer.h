#ifndef __HTTP_UPDATE_SERVER_H
#define __HTTP_UPDATE_SERVER_H

#include <esp_log.h>
#include <SPIFFS.h>
#include <StreamString.h>
#include <Update.h>
#include <WebServer.h>

#include "KMPProDinoESP32.h"

static const char serverIndex[] =
    R"(<!DOCTYPE html>
     <html lang='en'>
     <head>
         <meta charset='utf-8'>
         <meta name='viewport' content='width=device-width,initial-scale=1'/>
     </head>
     <body>
     <h1><h1>Nibe MQTT Gateway - OTA</h1>
     <form method='POST' action='' enctype='multipart/form-data'>
         Firmware:<br>
         <input type='file' accept='.bin,.bin.gz' name='firmware'>
         <input type='submit' value='Update Firmware'>
     </form>
     <form method='POST' action='' enctype='multipart/form-data'>
         FileSystem:<br>
         <input type='file' accept='.bin,.bin.gz,.image' name='filesystem'>
         <input type='submit' value='Update FileSystem'>
     </form>
     </body>
     </html>)";
static const char successResponse[] = "<META http-equiv=\"refresh\" content=\"15;URL=/\">Update Success! Rebooting...";

class HTTPUpdateServer {
    static inline const char *TAG = "ota";

   public:
    HTTPUpdateServer() {
        _server = NULL;
        _username = emptyString;
        _password = emptyString;
        _authenticated = false;
    }

    void setup(WebServer *server) { setup(server, emptyString, emptyString); }

    void setup(WebServer *server, const String &path) { setup(server, path, emptyString, emptyString); }

    void setup(WebServer *server, const String &username, const String &password) {
        setup(server, "/update", username, password);
    }

    void setup(WebServer *server, const String &path, const String &username, const String &password) {
        _server = server;
        _username = username;
        _password = password;

        // handler for the /update form page
        _server->on(path.c_str(), HTTP_GET, [&]() { _handleIndex(); });

        // handler for the /update form POST (once file upload finishes)
        _server->on(
            path.c_str(), HTTP_POST, [&]() { _handleUpdate(); }, [&]() { _handleUpload(); });
    }

    void updateCredentials(const String &username, const String &password) {
        _username = username;
        _password = password;
    }

   protected:
    void _setUpdaterError() {
        _updaterError = Update.errorString();
        ESP_LOGE(TAG, "%s", _updaterError.c_str());
    }

    void _handleIndex() {
        if (_username != emptyString && _password != emptyString && !_server->authenticate(_username.c_str(), _password.c_str()))
            return _server->requestAuthentication();
        _server->send_P(200, PSTR("text/html"), serverIndex);
    }

    void _handleUpdate() {
        if (!_authenticated) return _server->requestAuthentication();
        if (Update.hasError()) {
            _server->send(200, "text/html", String("Update error: ") + _updaterError);
        } else {
            _server->client().setNoDelay(true);
            _server->send_P(200, "text/html", successResponse);
            delay(100);
            _server->client().stop();
            ESP.restart();
        }
    }

    // handler for the file upload, get's the sketch bytes, and writes
    // them through the Update object
    void _handleUpload() {
        HTTPUpload &upload = _server->upload();

        if (upload.status == UPLOAD_FILE_START) {
            _updaterError.clear();

            _authenticated = (_username == emptyString || _password == emptyString ||
                              _server->authenticate(_username.c_str(), _password.c_str()));
            if (!_authenticated) {
                ESP_LOGE(TAG, "Unauthenticated Update");
                return;
            }

            ESP_LOGI(TAG, "Update: %s", upload.filename.c_str());
            if (upload.name == "filesystem") {
                if (!Update.begin(SPIFFS.totalBytes(), U_SPIFFS)) {  // start with max available size
                    ESP_LOGE(TAG, "%s", Update.errorString());
                }
            } else {
                uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
                if (!Update.begin(maxSketchSpace, U_FLASH)) {  // start with max available size
                    _setUpdaterError();
                }
            }
        } else if (_authenticated && upload.status == UPLOAD_FILE_WRITE && !_updaterError.length()) {
            // could add blinking LED here
            KMPProDinoESP32.processStatusLed(red, 1000);

            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                _setUpdaterError();
            }
        } else if (_authenticated && upload.status == UPLOAD_FILE_END && !_updaterError.length()) {
            ESP_LOGI(TAG, "Upload finished: %u bytes", upload.totalSize);
            KMPProDinoESP32.setStatusLed(red);
            if (Update.end(true)) {  // true to set the size to the current progress
                ESP_LOGI(TAG, "Update Success - Rebooting...");
            } else {
                _setUpdaterError();
            }
        } else if (_authenticated && upload.status == UPLOAD_FILE_ABORTED) {
            Update.end();
            ESP_LOGW(TAG, "Update was aborted");
        }
        delay(0);
    }

   private:
    WebServer *_server;
    String _username;
    String _password;
    bool _authenticated;
    String _updaterError;
};

#endif