/* WARNING: NON-MISRA COMPLIANT (TODO) */
#pragma once

#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <Update.h>
#include <WiFi.h>
#include <Ticker.h>

#include "index.h" /* Web page (generated/compressed) */

/******************************************************************************
 * GLOBALS
 *****************************************************************************/
static AsyncWebSocket web_socket("/ws");
static AsyncWebServer web_server(80);
static DNSServer      dns_server;

bool leaf_can_filter_web_update_success = false;
Ticker leaf_can_filter_web_cpu_reset_ticker;

struct leaf_can_filter *leaf_can_filter_web_instance = NULL;

/******************************************************************************
 * OTHER HAL
 *****************************************************************************/
bool leaf_can_filter_web_cpu_reset_scheduled = false;
void leaf_can_filter_web_cpu_reset_func()
{
	ESP.restart();
}
void leaf_can_filter_web_cpu_reset(struct leaf_can_filter *self)
{
	leaf_can_filter_web_cpu_reset_scheduled = true;
	leaf_can_filter_web_cpu_reset_ticker.once(5.0,
					   leaf_can_filter_web_cpu_reset_func);
}

/******************************************************************************
 * MESSAGES
 *****************************************************************************/
#include <ArduinoJson.h>

enum leaf_can_filter_web_msg_type {
	LEAF_CAN_FILTER_WEB_MSG_TYPE_CPU_RESET,

	LEAF_CAN_FILTER_WEB_MSG_MAX
};

void leaf_can_filter_web_msg(struct leaf_can_filter *self,
			     const char *payload)
{
	uint8_t type;
	
	JsonDocument doc;
	JsonArray    array;
	JsonVariant  value;
	
	deserializeJson(doc, payload);

	array = doc.as<JsonArray>();
	type  = array[0].as<int>();
	value = array[1];

	switch (type) {
	case LEAF_CAN_FILTER_WEB_MSG_TYPE_CPU_RESET:
		leaf_can_filter_web_cpu_reset(self);
		return; /* Prevents default send action, down there. */

	default:
		break;
	}

	web_socket.textAll(payload);
}

void leaf_can_filter_web_send_update(struct leaf_can_filter *self)
{
	String	serialized;

	JsonDocument doc;
	JsonArray	    array = doc.to<JsonArray>();
	JsonArray	    narr;
	/* JsonObject	    nobj; */

	narr = array.add<JsonArray>();
	narr.add(1337);
	narr.add("HELLO!");

	serializeJson(array, serialized);
	web_socket.textAll(serialized.c_str());
}

/******************************************************************************
 * WEB SOCKET
 *****************************************************************************/
void web_socket_handler(AsyncWebSocket *server, AsyncWebSocketClient *client,
		       AwsEventType type, void *arg, uint8_t *data, size_t len)
{
	switch (type) {
	case WS_EVT_CONNECT:
		Serial.println("[WEBSOCKET] client connected");
		client->setCloseClientOnQueueFull(false);
		client->ping();

		break;

	case WS_EVT_DISCONNECT:
		Serial.println("[WEBSOCKET] client disconnected");

		break;

	case WS_EVT_ERROR:
		Serial.println("[WEBSOCKET] error");

		break;
	
	case WS_EVT_PONG:
		Serial.println("[WEBSOCKET] pong");

		break;
	
	case WS_EVT_DATA: {
		AwsFrameInfo *info = (AwsFrameInfo *)arg;
		if (info->final && (info->index == 0) && (info->len == len) &&
		    (info->opcode == WS_TEXT)) {
			data[len] = 0;
			leaf_can_filter_web_msg(leaf_can_filter_web_instance,
						(const char *)data);
			Serial.printf("[WEBSOCKET] text: %s\n",
				       (const char *)data);
		}
		
		break;
	}
	
	default:
		break;
	}
}

/******************************************************************************
 * WEB SERVER
 *****************************************************************************/
void web_server_send_index(AsyncWebServerRequest *request)
{
	AsyncWebServerResponse *response =
		request->beginResponse(200, "text/html", index_html_gz,
				       index_html_gz_len);
	
	response->addHeader("Content-Encoding", "gzip");
	
	request->send(response);
}

void web_server_send_ok(AsyncWebServerRequest *request)
{
	request->send(200);
}

static void web_server_handle_cors_preflight(AsyncWebServerRequest *request)
{
	AsyncWebServerResponse *response =
				request->beginResponse(204, "text/plain");

	response->addHeader("Access-Control-Allow-Origin", "*");
	/* response->addHeader("Access-Control-Allow-Headers", "Content-Type,
	 * 			X-FileSize"); */

	request->send(response);
}

static void web_server_handle_firmware_upload(
			AsyncWebServerRequest *request, const String& filename,
			size_t index, uint8_t *data, size_t len,
			bool final)
{
	if (index == 0) {
		/* size_t filesize = request->header("X-FileSize").toInt();
		Serial.printf("Update: '%s' size %u\n",
			      filename.c_str(), filesize);
		if (!Update.begin(filesize, U_FLASH)) {
			// pass the size provided */
		uint32_t free_space =
			(ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;

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
			leaf_can_filter_web_update_success = true;
			Serial.println("Update end success");
			leaf_can_filter_web_cpu_reset(
						leaf_can_filter_web_instance);

			AsyncWebServerResponse *response =
				request->beginResponse(200, "text/plain",
					    "Update successful. Rebooting...");

			response->addHeader("Access-Control-Allow-Origin",
					    "*");

			request->send(response);
		} else {
			leaf_can_filter_web_update_success = false;
			Serial.println("Update end fail");
			Update.printError(Serial);

			AsyncWebServerResponse *response =
				request->beginResponse(500, "text/plain",
						      "Update failed.");

			response->addHeader("Access-Control-Allow-Origin",
					    "*");

			request->send(response);
		}
	}
}

/******************************************************************************
 * PUBLIC
 *****************************************************************************/
void leaf_can_filter_web_init(struct leaf_can_filter *self)
{
	leaf_can_filter_web_instance = self;

	/* AP config */
	WiFi.mode(WIFI_AP);

	WiFi.softAPConfig(IPAddress(10, 10, 10, 10), IPAddress(10, 10, 10, 10),
			  IPAddress(255, 255, 255, 0));
	WiFi.softAP("LeafBOX");

	/* Server config */
	web_server.onNotFound(web_server_send_index);

	web_server.on("/update", HTTP_POST,
		     [](AsyncWebServerRequest *request){},
		     web_server_handle_firmware_upload);

	web_server.on("/update", HTTP_OPTIONS,
		      web_server_handle_cors_preflight);

	web_socket.onEvent(web_socket_handler);
	web_server.addHandler(&web_socket);
	
	web_server.begin();

	/* DNS */
	dns_server.start(53, "*", WiFi.softAPIP());
}

void leaf_can_filter_web_update(struct leaf_can_filter *self,
				uint32_t delta_time_ms)
{
	static int32_t repeat_ms = 0;
	if (repeat_ms >= 0) {
		repeat_ms -= delta_time_ms;
	}
	
	if (repeat_ms <= 0) {
		repeat_ms = 1000;

		leaf_can_filter_web_send_update(self);
	}

	dns_server.processNextRequest();
}
