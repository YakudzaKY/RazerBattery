#pragma once

#include <windows.h>
#include <string>
#include <vector>
#include <hidsdi.h>
#include <setupapi.h>

class RazerDevice {
public:
    RazerDevice(const std::wstring& devicePath);
    ~RazerDevice();

    bool Connect();
    void Disconnect();
    bool IsConnected() const;

    std::wstring GetDevicePath() const;
    unsigned short GetPID() const;

    // Returns -1 on failure, 0-100 on success
    int GetBatteryLevel();
    // Returns -1 on failure, 0 = not charging, 1 = charging
    int GetChargingStatus();

    std::wstring GetDeviceType() const;
    bool IsRazerControlInterface();

private:
    HANDLE m_hDevice;
    std::wstring m_devicePath;
    unsigned short m_pid;
    unsigned char m_transactionId = 0; // Cached working transaction ID
    bool m_isReadOnly = false;

    unsigned char GetTransactionID() const;
    void CloseHandleSafe();

    // Razer protocol constants
    static const int RAZER_USB_REPORT_LEN = 90;
    static const int USB_VENDOR_ID_RAZER = 0x1532;

    #pragma pack(push, 1)
    struct RazerReport {
        unsigned char status;
        unsigned char transaction_id;
        unsigned short remaining_packets; // Big Endian
        unsigned char protocol_type;
        unsigned char data_size;
        unsigned char command_class;
        unsigned char command_id;
        unsigned char arguments[80];
        unsigned char crc;
        unsigned char reserved;
    };
    #pragma pack(pop)

    bool SendRequest(RazerReport& request, RazerReport& response);
    unsigned char CalculateCRC(const RazerReport& report);
};
