# Smart home devices controller
The design allows to controll two devices connected to 230V power socket via a simple website. It gives user an oportunity to choose a mode in which the device should work.<br>
In automatic mode 230V devices are controlled according to indications of a light or temperature sensors. In manual mode user must change states of devices (on/off) by hand.<br>
Project is based on three microcontrollers - ESP8266 NodeMCUv3 as a master controller and two ESP01s as slave controllers.<br>
ESP8266 acts as a server for a simple web page written mostly in plain HTML. ESP01s' are responsible for receiving requests from the server and turning on or off 230V devices using relay.
