# Bright Chip

The **Bright Chip** is an advanced ESP8266 chip designed to seamlessly communicate with the Bright App using websockets. It serves as a bridge between the app and the light bulb, providing reliable control and additional functionalities. Here are some key features of the Bright Chip:

## Communication and Webserver

- **Websocket Communication**: The Bright Chip establishes a secure and efficient communication channel with the Bright App through websockets. This allows for real-time control and monitoring of the light bulb's settings and status.

- **Built-in Webserver**: In scenarios where direct communication with the app is not possible, the Bright Chip acts as a webserver. It hosts a small web page that can be accessed locally, enabling you to control and manage the light bulb's functionalities directly.

## Storage and Compression

- **EEPROM for Settings**: The Bright Chip utilizes the EEPROM (Electrically Erasable Programmable Read-Only Memory) to store essential settings such as SSID and passwords. This ensures that your preferences are retained even after power cycles or restarts.

- **SPIFFS Compression**: The chip stores the web page used for configuration and control in a compressed format in SPIFFS (SPI Flash File System). This efficient compression technique optimizes storage utilization and allows for quick retrieval of web resources.

## Additional Functionalities

- **Over-the-Air (OTA) Support**: The Bright Chip facilitates easy firmware updates through OTA, eliminating the need for physical connections or manual flashing. This feature ensures that your chip stays up-to-date with the latest enhancements and bug fixes.

- **MDNS Responder**: With the built-in MDNS (Multicast DNS) responder, the Bright Chip simplifies the process of discovering and connecting to the chip on your local network. This enables effortless setup and integration within your home automation ecosystem.

- **Password Attack Handling**: To enhance security, the Bright Chip implements measures to prevent password attacks. In the event of repeated failed attempts, the chip intelligently blocks the user for a certain period before allowing retry attempts, mitigating potential unauthorized access.

- **Local Switch Input**: The Bright Chip includes an input for a local switch, offering an alternative control method without relying solely on the app. This allows you to conveniently operate the light bulb directly through a physical switch whenever desired.

With its reliable communication, webserver capabilities, storage efficiency, and additional features, the Bright Chip ensures seamless integration between the Bright App and your light bulb, providing a versatile and robust control solution.