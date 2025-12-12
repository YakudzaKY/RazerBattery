#pragma once
#include <cstdint>

#define RAZER_USB_REPORT_LEN 0x5A // 90

#pragma pack(push, 1)

union transaction_id_union {
    uint8_t id;
    struct {
        uint8_t device : 3;
        uint8_t id : 5;
    } parts;
};

union command_id_union {
    uint8_t id;
    struct {
        uint8_t direction : 1;
        uint8_t id : 7;
    } parts;
};

struct razer_report {
    uint8_t status;
    union transaction_id_union transaction_id;
    uint16_t remaining_packets; // Big Endian!
    uint8_t protocol_type; // 0x00
    uint8_t data_size;
    uint8_t command_class;
    union command_id_union command_id;
    uint8_t arguments[80];
    uint8_t crc;
    uint8_t reserved;
};

#pragma pack(pop)
