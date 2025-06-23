#include "index.h"

/************************************************
 * WS
 ***********************************************/
#include <WebSocketsServer.h>

WebSocketsServer ws_server = WebSocketsServer(81);
void		*ws_server_parent;
void (*ws_server_connection_handler)(void *parent);
void (*ws_server_reception_handler)(void *parent, uint8_t *);
uint8_t ws_server_current_client;

void ws_server_send(uint8_t dst, const char *payload)
{
	ws_server.sendTXT(dst, payload);
}

void ws_server_send_all(int16_t src, const char *payload)
{
	for (uint8_t i = 0; i < WEBSOCKETS_SERVER_CLIENT_MAX; i++) {
		if (!ws_server.clientIsConnected(i))
			continue;

		if (i == src)
			continue;

		ws_server.sendTXT(i, payload);
	}
}

void ws_server_event(uint8_t num, WStype_t type, uint8_t *payload,
		     size_t length)
{
	ws_server_current_client = num;

	switch (type) {
	case WStype_DISCONNECTED:
		printf("[%u] Disconnected!\n", num);
		break;
	case WStype_CONNECTED: {
		IPAddress ip = ws_server.remoteIP(num);
		printf("[%u] Connected from %d.%d.%d.%d url: %s\n", num, ip[0],
		       ip[1], ip[2], ip[3], payload);

		ws_server_connection_handler(ws_server_parent);
		// ws_server.sendTXT(num, "Connected");
		break;
	}

	case WStype_TEXT:
		printf("[%u] get Text: %s\n", num, payload);
		ws_server_reception_handler(ws_server_parent, payload);
		// ws_server.broadcastTXT(payload);

		break;

	default:
		break;
	}
}

/************************************************
 * WEB
 ***********************************************/
#include <DNSServer.h>
#include <Update.h>
#include <WebServer.h>
#include <WiFi.h>
#include <Ticker.h>

Ticker restart_ticker;
DNSServer dns_server;
WebServer web_server(80);

void web_ui_send_index()
{
	web_server.sendHeader("Content-Encoding", "gzip");
	web_server.sendHeader("Content-Type", "text/html");

	web_server.send_P(200, "text/html",
			  (const char *)index_html_gz,
					index_html_gz_len);
	return;
}

void web_ui_send_ok()
{
	web_server.sendHeader("Access-Control-Allow-Origin", "*");
	web_server.send(200);
}

void web_ui_safe_restart()
{
	ESP.restart();
}

void web_ui_update_firmware()
{
	uint32_t free_space = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
	HTTPUpload &upload  = web_server.upload();

	if (upload.status == UPLOAD_FILE_START) {
		if (Update.begin(free_space)) {
			Serial.println("Update begin success");
		} else {
			Serial.println("Update begin fail");
		}
	} else if (upload.status == UPLOAD_FILE_WRITE) {
		if (Update.write(upload.buf, upload.currentSize))
			Serial.println("Update write ok...");
	} else if (upload.status == UPLOAD_FILE_END) {
		if (Update.end(true)) {
			Serial.println("Update end success");
			restart_ticker.once(1.0, web_ui_safe_restart);
		} else {
			Serial.println("Update end fail");
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
	web_server.on("/update", HTTP_POST, web_ui_send_ok,
		      web_ui_update_firmware);

	dns_server.start(53, "*", WiFi.softAPIP());
	web_server.begin();

	ws_server.begin();
	ws_server.onEvent(ws_server_event);
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
	web_server.handleClient();
	ws_server.loop();
}
