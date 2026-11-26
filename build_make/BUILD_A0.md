# Build the final A0 firmware

From the repo root, use GNU Make, a host C++ compiler and Arm GNU 14.2.Rel1.
Set `PICO_SDK_PATH` to Pico SDK 2.3.0 with its TinyUSB, CYW43 and lwIP
submodules initialised. For a fresh checkout, copy the
[Wi-Fi configuration example](../config/wifi_secrets.example.h) to
`config/wifi_secrets.h`. Set the hotspot and broker credentials locally for
live telemetry.

~~~sh
make -C build_make -B -j8 SMP=1 CONSOLE=usb_cdc WIFI=cyw43 NET=lwip MQTT=1 \
     LINE_DEMO=1 LINE_CIRCUIT=1 LINE_AUTOSTART=0 LINE_SEARCH=0 LINE_ADAPTIVE=1 \
     LINE_ROUTE=1 LINE_REMOTE=1 BARCODE_DIGITAL=0 LINE_UNLIMITED=1 TEST=0
~~~

The output is `build_make/mtk3pico_smp1_usb_cdc_wifi_lwip_mqtt_line30_button_adaptive_route_remote_no_time_limit.uf2`.
