/*
 *	wifi_secrets.example.h
 *
 *	Copy this file to wifi_secrets.h (same directory) and fill in your own
 *	network's credentials. wifi_secrets.h is listed in .gitignore -- it
 *	will never be committed, so it's safe to put your real password there.
 *
 *	Only used when built with NET=lwip (see build_make/pico_rp2040.mk and
 *	lib/libwifi/sysdepend/pico_rp2040/cyw43_utk.c).
 */

#ifndef WIFI_SECRETS_H
#define WIFI_SECRETS_H

#define WIFI_SSID     "your-network-name"
#define WIFI_PASSWORD "your-network-password"

/* Only used when built with MQTT=1 (also requires NET=lwip). */
#define MQTT_BROKER_IP   "192.168.1.10"
#define MQTT_BROKER_PORT 1884
#define MQTT_CLIENT_ID   "se08-car01-pico"
/* Set matching credentials in the laptop broker. Never commit real values. */
#define MQTT_USERNAME    "se08-car01"
#define MQTT_PASSWORD    "replace-with-local-broker-password"
#define MQTT_TOPIC_BASE  "inf2004/se08/car01/v1"
#define MQTT_TOPIC       "inf2004/se08/car01/v1/status"

#endif /* WIFI_SECRETS_H */
