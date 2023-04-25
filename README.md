# IoT-Twin-Lamps
Communicating Twin Lamps made with Arduino

Based on [DevJav's approach](https://github.com/DevJav/best_friend_lamp_arduino.git)

Material used per lamp:
  - 1 NeoPixel 24 led ring
  - 1 ESP8266
  - 1 Touch Capacitor TTP223
  - Custom wood frame + stainless steel base and top where the capacitor will be connected to

Added some animations and features like a way to reset the wifi connection without flashing a new program. Holding the top while connecting it to a socket will trigger a WiFi reset and a new configuration.

#Turning on, connecting to WiFi and Adafruit
![](https://github.com/fabiot16/IoT-Twin-Lamps/blob/main/gifs/on_connect.gif)

#Colors and the normal mode (no animation)
![](https://github.com/fabiot16/IoT-Twin-Lamps/blob/main/gifs/colors_normalmode.gif)

#Send a pulse everytime you touch the sensor after the connection is made
![](https://github.com/fabiot16/IoT-Twin-Lamps/blob/main/gifs/send_pulse.gif)

#Fireplace mode
![](https://github.com/fabiot16/IoT-Twin-Lamps/blob/main/gifs/fireplace_mode.gif)
