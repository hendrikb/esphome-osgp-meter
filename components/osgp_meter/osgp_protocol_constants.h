#pragma once

#include <cstddef>
#include <cstdint>

namespace esphome {
namespace osgp_meter {
namespace protocol {

inline constexpr uint8_t NACK = 0x15;
inline constexpr uint8_t ACK = 0x06;
inline constexpr uint8_t START = 0xEE;
inline constexpr uint8_t IDENTITY = 0x00;

inline constexpr uint8_t REQUEST_ID_IDENT = 0x20;
inline constexpr uint8_t REQUEST_ID_TERMINATE = 0x21;
inline constexpr uint8_t REQUEST_ID_READ = 0x30;
inline constexpr uint8_t REQUEST_ID_READ_PARTIAL = 0x3F;
inline constexpr uint8_t REQUEST_ID_LOGON = 0x50;
inline constexpr uint8_t REQUEST_ID_SECURITY = 0x51;
inline constexpr uint8_t REQUEST_ID_LOGOFF = 0x52;
inline constexpr uint8_t REQUEST_ID_NEGOTIATE2 = 0x61;

inline constexpr uint8_t BAUD_9600_ENUM = 0x06;

inline constexpr uint32_t RESPONSE_TIMEOUT_MS = 500;
inline constexpr uint32_t INTER_BYTE_TIMEOUT_MS = 50;
inline constexpr uint32_t MIN_REQUEST_INTERVAL_MS = 100;
inline constexpr uint32_t RX_QUIET_TIME_MS = 50;
inline constexpr uint8_t SEND_RETRY_COUNT = 3;
inline constexpr uint8_t RECEIVE_RETRY_COUNT = 10;
inline constexpr uint8_t START_SCAN_TRIES = 100;
inline constexpr uint16_t SEGMENT_SIZE_BYTES = 117;
inline constexpr uint32_t SEGMENT_DELAY_MS = 25;
inline constexpr uint8_t WAKEUP_BYTE = 0x55;
inline constexpr uint8_t WAKEUP_COUNT = 4;
inline constexpr uint32_t WAKEUP_DELAY_MS = 50;
inline constexpr uint32_t UNKNOWN_SEQUENCE_WINDOW_MS = 100;
inline constexpr size_t UNKNOWN_SEQUENCE_CAPTURE_MAX = 32;

enum UnknownContext : uint8_t {
  UNKNOWN_PRE_SEND = 1,
  UNKNOWN_START_SCAN = 2,
  UNKNOWN_RESPONSE = 3,
};

}  // namespace protocol
}  // namespace osgp_meter
}  // namespace esphome
