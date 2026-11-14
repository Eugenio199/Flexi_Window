# Flexi_Window

## Connecting the controller to the ESP32

1. Upload `uno esp32/Flexi/Flexi.ino` to the ESP32. No router Wi-Fi credentials are needed.
2. Connect the computer or phone running the controller to `FlexiWindow-Setup` (password: `FlexiWin-49281`).
3. Open `http://192.168.4.1/` in your browser. The ESP32 serves the controller page and API from the same HTTP origin, so the browser allows local control.
4. Enable **Manual Override Mode** to use the open/close control. The page polls the ESP32 for window and sensor state.

The HTTPS site at `https://flexii-smart.netlify.app/` cannot directly call the ESP32's HTTP API because browsers block mixed-content requests. Use the local address above while connected to the ESP32 Wi-Fi. The setup network has no internet, so online weather data may be unavailable.

Change `SETUP_AP_PASSWORD` before use if you want a private setup network. Do not expose the ESP32 directly to the public internet. The displayed window position is an open/closed estimate; this sketch does not use a position sensor.
