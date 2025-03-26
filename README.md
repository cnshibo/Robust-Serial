# Robust Serial

RobustSerial is a lightweight, reliable, and extensible serial communication stack designed for embedded systems. 
It supports UART, SPI, and I2C interfaces and ensures safe data transmission using framing, CRC checks, acknowledgments, and retransmission mechanisms.

🧱 Architecture
<pre>
  +------------------------------------------+
  |              Application                 |
  |  (User's application using the stack)    |
  +------------------------------------------+
  |              RobustStack                 |
  |  (Manages all layers and callbacks)      |
  +------------------------------------------+
  |            Transport Layer               |
  |  - Connection management                 |
  |  - Reliable data transfer                |
  |  - Keep-alive mechanism                  |
  |  - Datagram support                      |
  +------------------------------------------+
  |              Link Layer                  |
  |  - Frame synchronization                 |
  |  - Error detection (CRC)                 |
  |  - Frame   transmission                  |
  +------------------------------------------+
  |            Physical Layer                |
  |  - Serial port management                |
  |  - Byte-level I/O                        |
  |  - Hardware abstraction                  |
  +------------------------------------------+
 </pre>
Features:

✅ Reliable data transfer over UART/SPI/I2C

🔁 ACK/NACK & retransmission for guaranteed delivery

🧱 Modular layered architecture (Physical → Link → Transport)

📦 Packet framing with COBS (or custom encoders)

✅ CRC16 error detection

⏱️ Timeout management for lost packets

🔄 Peer-to-peer connection model

📋 Status reporting (connected, disconnected, timeout, etc.)

🚫 No dynamic memory allocation

⚡ Suitable for bare-metal, FreeRTOS, or RTOS-based systems

🧪 Simple, testable C++ interface

🛠️ Usage:

You can integrate RobustSerial in your embedded projects to ensure safe and reliable serial communication, especially between multiple MCUs or between MCU and host PC.

Example use cases: OTA firmware update, Command/response protocols, Streaming sensor data with integrity

🧱 Portability:

No OS dependencies, No C++ STL, Easy to port to any MCU or platform, Works great with STM32, ESP32, NRF52, etc.
