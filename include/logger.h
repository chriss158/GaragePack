#ifndef Logger_h
#define Logger_h
#include "Arduino.h"
#include "stdlib_noniso.h"
#include <functional>

#include "ESP8266WiFi.h"
#include "ESPAsyncTCP.h"
#include "ESPAsyncWebServer.h"

#define BUFFER_SIZE 500

typedef std::function<void(uint8_t *data, size_t len)> RecvMsgHandler;

class LoggerClass
{

public:
    void begin(AsyncWebServer *server, const char *url = "/log")
    {
        _server = server;
        _ws = new AsyncWebSocket("/logws");

        /*_server->on(url, HTTP_GET, [](AsyncWebServerRequest *request) {
            // Send Webpage
            AsyncWebServerResponse *response = request->beginResponse_P(200, "text/html", WEBSERIAL_HTML, WEBSERIAL_HTML_SIZE);
            response->addHeader("Content-Encoding","gzip");
            request->send(response);        
            // request->send(SPIFFS, "/log.log", "text/plain");
        });*/

        _ws->onEvent([&](AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) -> void {
            if (type == WS_EVT_CONNECT)
            {
                Serial.println("Client connection received");
            }
            else if (type == WS_EVT_DISCONNECT)
            {
                Serial.println("Client disconnected");
            }
            else if (type == WS_EVT_DATA)
            {
                Serial.println("Received Websocket Data");

                if (_RecvFunc != NULL)
                {
                    _RecvFunc(data, len);
                }
            }
        });

        _server->addHandler(_ws);
    }

    void msgCallback(RecvMsgHandler _recv)
    {
        _RecvFunc = _recv;
    }

    // Print

    void print(String m = "")
    {
        _ws->textAll(m);
        Serial.print(m);
    }

    void print(const char *m)
    {
        _ws->textAll(m);
        Serial.print(m);
    }

    void print(char *m)
    {
        _ws->textAll(m);
        Serial.print(m);
    }

    void print(int m)
    {
        _ws->textAll(String(m));
        Serial.print(String(m));
    }

    void print(uint8_t m)
    {
        _ws->textAll(String(m));
        Serial.print(String(m));
    }

    void print(uint16_t m)
    {
        _ws->textAll(String(m));
        Serial.print(String(m));
    }

    void print(uint32_t m)
    {
        _ws->textAll(String(m));
        Serial.print(String(m));
    }

    void print(double m)
    {
        _ws->textAll(String(m));
        Serial.print(String(m));
    }

    void print(float m)
    {
        _ws->textAll(String(m));
        Serial.print(String(m));
    }

    // Print with New Line

    void println(String m = "")
    {
        _ws->textAll(m + "\n");
        Serial.println(m);
    }

    void println(const char *m)
    {
        _ws->textAll(String(m) + "\n");
        Serial.println(String(m));
    }

    void println(char *m)
    {
        _ws->textAll(String(m) + "\n");
        Serial.println(String(m));
    }

    void println(int m)
    {
        _ws->textAll(String(m) + "\n");
        Serial.println(String(m));
    }

    void println(uint8_t m)
    {
        _ws->textAll(String(m) + "\n");
        Serial.println(String(m));
    }

    void println(uint16_t m)
    {
        _ws->textAll(String(m) + "\n");
        Serial.println(String(m));
    }

    void println(uint32_t m)
    {
        _ws->textAll(String(m) + "\n");
        Serial.println(String(m));
    }

    void println(float m)
    {
        _ws->textAll(String(m) + "\n");
        Serial.println(String(m));
    }

    void println(double m)
    {
        _ws->textAll(String(m) + "\n");
        Serial.println(String(m));
    }

private:
    AsyncWebServer *_server;
    AsyncWebSocket *_ws;
    RecvMsgHandler _RecvFunc = NULL;
};
LoggerClass Logger;
#endif