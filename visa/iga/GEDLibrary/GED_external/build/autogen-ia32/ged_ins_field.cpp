/*========================== begin_copyright_notice ============================

Copyright (C) 2017-2021 Intel Corporation

SPDX-License-Identifier: MIT

============================= end_copyright_notice ===========================*/

/*
 * !!! DO NOT EDIT THIS FILE !!!
 *
 * This file was automagically crafted by GED's model parser.
 */

#include "ged_enumerations_internal.h"
#include "ged_ins_field_internal.h"
#include "ged_ins_field.h"
GED_FIELD_TYPE fieldTypesByField[128] =
{
    0x3, // 0
    0x0, // 1
    0x0, // 2
    0x8000, // 3
    0x0, // 4
    0x0, // 5
    0x103, // 6
    0x103, // 7
    0x103, // 8
    0x103, // 9
    0x103, // 10
    0x103, // 11
    0x103, // 12
    0x103, // 13
    0x102, // 14
    0x103, // 15
    0x103, // 16
    0x103, // 17
    0x103, // 18
    0x103, // 19
    0x103, // 20
    0x103, // 21
    0x103, // 22
    0x103, // 23
    0x103, // 24
    0x103, // 25
    0x100, // 26
    0x108, // 27
    0x100, // 28
    0x100, // 29
    0x102, // 30
    0x103, // 31
    0x100, // 32
    0x100, // 33
    0x108, // 34
    0x100, // 35
    0x100, // 36
    0x103, // 37
    0x103, // 38
    0x102, // 39
    0x102, // 40
    0x102, // 41
    0x100, // 42
    0x100, // 43
    0x100, // 44
    0x100, // 45
    0x108, // 46
    0x100, // 47
    0x100, // 48
    0x103, // 49
    0x103, // 50
    0x102, // 51
    0x102, // 52
    0x102, // 53
    0x104, // 54
    0x103, // 55
    0x103, // 56
    0x103, // 57
    0x103, // 58
    0x103, // 59
    0x100, // 60
    0x100, // 61
    0x100, // 62
    0x103, // 63
    0x103, // 64
    0x102, // 65
    0x103, // 66
    0x103, // 67
    0x103, // 68
    0x100, // 69
    0x100, // 70
    0x102, // 71
    0x102, // 72
    0x102, // 73
    0x100, // 74
    0x100, // 75
    0x103, // 76
    0x103, // 77
    0x108, // 78
    0x108, // 79
    0x100, // 80
    0x100, // 81
    0x100, // 82
    0x100, // 83
    0x100, // 84
    0x100, // 85
    0x103, // 86
    0x103, // 87
    0x103, // 88
    0x103, // 89
    0x103, // 90
    0x103, // 91
    0x100, // 92
    0x103, // 93
    0x103, // 94
    0x100, // 95
    0x100, // 96
    0x100, // 97
    0x100, // 98
    0x100, // 99
    0x100, // 100
    0x102, // 101
    0x100, // 102
    0x103, // 103
    0x103, // 104
    0x103, // 105
    0x104, // 106
    0x104, // 107
    0x102, // 108
    0x100, // 109
    0x100, // 110
    0x100, // 111
    0x100, // 112
    0x103, // 113
    0x103, // 114
    0x100, // 115
    0x100, // 116
    0x102, // 117
    0x102, // 118
    0x103, // 119
    0x103, // 120
    0x103, // 121
    0x103, // 122
    0x100, // 123
    0x100, // 124
    0x100, // 125
    0x100, // 126
    0x100 // 127
}; // fieldTypesByField[]

#if GED_VALIDATION_API
const char* fieldNameByField[128] =
{
    "Opcode", // 0
    "CmptCtrl", // 1
    "", // 2
    "Reserved", // 3
    "NumOfSourceOperands", // 4
    "HasDestinationOperand", // 5
    "AccessMode", // 6
    "MaskCtrl", // 7
    "DepCtrl", // 8
    "ExecMaskOffsetCtrl", // 9
    "ChannelOffset", // 10
    "ThreadCtrl", // 11
    "PredCtrl", // 12
    "PredInv", // 13
    "ExecSize", // 14
    "CondModifier", // 15
    "AccWrCtrl", // 16
    "DebugCtrl", // 17
    "Saturate", // 18
    "DstRegFile", // 19
    "DstDataType", // 20
    "Src0RegFile", // 21
    "Src0DataType", // 22
    "Src1RegFile", // 23
    "Src1DataType", // 24
    "DstChanEn", // 25
    "DstSubRegNum", // 26
    "DstAddrImm", // 27
    "DstRegNum", // 28
    "DstAddrSubRegNum", // 29
    "DstHorzStride", // 30
    "DstAddrMode", // 31
    "Src0ChanSel", // 32
    "Src0SubRegNum", // 33
    "Src0AddrImm", // 34
    "Src0RegNum", // 35
    "Src0AddrSubRegNum", // 36
    "Src0SrcMod", // 37
    "Src0AddrMode", // 38
    "Src0HorzStride", // 39
    "Src0Width", // 40
    "Src0VertStride", // 41
    "FlagSubRegNum", // 42
    "FlagRegNum", // 43
    "Src1ChanSel", // 44
    "Src1SubRegNum", // 45
    "Src1AddrImm", // 46
    "Src1RegNum", // 47
    "Src1AddrSubRegNum", // 48
    "Src1SrcMod", // 49
    "Src1AddrMode", // 50
    "Src1HorzStride", // 51
    "Src1Width", // 52
    "Src1VertStride", // 53
    "Imm", // 54
    "Src2SrcMod", // 55
    "SrcDataType", // 56
    "Src0RepCtrl", // 57
    "Src1RepCtrl", // 58
    "Src2RepCtrl", // 59
    "Src2ChanSel", // 60
    "Src2SubRegNum", // 61
    "Src2RegNum", // 62
    "Src2RegFile", // 63
    "Src2AddrMode", // 64
    "Src2VertStride", // 65
    "SFID", // 66
    "DescRegFile", // 67
    "DescDataType", // 68
    "DescAddrSubRegNum", // 69
    "DescRegNum", // 70
    "DescHorzStride", // 71
    "DescWidth", // 72
    "DescVertStride", // 73
    "MsgDesc", // 74
    "ExMsgDescImm", // 75
    "EOT", // 76
    "MathFC", // 77
    "JIP", // 78
    "UIP", // 79
    "ControlIndex", // 80
    "DataTypeIndex", // 81
    "SubRegIndex", // 82
    "Src0Index", // 83
    "Src1Index", // 84
    "DescIndex", // 85
    "ExDescRegFile", // 86
    "DstMathMacroExt", // 87
    "Src0MathMacroExt", // 88
    "Src1MathMacroExt", // 89
    "Src2MathMacroExt", // 90
    "BranchCtrl", // 91
    "SourceIndex", // 92
    "Src2DataType", // 93
    "NoSrcDepSet", // 94
    "ExFuncCtrl", // 95
    "ExMsgLength", // 96
    "ExDescAddrSubRegNum", // 97
    "ExDescRegNum", // 98
    "MsgDescCategory", // 99
    "MsgDescScratchAddrOffset", // 100
    "MsgDescScratchBlockSize", // 101
    "MsgDescScratchInvalidateAfterRead", // 102
    "MsgDescScratchChannelMode", // 103
    "MsgDescScratchMessageType", // 104
    "ExecutionDataType", // 105
    "Src0TernaryImm", // 106
    "Src2TernaryImm", // 107
    "Src2HorzStride", // 108
    "SWSB", // 109
    "Src1IsImm", // 110
    "Src0IsImm", // 111
    "Src0SubRegNumByte", // 112
    "SyncFC", // 113
    "FusionCtrl", // 114
    "DataTypeIndexNoDep", // 115
    "CompactedImm", // 116
    "RepeatCount", // 117
    "SystolicDepth", // 118
    "Src2Precision", // 119
    "Src2SubBytePrecision", // 120
    "Src1Precision", // 121
    "Src1SubBytePrecision", // 122
    "BfnFC", // 123
    "ExBSO", // 124
    "CPS", // 125
    "Src1Length", // 126
    "Src2IsImm" // 127
}; // fieldNameByField[]
#endif // GED_VALIDATION_API
GED_FIELD_TYPE pseudoFieldTypesByField[45] =
{
    0x103, // 0
    0x100, // 1
    0x103, // 2
    0x103, // 3
    0x103, // 4
    0x103, // 5
    0x100, // 6
    0x100, // 7
    0x103, // 8
    0x103, // 9
    0x103, // 10
    0x103, // 11
    0x103, // 12
    0x103, // 13
    0x103, // 14
    0x103, // 15
    0x103, // 16
    0x100, // 17
    0x103, // 18
    0x103, // 19
    0x103, // 20
    0x103, // 21
    0x103, // 22
    0x103, // 23
    0x103, // 24
    0x103, // 25
    0x103, // 26
    0x100, // 27
    0x100, // 28
    0x103, // 29
    0x100, // 30
    0x103, // 31
    0x103, // 32
    0x103, // 33
    0x103, // 34
    0x100, // 35
    0x103, // 36
    0x103, // 37
    0x103, // 38
    0x103, // 39
    0x103, // 40
    0x100, // 41
    0x103, // 42
    0x103, // 43
    0x103 // 44
}; // pseudoFieldTypesByField[]

#if GED_VALIDATION_API
const char* fieldNameByPseudoField[45] =
{
    "ArchReg", // 0
    "ArchRegNum", // 1
    "SwizzleX", // 2
    "SwizzleY", // 3
    "SwizzleZ", // 4
    "SwizzleW", // 5
    "MessageLength", // 6
    "ResponseLength", // 7
    "HeaderPresent", // 8
    "MessageTypeDP_SAMPLER", // 9
    "MessageTypeDP_RC", // 10
    "MessageTypeDP_CC", // 11
    "MessageTypeDP_DC0", // 12
    "TypedSurfaceSlotGroup", // 13
    "TypedAtomicSlotGroup", // 14
    "UntypedSurfaceSIMDMode", // 15
    "UntypedAtomicSIMDMode", // 16
    "InvalidateAfterRead", // 17
    "BlockSize", // 18
    "RedChannel", // 19
    "GreenChannel", // 20
    "BlueChannel", // 21
    "AlphaChannel", // 22
    "ReturnDataControl", // 23
    "AtomicOperationType", // 24
    "AtomicCounterOperationType", // 25
    "SubFuncID", // 26
    "BindingTableIndex", // 27
    "FuncControl", // 28
    "MessageTypeDP_DC1", // 29
    "MessageTypeDP0Category", // 30
    "MessageTypeDP_DC0Legacy", // 31
    "MessageTypeDP_DC0ScratchBlock", // 32
    "MessageTypeDP_DC2", // 33
    "MessageTypeDP_DCRO", // 34
    "ExMessageLength", // 35
    "DPOpcode", // 36
    "DPAddrSurfaceType", // 37
    "DPVectSize", // 38
    "DPFlushType", // 39
    "DPTranspose", // 40
    "DPFlushRange", // 41
    "DPDataSize", // 42
    "DPFenceScope", // 43
    "DPAddrSize" // 44
}; // fieldNameByPseudoField[]
#endif // GED_VALIDATION_API
