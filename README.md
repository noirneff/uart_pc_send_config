## ESP32-S3 UART Command Handler

Simple ESP32-S3 firmware using ESP-IDF to handle UART events and control an external LED.

UART commands from a PC are parsed to control GPIO state and update UART configuration at runtime.

---

## Hardware

- ESP32-S3
- LED + 220Ω resistor
- USB-UART interface

| GPIO  | Function |
|------|----------|
| GPIO2 | LED      |

---

## Commands

```
LED ON
LED OFF
```

```
UPDATE_CONFIG
BAUDRATE=115200
CHECKSUM=5C
END_CONFIG
```

---

## Implementation

- UART runs in event-driven mode using ESP-IDF UART driver
- A dedicated FreeRTOS task handles UART events via queue
- Incoming data is parsed and dispatched to:
  - GPIO control module
  - UART configuration handler

### UART Events Handled

- UART_DATA
- UART_FIFO_OVF
- UART_BUFFER_FULL
- UART_BREAK
- UART_PARITY_ERR
- UART_FRAME_ERR

Configuration changes are applied only after XOR checksum validation.

---

## Project Structure

```
main/
└── main.c
```

Includes:

- UART initialization
- UART event task
- Command parser
- LED control logic
- Configuration + checksum handling

---

## Example

```
PC  > LED ON
ESP > LED ON

PC  > LED OFF
ESP > LED OFF

PC  > UPDATE_CONFIG
PC  > BAUDRATE=115200
PC  > CHECKSUM=5C
END_CONFIG
ESP > CONFIG UPDATED
```

---

## Tools

- ESP-IDF
- FreeRTOS
- Visual Studio Code
- Hercules Terminal

---

## Notes

This project focuses on UART event-driven design on ESP32-S3.
Instead of polling, all incoming data is handled via UART event queue + FreeRTOS task.

Key idea is separation between:
- transport layer (UART driver)
- processing layer (parser/task)
- application layer (GPIO + config logic)

Checksum is implemented using simple XOR for basic data integrity check.
This project was created while learning embedded firmware development on the ESP32-S3 architecture. The primary focus was on mastering the ESP-IDF UART event queue, parsing non-blocking user commands, and organizing firmware into separate functional blocks instead of handling everything inside a single monolithic loop.
