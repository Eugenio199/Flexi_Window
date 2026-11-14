# Flexi_Window

## Connecting the controller to the ESP32

1. In `uno esp32/Flexi/Flexi.ino`, set `WIFI_SSID` and `WIFI_PASSWORD` to your 2.4 GHz Wi-Fi credentials, then upload the sketch to the ESP32.
2. The ESP32 starts its own setup Wi-Fi: `FlexiWindow-Setup` (password: `FlexiWin-49281`). Join it from the computer or phone running the controller.
3. Open `controller/index.html` in a browser. It automatically polls `192.168.4.1` and connects when the ESP32 setup Wi-Fi is available. The **RETRY** button can retry immediately, and the setup Wi-Fi/API remain available even if router Wi-Fi fails.
4. To use the router network instead, keep the controller device on the same Wi-Fi and enter the ESP32's router IP shown in the Serial Monitor at 115200 baud.
5. Enable **Manual Override Mode** to use the open/close control. The page polls the ESP32 for window and sensor state.

Change `SETUP_AP_PASSWORD` before use if you want a private setup network. The controller communicates with the ESP32 over HTTP on your local network. Do not expose the ESP32 directly to the public internet. The displayed window position is an open/closed estimate; this sketch does not use a position sensor.