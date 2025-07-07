#include "index.h"

#include <ESPAsyncWebServer.h>

/************************************************
 * WS
 ***********************************************/
static AsyncWebSocket ws_server("/ws");

void  *ws_server_parent;
void (*ws_server_connection_handler)(void *parent);
void (*ws_server_reception_handler)(void *parent, uint8_t *);
uint8_t ws_server_current_client;

void ws_server_send(uint8_t dst, const char *payload)
{
	ws_server.text(ws_server_current_client, payload);
}

void ws_server_send_all(int16_t src, const char *payload)
{
	ws_server.textAll(payload);
}

void ws_server_handler(AsyncWebSocket *server, AsyncWebSocketClient *client,
		       AwsEventType type, void *arg, uint8_t *data, size_t len)
{
	(void)len;
	ws_server_current_client = client->id();

	if (type == WS_EVT_CONNECT) {
		Serial.println("[WEBSOCKET] client connected");
		client->setCloseClientOnQueueFull(false);
		client->ping();

		ws_server_connection_handler(ws_server_parent);
	} else if (type == WS_EVT_DISCONNECT) {
		Serial.println("[WEBSOCKET] client disconnected");
	} else if (type == WS_EVT_ERROR) {
		Serial.println("[WEBSOCKET] error");
	} else if (type == WS_EVT_PONG) {
		Serial.println("[WEBSOCKET] pong");
	} else if (type == WS_EVT_DATA) {
		AwsFrameInfo *info = (AwsFrameInfo *)arg;
		String msg = "";
		if (info->final && info->index == 0 && info->len == len) {
			if (info->opcode == WS_TEXT) {
				data[len] = 0;
				ws_server_reception_handler(ws_server_parent, (unsigned char *)data);
				Serial.printf("[WEBSOCKET] text: %s\n", (unsigned char *)data);
			}
		}
	}
}

/************************************************
 * WEB
 ***********************************************/
#include <DNSServer.h>
#include <Update.h>
#include <WiFi.h>
#include <Ticker.h>

Ticker restart_ticker;
DNSServer dns_server;
static AsyncWebServer web_server(80);
bool web_arduino_esp32_update_success = false;

void web_ui_send_index(AsyncWebServerRequest *request)
{
	AsyncWebServerResponse *response = request->beginResponse(200, "text/html", index_html_gz, index_html_gz_len);
	response->addHeader("Content-Encoding", "gzip");
	request->send(response);
}

void web_ui_send_ok(AsyncWebServerRequest *request)
{
	request->send(200);
}

void web_ui_safe_restart()
{
	ESP.restart();
}

static void web_ui_handle_cors_preflight(AsyncWebServerRequest *request)
{
	AsyncWebServerResponse *response = request->beginResponse(204, "text/plain");

	response->addHeader("Access-Control-Allow-Origin", "*");
	/* response->addHeader("Access-Control-Allow-Headers", "Content-Type, X-FileSize"); */

	request->send(response);
}

static void web_ui_handle_firmware_upload(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
	if (index == 0) {
		/* size_t filesize = request->header("X-FileSize").toInt();
		Serial.printf("Update: '%s' size %u\n", filename.c_str(), filesize);
		if (!Update.begin(filesize, U_FLASH)) { // pass the size provided */
		uint32_t free_space = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;

		if (Update.begin(free_space)) {
			Update.printError(Serial);
		}
	}

	if (len) {
		Serial.printf("writing %d\n", len);
		if (Update.write(data, len) == len) {
		} else {
			Serial.printf("write failed to write %d\n", len);
		}
	}

	if (final) {
		if (Update.end(true)) {
			web_arduino_esp32_update_success = true;
			Serial.println("Update end success");
			restart_ticker.once(1.0, web_ui_safe_restart);
			AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", "Update successful. Rebooting...");
			response->addHeader("Access-Control-Allow-Origin", "*");
			request->send(response);
		} else {
			web_arduino_esp32_update_success = false;
			Serial.println("Update end fail");
			Update.printError(Serial);
			AsyncWebServerResponse *response = request->beginResponse(500, "text/plain", "Update failed.");
			response->addHeader("Access-Control-Allow-Origin", "*");
			request->send(response);
		}
	}
}

void web_arduino_esp32_init(const char *name, bool ap_sta)
{
	if (ap_sta) {
		WiFi.mode(WIFI_AP_STA);
		WiFi.config(IPAddress(192, 168, 1, 37),
			    IPAddress(192, 168, 1, 1),
			    IPAddress(255, 255, 255, 0));
		WiFi.begin("netis", "novaservis");
		// WiFi.begin("soni2", "0987w456321s");
	} else {
		WiFi.mode(WIFI_AP);
	}

	WiFi.softAPConfig(IPAddress(10, 10, 10, 10), IPAddress(10, 10, 10, 10),
			  IPAddress(255, 255, 255, 0));
	WiFi.softAP(name);

	web_server.onNotFound(web_ui_send_index);
	web_server.on("/update", HTTP_POST,
		     [](AsyncWebServerRequest *request){},
		     web_ui_handle_firmware_upload);
	web_server.on("/update", HTTP_OPTIONS, web_ui_handle_cors_preflight);

	dns_server.start(53, "*", WiFi.softAPIP());

	ws_server.onEvent(ws_server_handler);
	web_server.addHandler(&ws_server);
	
	web_server.begin();
}

void web_arduino_esp32_send(uint8_t dst, const char *payload)
{
	ws_server_send(dst, payload);
}

void web_arduino_esp32_send_all(int16_t src, const char *payload)
{
	ws_server_send_all(src, payload);
}

void web_arduino_esp32_set_parent_object(void *obj) { ws_server_parent = obj; }

void web_arduino_esp32_set_connection_handler(void (*handler)(void *))
{
	ws_server_connection_handler = handler;
}

void web_arduino_esp32_set_reception_handler(void (*handler)(void *, uint8_t *))
{
	ws_server_reception_handler = handler;
}

uint8_t web_arduino_esp32_get_client() { return ws_server_current_client; }

void web_arduino_esp32_update()
{
	dns_server.processNextRequest();
}
