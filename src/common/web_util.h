// Copyright 2026 Citra Emulator Project / Azahar Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "common/common_types.h"

namespace Common {
struct WebResult {
    enum class Code : u32 {
        Success,
        InvalidURL,
        CredentialsMissing,
        LibError,
        HttpError,
        WrongContent,
        NoWebservice,
    };
    Code result_code;
    std::string result_string;
    std::string returned_data;
};

/**
 * @brief Parsed URL components.
 */
struct URLInfo {
    bool is_https;    ///< True if the URL uses HTTPS, false for HTTP.
    std::string host; ///< Hostname or IP address.
    int port;         ///< Network port.
    std::string path; ///< Resource path.
};
URLInfo SplitUrl(const std::string& url);

inline std::string MacToString(u64 mac) {
    return fmt::format("{:02X}:{:02X}:{:02X}:{:02X}:{:02X}:{:02X}", (mac >> (5 * 8)) & 0xFF,
                       (mac >> (4 * 8)) & 0xFF, (mac >> (3 * 8)) & 0xFF, (mac >> (2 * 8)) & 0xFF,
                       (mac >> (1 * 8)) & 0xFF, (mac >> (0 * 8)) & 0xFF);
}

inline std::string MacToString(const std::array<u8, 6>& mac) {
    u64 mac_u64 = u64(mac[0]) << 40 | u64(mac[1]) << 32 | u64(mac[2]) << 24 | u64(mac[3]) << 16 |
                  u64(mac[4]) << 8 | u64(mac[5]);
    return MacToString(mac_u64);
}
} // namespace Common
