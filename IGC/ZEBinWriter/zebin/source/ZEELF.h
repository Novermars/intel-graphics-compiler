/*========================== begin_copyright_notice ============================

Copyright (C) 2020-2021 Intel Corporation

SPDX-License-Identifier: MIT

============================= end_copyright_notice ===========================*/

//===- ZEELF.hpp ------------------------------------------------*- C++ -*-===//
// ZE Binary Utilities
//
// \file
// The file define the special enum value for ZE binary
//===----------------------------------------------------------------------===//

#ifndef ZE_ELF_H
#define ZE_ELF_H

#include <cstdint>

namespace zebin {

// ELF machine architecture
enum {
    EM_INTELGT = 205,
};

// ELF section type for ELF32_Shdr::sh_type
enum SHT_ZEBIN : uint32_t
{
    SHT_ZEBIN_SPIRV      = 0xff000009, // .spv section, value the same as SHT_OPENCL_SPIRV
    SHT_ZEBIN_ZEINFO     = 0xff000011, // .ze.info section
    SHT_ZEBIN_GTPIN_INFO = 0xff000012, // .gtpin_info section
    SHT_ZEBIN_VISAASM    = 0xff000013, // .visaasm section
    SHT_ZEBIN_MISC       = 0xff000014  // .misc section
};

// ELF relocation type for ELF32_Rel::ELF32_R_TYPE
enum R_TYPE_ZEBIN
{
    R_ZE_NONE           = 0,
    R_ZE_SYM_ADDR       = 1, // 64-bit type address
    R_ZE_SYM_ADDR_32    = 2, // lower 32-bit of 64-bit address.
    R_ZE_SYM_ADDR_32_HI = 3  // higher 32bits of 64-bit address
};

// ELF note type for INTELGT
enum {
    NT_INTELGT_PRODUCT_FAMILY = 1,
    NT_INTELGT_GFXCORE_FAMILY = 2,
    NT_INTELGT_TARGET_METADATA = 3,
};

struct TargetMetadata {
    // bit[7:0]: dedicated for specific generator (meaning based on generatorId)
    enum GeneratorSpecificFlags : uint8_t {
        NONE = 0
    };
    // bit[23:21]: generator of this device binary
    enum GeneratorId : uint8_t {
        UNREGISTERED = 0,
        IGC          = 1
    };

    union {
        struct {
            // bit[7:0]: dedicated for specific generator (meaning based on generatorId)
            GeneratorSpecificFlags generatorSpecificFlags : 8;
            // bit[12:8]: values [0-31], min compatbile device revision Id (stepping)
            uint8_t minHwRevisionId : 5;
            // bit[13:13]:
            // 0 - full validation during decoding (safer decoding)
            // 1 - no validation (faster decoding - recommended for known generators)
            bool validateRevisionId : 1;
            // bit[14:14]:
            // 0 - ignore minHwRevisionId and maxHwRevisionId
            // 1 - underlying device must match specified revisionId info
            bool disableExtendedValidation : 1;
            // bit[15:15]: reserved bit for future use
            bool reservedBit : 1;
            // bit[20:16]:  max compatible device revision Id (stepping)
            uint8_t maxHwRevisionId : 5;
            // bit[23:21]: generator of this device binary. Value defined in above GeneratorId
            uint8_t generatorId : 3;
            // bit[31:24]: MBZ, reserved for future use
            uint8_t reserved : 8;
        };
        uint32_t packed = 0U;
    };
};

static_assert(sizeof(TargetMetadata) == sizeof(uint32_t), "TargetMetadata should be packed");

} // namespace zebin

#endif // ZE_ELF_H
