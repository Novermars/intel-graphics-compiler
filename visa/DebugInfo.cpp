/*===================== begin_copyright_notice ==================================

Copyright (c) 2017 Intel Corporation

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to
permit persons to whom the Software is furnished to do so, subject to
the following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.


======================= end_copyright_notice ==================================*/

#include "DebugInfo.h"
#include "Common_ISA.h"
#include "Gen4_IR.hpp"
#include "FlowGraph.h"
#include "BuildIR.h"
#include <map>
#include "Common_ISA_framework.h"
#include "VISAKernel.h"
#include "BitSet.h"
using namespace vISA;

using std::fclose;
using std::fopen;
using std::fwrite;
using std::FILE;

uint32_t getBinInstSize(G4_INST* inst);

int32_t get32BitSignedIntFrom31BitSignedInt(uint32_t data)
{
    // MSB of 32-bit input is discarded
    int32_t signedMemOffset = (int32_t)data;
    bool isNeg = ((signedMemOffset << 1) < 0);
    // Right shift on signed int is implementation defined
    // so on some compilers it could do bitshift and
    // on some arithmetic shift. Bitwise OR'ing later
    // ensures that sign bit is setup correctly.
    signedMemOffset = (signedMemOffset << 1) >> 1;
    signedMemOffset |= (isNeg ? (1 << (sizeof(int32_t)-1)) : 0);

    return signedMemOffset;
}

void DbgDecoder::ddName()
{
    uint8_t nameLen;
    auto retval = fread(&nameLen, sizeof(uint8_t), 1, dbgFile);
    if (!retval)
        return;

    char name[COMMON_ISA_MAX_KERNEL_NAME_LEN + 1];
    retval = fread(name, sizeof(uint8_t), nameLen, dbgFile);
    if (!retval)
        return;

    name[nameLen] = 0;

    std::cout << name;
}

template<class T>
void DbgDecoder::ddLiveInterval()
{
    // Dump live-interval info
    uint16_t numLiveIntervals;
    T start, end;
    auto retval = fread(&numLiveIntervals, sizeof(uint16_t), 1, dbgFile);
    if (!retval)
        return;

    std::cout << "\tLive intervals: \n";
    for (uint16_t i = 0; i < numLiveIntervals; i++)
    {
        retval = fread(&start, sizeof(T), 1, dbgFile);
        if (!retval)
            return;

        retval = fread(&end, sizeof(T), 1, dbgFile);
        if (!retval)
            return;

        std::cout << "(" << start << ", " << end << ") @ ";

        uint8_t virtualType;
        retval = fread(&virtualType, sizeof(uint8_t), 1, dbgFile);
        if (!retval)
            return;

        if (virtualType == VARMAP_VREG_FILE_ADDRESS)
        {
            std::cout << "\t";
        }
        else if (virtualType == VARMAP_VREG_FILE_FLAG)
        {
            std::cout << "\t";
        }
        else if (virtualType == VARMAP_VREG_FILE_GRF)
        {
            std::cout << "\t";
        }
        else
        {
            MUST_BE_TRUE(false, "Unknown virtual type found");
        }

        uint8_t physicalType;
        retval = fread(&physicalType, sizeof(uint8_t), 1, dbgFile);
        if (!retval)
            return;

        if (physicalType == VARMAP_PREG_FILE_ADDRESS)
        {
            std::cout << "a";
        }
        else if (physicalType == VARMAP_PREG_FILE_FLAG)
        {
            std::cout << "f";
        }
        else if (physicalType == VARMAP_PREG_FILE_GRF)
        {
            std::cout << "r";
        }
        else if (physicalType == VARMAP_PREG_FILE_MEMORY)
        {
            std::cout << "Spilled";
        }
        else
        {
            MUST_BE_TRUE(false, "Unknown physical type found");
        }

        if (physicalType == VARMAP_PREG_FILE_MEMORY)
        {
            uint32_t memoryOffset;
            bool isAbsoluteOffset = false;
            retval = fread(&memoryOffset, sizeof(uint32_t), 1, dbgFile);
            if (!retval)
                return;

            if (memoryOffset & 0x80000000)
            {
                isAbsoluteOffset = true;
            }

            std::cout << " (offset = " << get32BitSignedIntFrom31BitSignedInt(memoryOffset) << " bytes)" <<
                (isAbsoluteOffset ? " (absolute offset)" : " (off be_fp)") <<
                "\n";
        }
        else
        {
            uint16_t regNum, subRegNum;
            retval = fread(&regNum, sizeof(uint16_t), 1, dbgFile);
            if (!retval)
                return;

            retval = fread(&subRegNum, sizeof(uint16_t), 1, dbgFile);
            if (!retval)
                return;

            std::cout << regNum << "." << subRegNum;

            if (physicalType == VARMAP_PREG_FILE_GRF)
            {
                std::cout << ":ub";
            }

            std::cout << "\n";

        }
    }
    std::cout << "\n";
}

void DbgDecoder::ddCalleeCallerSave(uint32_t relocOffset)
{
    uint16_t num;

    if (feof(dbgFile))
    {
        return;
    }

    auto retval = fread(&num, sizeof(uint16_t), 1, dbgFile);
    if (!retval)
        return;

    for (uint32_t i = 0; i < num; i++)
    {
        uint32_t genOffset;
        retval = fread(&genOffset, sizeof(uint32_t), 1, dbgFile);
        if (!retval)
            return;

        std::cout << "Gen ISA offset: " << genOffset << "\n";

        uint16_t numElems;
        retval = fread(&numElems, sizeof(uint16_t), 1, dbgFile);
        if (!retval)
            return;

        for (uint32_t j = 0; j < numElems; j++)
        {
            uint16_t srcReg, numBytes;
            retval = fread(&srcReg, sizeof(uint16_t), 1, dbgFile);
            if (!retval)
                return;

            retval = fread(&numBytes, sizeof(uint16_t), 1, dbgFile);
            if (!retval)
                return;

            uint8_t subReg = srcReg%numEltPerGRF(Type_UB);
            MUST_BE_TRUE(subReg == 0, "Not expecting non-zero sub-reg in callee/caller save");
            std::cout << "\tr" << (srcReg) / numEltPerGRF(Type_UB) << "." <<
                (uint32_t)subReg << ":ub (" << numBytes << " bytes) -> ";

            uint8_t dstInReg;
            retval = fread(&dstInReg, sizeof(uint8_t), 1, dbgFile);
            if (!retval)
                return;

            if (dstInReg)
            {
                uint16_t reg, subreg;
                retval = fread(&reg, sizeof(uint16_t), 1, dbgFile);
                if (!retval)
                    return;

                retval = fread(&subreg, sizeof(uint16_t), 1, dbgFile);
                if (!retval)
                    return;

                std::cout << "r" << reg << "." << subreg << ":ub" << "\n";
            }
            else
            {
                uint32_t memOffset;
                retval = fread(&memOffset, sizeof(uint32_t), 1, dbgFile);
                if (!retval)
                    return;

                if (memOffset & 0x80000000)
                {
                    std::cout << get32BitSignedIntFrom31BitSignedInt(memOffset);
                }
                else
                {
                    std::cout << "BE_FP + " << memOffset;
                }

                std::cout << " bytes" << "\n";
            }
        }
    }
}

int DbgDecoder::ddDbg()
{
    dbgFile = fopen(filename, "rb");

    if (!dbgFile)
    {
        std::cerr << "Error opening and creating debug file: " << filename << "\n";
        ASSERT_USER(false, "Unable to wrie debug file to disk.");
        return -1;
    }

    uint32_t magic;
    auto retval = fread(&magic, sizeof(uint32_t), 1, dbgFile);
    if (!retval)
        return -1;

    std::cout << "=== Start of Debug Dump ===" << "\n";
    std::cout << "Magic: " << "0x" << std::hex << magic << std::dec << "\n";
    if (magic != DEBUG_MAGIC_NUMBER)
    {
        std::cout << "************ Magic expected = " << "0x" << std::hex << DEBUG_MAGIC_NUMBER << std::dec << " *************" << "\n";

        fclose(dbgFile);

        return -1;
    }

    uint16_t numCompiledObjects;
    retval = fread(&numCompiledObjects, sizeof(uint16_t), 1, dbgFile);
    if (!retval)
        return -1;

    std::cout << "Number of compiled objects: " << numCompiledObjects << "\n\n";

    for (unsigned int i = 0; i < numCompiledObjects; i++)
    {
        std::cout << "Current compiled object index: " << i << "\n";

        std::cout << "Kernel name: ";
        ddName();
        std::cout << "\n";

        uint32_t reloc_offset;
        retval = fread(&reloc_offset, sizeof(uint32_t), 1, dbgFile);
        if (!retval)
            return -1;

        if (reloc_offset == 0)
        {
            std::cout << "(kernel)\n";
        }
        else
        {
            std::cout << "(function binary @ gen offset " << reloc_offset << " bytes)" << "\n";
        }

        uint32_t numElementsCISAOffsetMap;
        retval = fread(&numElementsCISAOffsetMap, sizeof(uint32_t), 1, dbgFile);
        if (!retval)
            return -1;

        std::cout << "CISA byte offset -> Gen byte offset mapping\n";

        for (unsigned int j = 0; j < numElementsCISAOffsetMap; j++)
        {
            uint32_t cisaOffset, genOffset;
            retval = fread(&cisaOffset, sizeof(uint32_t), 1, dbgFile);
            if (!retval)
                return -1;

            retval = fread(&genOffset, sizeof(uint32_t), 1, dbgFile);
            if (!retval)
                return -1;

            std::cout << cisaOffset << "\t" << genOffset << "\n";
        }

        std::cout << "\n";

        uint32_t numElementsCISAIndexMap;
        retval = fread(&numElementsCISAIndexMap, sizeof(uint32_t), 1, dbgFile);
        if (!retval)
            return -1;

        std::cout << "CISA index -> Gen byte offset mapping\n";

        for (unsigned int j = 0; j < numElementsCISAIndexMap; j++)
        {
            uint32_t cisaIndex, genOffset;
            retval = fread(&cisaIndex, sizeof(uint32_t), 1, dbgFile);
            if (!retval)
                return -1;

            retval = fread(&genOffset, sizeof(uint32_t), 1, dbgFile);
            if (!retval)
                return -1;

            std::cout << cisaIndex << "\t" << genOffset << "\n";
        }

        std::cout << "\n";

        uint32_t numElementsVarMap;
        retval = fread(&numElementsVarMap, sizeof(uint32_t), 1, dbgFile);
        if (!retval)
            return -1;

        std::cout << "Virtual Register -> Physical Register mapping\n";

        for (unsigned int j = 0; j < numElementsVarMap; j++)
        {
            ddName();

            ddLiveInterval<uint16_t>();
        }
        std::cout << "\n\n";

        // Read sub-info
        uint16_t numSubs;
        retval = fread(&numSubs, sizeof(uint16_t), 1, dbgFile);
        if (!retval)
            return -1;

        std::cout << "Number of subroutines: " << numSubs << "\n";

        for (unsigned int j = 0; j < numSubs; j++)
        {
            std::cout << "Subroutine name: ";
            ddName();
            std::cout << "\n";
            uint32_t startoffset = 0, endOffset = 0;
            retval = fread(&startoffset, sizeof(uint32_t), 1, dbgFile);
            if (!retval)
                return -1;

            retval = fread(&endOffset, sizeof(uint32_t), 1, dbgFile);
            if (!retval)
                return -1;

            std::cout << "Start VISA: " << startoffset << ", end VISA: " << endOffset << "\n";
            std::cout << "Retval: \n";
            ddLiveInterval<uint16_t>();
        }

        std::cout << "\n";
        uint16_t frameSize;
        retval = fread(&frameSize, sizeof(uint16_t), 1, dbgFile);
        if (!retval)
            return -1;

        std::cout << "Frame size: " << frameSize << " bytes\n";

        uint8_t scratch;
        retval = fread(&scratch, sizeof(uint8_t), 1, dbgFile);
        if (!retval)
            return -1;

        if (scratch)
        {
            std::cout << "BE_FP: \n";
            ddLiveInterval<uint32_t>();
        }
        else
        {
            std::cout << "BE_FP not found";
        }

        std::cout << "\n";

        retval = fread(&scratch, sizeof(uint8_t), 1, dbgFile);
        if (!retval)
            return -1;

        if (scratch)
        {
            std::cout << "Caller BE_FP saved at:\n";
            ddLiveInterval<uint32_t>();
        }
        else
        {
            std::cout << "Caller BE_FP not saved";
        }

        std::cout << "\n";

        retval = fread(&scratch, sizeof(uint8_t), 1, dbgFile);
        if (!retval)
            return -1;

        if (scratch)
        {
            std::cout << "Return addr saved at:\n";
            ddLiveInterval<uint32_t>();
        }
        else
        {
            std::cout << "Return addr not stored";
        }

        std::cout << "\n";

        std::cout << "Callee save:\n";
        ddCalleeCallerSave(reloc_offset);
        std::cout << "\n";

        std::cout << "Caller save:\n";
        ddCalleeCallerSave(reloc_offset);
        std::cout << "\n";
    }

    std::cout << "=== End of Debug Dump ===\n";

    fclose(dbgFile);

    return 0;
}

DEBUG_RELEASE_INTERNAL_DLL_EXPORT_ONLY int decodeAndDumpDebugInfo(char* filename)
{
    DbgDecoder dd(filename);
    return dd.ddDbg();
}

void getGRF(G4_Declare* dcl, unsigned int& regNum, unsigned int& subRegNumInBytes)
{
    if (dcl->getRegVar()->getPhyReg() != NULL)
    {
        regNum = dcl->getRegVar()->getPhyReg()->asGreg()->getRegNum();
        subRegNumInBytes = dcl->getRegVar()->getPhyRegOff() * G4_Type_Table[dcl->getElemType()].byteSize;
    }
    else
    {
        regNum = 65535;
        subRegNumInBytes = 65535;
    }
}

bool KernelDebugInfo::isMissingVISAId(unsigned int id)
{
    if (!missingVISAIdsComputed)
    {
        computeMissingVISAIds();
    }

    return (missingVISAIds.find(id) != missingVISAIds.end());
}

void KernelDebugInfo::computeMissingVISAIds()
{
    unsigned int maxCISAId = 0;

    for (auto bb : getKernel().fg)
    {
        for (auto inst : *bb)
        {
            if (inst->getCISAOff() != UNMAPPABLE_VISA_INDEX &&
                (unsigned int)inst->getCISAOff() > maxCISAId)
            {
                maxCISAId = inst->getCISAOff();
            }
        }
    }

    std::vector<bool> seenVISAIds(maxCISAId+1, false);

    for (auto bb : getKernel().fg)
    {
        for (auto inst : *bb)
        {
            if (inst->getCISAOff() != UNMAPPABLE_VISA_INDEX)
            {
                seenVISAIds[inst->getCISAOff()] = true;
            }
        }
    }

    for (unsigned int i = 0, size = seenVISAIds.size(); i < size; i++)
    {
        if (!seenVISAIds[i])
        {
            missingVISAIds.insert(i);
        }
    }

    missingVISAIdsComputed = true;
}

void KernelDebugInfo::updateMapping(std::list<G4_BB*>& stackCallEntryBBs)
{
    reset();

    generateByteOffsetMapping(stackCallEntryBBs);
    emitRegisterMapping();
    generateCISAByteOffsetFromOffset();
    generateGenISAToVISAIndex();
}

void KernelDebugInfo::generateGenISAToVISAIndex()
{
    // Generate list of Gen ISA offset -> VISA index
    // This is used to emit debug_ranges section in IGC.
    // Inserting entries per Gen ISA offset guarantees
    // all instructions will be present in the vector.
    for (auto bb : kernel->fg)
    {
        for (auto inst : *bb)
        {
            if (inst->getGenOffset() == -1)
                continue;
            genISAOffsetToVISAIndex.push_back(std::make_pair((unsigned int)inst->getGenOffset(), (unsigned int)inst->getCISAOff()));
        }
    }
}

void KernelDebugInfo::setVISAKernel(VISAKernelImpl* k)
{
    visaKernel = k;
    kernel = k->getKernel();
}

void KernelDebugInfo::generateCISAByteOffsetFromOffset()
{
    // Using map1 and map2, generate map3
    for (std::vector<std::pair<unsigned int, unsigned int>>::iterator it = mapCISAIndexGenOffset.begin();
        it != mapCISAIndexGenOffset.end();
        it++)
    {
        // Read each entry in CISA Index->Gen Offset then map CISA Index to CISA Offset.
        // Push back results.
        unsigned int cisaIndex = (*it).first;
        unsigned int genOffset = (*it).second;

        std::map<unsigned int, unsigned int>::iterator map_it = mapCISAOffset.find(cisaIndex);

        if (map_it != mapCISAOffset.end())
        {
            unsigned int cisaOffset = mapCISAOffset.find(cisaIndex)->second;
            mapCISAOffsetGenOffset.push_back(std::make_pair(cisaOffset, genOffset));
        }
    }
}

void KernelDebugInfo::generateByteOffsetMapping(std::list<G4_BB*>& stackCallEntryBBs)
{
    // When compiling stack call functions, all stack call functions
    // invoked are stitched to kernel being compiled. So G4_BBs of
    // all stack call functions are appended to G4_Kernel's BB list.
    // We need a way to differentiate between BBs of kernel and those
    // of functions to emit out correct debug info. So a list is
    // passed - stackCallEntryBBs that holds entryBBs of all stack
    // call functions part of this compilation unit.

    bool done = false;
    unsigned int maxVISAIndex = 0;
    uint64_t maxGenIsaOffset = 0;
    // Now traverse CFG, create pair of CISA byte offset, gen binary offset and push to vector
    for (BB_LIST_ITER bb_it = kernel->fg.begin(), bbEnd = kernel->fg.end(); bb_it != bbEnd; bb_it++)
    {
        G4_BB* bb = (*bb_it);

        int isaPrevByteOffset = -1;

        if (kernel->fg.builder->getIsKernel())
        {
            auto entryBBend = stackCallEntryBBs.end();
            for (auto entryBBIt = stackCallEntryBBs.begin();
                entryBBIt != entryBBend;
                entryBBIt++)
            {
                if (bb == (*entryBBIt))
                {
                    // Since we are traversing BBs in layout
                    // order, we will parse all kernel BBs
                    // first and as soon as we reach entryBB
                    // of first stack call function, we stop
                    // processing.
                    done = true;
                    break;
                }
            }
        }

        if (done == true)
        {
            break;
        }

        for (INST_LIST_ITER inst_it = bb->begin(), bbEnd = bb->end();
            inst_it != bbEnd;
            inst_it++)
        {
            G4_INST* inst = (*inst_it);

            if (inst->getGenOffset() != UNDEFINED_GEN_OFFSET)
            {
                int cisaByteIndex = inst->getCISAOff();
                maxGenIsaOffset = (uint64_t)inst->getGenOffset() +
                                    (inst->isCompactedInst() ? 8 : 16);
                if (cisaByteIndex == -1)
                {
                    continue;
                }

                maxVISAIndex = std::max(maxVISAIndex, (unsigned int)cisaByteIndex);

                if (isaPrevByteOffset != cisaByteIndex)
                {
                    isaPrevByteOffset = cisaByteIndex;

                    // mapping holds pair of CISA bytecode index and gen Offset
                    // Use VISAKernelImpl's member mapCISAOffset to convert
                    // CISA bytecode index to CISA bytecode byte offset
                    mapCISAIndexGenOffset.push_back(std::make_pair(cisaByteIndex, (int)inst->getGenOffset()));
                }
            }
        }
    }

    // Insert out-of-sequence entry in to VISA index->Gen offset map
    mapCISAIndexGenOffset.push_back(std::make_pair(++maxVISAIndex, (unsigned int)maxGenIsaOffset));
}

void KernelDebugInfo::emitRegisterMapping()
{
    // Emit out mapping between
    // virtual variables -> physical registers
    // In case a variable has been spilled to memory,
    // emit out memory offset.
    // For address/flag registers, spill location is
    // GRF registers. Only general variables, ie GRF
    // candidates can be spilled to memory.

    for (DECLARE_LIST_ITER dcl_it = getKernel().Declares.begin();
        dcl_it != getKernel().Declares.end();
        dcl_it++)
    {
        G4_Declare* dcl = (*dcl_it);
        if (getKernel().fg.isPseudoDcl(dcl) ||
            (dcl->getRegVar()->getPhyReg() &&
            dcl->getRegVar()->getPhyReg()->isAreg()))
        {
            // These pseudo nodes may or may not get
            // an allocation depending on register
            // pressure across fcall. There is no
            // need to look at allocation results
            // for these as far as debug info goes.
            continue;
        }

        VarnameMap* varMap = (VarnameMap*)getKernel().fg.builder->mem.alloc(sizeof(struct VarnameMap));
        varMap->dcl = dcl;

        if ((dcl->getRegFile() == G4_GRF || dcl->getRegFile() == G4_INPUT) &&
            dcl->getRegVar()->isNullReg() == false)
        {
            // GRF candidate can be either in GRF or
            // spilled to memory
            bool isSpilled = dcl->isSpilled() && (dcl->getRegVar()->getPhyReg() == NULL);
            varMap->virtualType = VARMAP_VREG_FILE_GRF;

            if (!isSpilled)
            {
                unsigned int regNum, subRegNumInBytes;
                getGRF(dcl, regNum, subRegNumInBytes);
                varMap->physicalType = VARMAP_PREG_FILE_GRF;
                varMap->Mapping.Register.regNum = static_cast<uint16_t>(regNum);
                varMap->Mapping.Register.subRegNum = static_cast<uint16_t>(subRegNumInBytes);
                varsMap.push_back(varMap);
            }
            else
            {
                unsigned int spillOffset = 0;
                while (dcl->getAliasDeclare() != NULL)
                {
                    spillOffset += dcl->getAliasOffset();
                    dcl = dcl->getAliasDeclare();
                }
                spillOffset += dcl->getRegVar()->getDisp();
                varMap->physicalType = VARMAP_PREG_FILE_MEMORY;
                if (getKernel().fg.getHasStackCalls() == false)
                {
                    varMap->Mapping.Memory.isAbs = 1;
                }
                else
                {
                    varMap->Mapping.Memory.isAbs = 0;
                }
                varMap->Mapping.Memory.memoryOffset = (int32_t)spillOffset;
                varsMap.push_back(varMap);
            }
        }
        else if (dcl->getRegFile() == G4_ADDRESS)
        {
            bool isSpilled = dcl->isSpilled() && (dcl->getRegVar()->getPhyReg() == NULL);
            varMap->virtualType = VARMAP_VREG_FILE_ADDRESS;

            if (!isSpilled)
            {
                unsigned int subRegNum;
                subRegNum = dcl->getRegVar()->getPhyRegOff();
                varMap->physicalType = VARMAP_PREG_FILE_ADDRESS;
                varMap->Mapping.Register.regNum = 0;
                varMap->Mapping.Register.subRegNum = static_cast<uint16_t>(subRegNum);
                varsMap.push_back(varMap);
            }
            else
            {
                // Spilled to GRF
                if (!dcl->getSpilledDeclare()->isSpilled())
                {
                    unsigned int regNum, subRegNumInBytes;
                    getGRF(dcl->getSpilledDeclare(), regNum, subRegNumInBytes);
                    varMap->physicalType = VARMAP_PREG_FILE_GRF;
                    varMap->Mapping.Register.regNum = static_cast<uint16_t>(regNum);
                    varMap->Mapping.Register.subRegNum = static_cast<uint16_t>(subRegNumInBytes);
                    varsMap.push_back(varMap);
                }
                else
                {
                    unsigned int spillOffset = 0;
                    //G4_Declare* origDcl = dcl;
                    while (dcl->getAliasDeclare() != NULL)
                    {
                        spillOffset += dcl->getAliasOffset();
                        dcl = dcl->getAliasDeclare();
                    }
                    spillOffset += dcl->getSpilledDeclare()->getRegVar()->getDisp();
                    varMap->physicalType = VARMAP_PREG_FILE_MEMORY;
                    if (getKernel().fg.getHasStackCalls() == false)
                    {
                        varMap->Mapping.Memory.isAbs = 1;
                    }
                    else
                    {
                        varMap->Mapping.Memory.isAbs = 0;
                    }
                    varMap->Mapping.Memory.memoryOffset = (int32_t)spillOffset;
                    varsMap.push_back(varMap);
                }
            }
        }
        else if (dcl->getRegFile() == G4_FLAG)
        {
            bool isSpilled = dcl->isSpilled() && (dcl->getRegVar()->getPhyReg() == NULL);
            varMap->virtualType = VARMAP_VREG_FILE_FLAG;

            if (!isSpilled)
            {
                unsigned int regNum, subRegNum;
                regNum = dcl->getRegVar()->getPhyReg()->asAreg()->getFlagNum();
                subRegNum = dcl->getRegVar()->getPhyRegOff();
                varMap->physicalType = VARMAP_PREG_FILE_FLAG;
                varMap->Mapping.Register.regNum = static_cast<uint16_t>(regNum);
                varMap->Mapping.Register.subRegNum = static_cast<uint16_t>(subRegNum);
                varsMap.push_back(varMap);
            }
            else
            {
                // Spilled to GRF
                if (!dcl->getSpilledDeclare()->isSpilled())
                {
                    unsigned int regNum, subRegNumInBytes;
                    getGRF(dcl->getSpilledDeclare(), regNum, subRegNumInBytes);
                    varMap->physicalType = VARMAP_PREG_FILE_GRF;
                    varMap->Mapping.Register.regNum = static_cast<uint16_t>(regNum);
                    varMap->Mapping.Register.subRegNum = static_cast<uint16_t>(subRegNumInBytes);
                    varsMap.push_back(varMap);
                }
                else
                {
                    unsigned int spillOffset = 0;
                    //G4_Declare* origDcl = dcl;
                    while (dcl->getAliasDeclare() != NULL)
                    {
                        spillOffset += dcl->getAliasOffset();
                        dcl = dcl->getAliasDeclare();
                    }
                    spillOffset += dcl->getSpilledDeclare()->getRegVar()->getDisp();
                    varMap->physicalType = VARMAP_PREG_FILE_MEMORY;
                    if (getKernel().fg.getHasStackCalls() == false)
                    {
                        varMap->Mapping.Memory.isAbs = 1;
                    }
                    else
                    {
                        varMap->Mapping.Memory.isAbs = 0;
                    }
                    varMap->Mapping.Memory.memoryOffset = (int32_t)spillOffset;
                    varsMap.push_back(varMap);
                }
            }
        }
    }
}

void insertData(const void* ptr, unsigned size, FILE* f)
{
    fwrite(ptr, size, 1, f);
}

void insertData(const void* ptr, unsigned size, std::vector<unsigned char>& vec)
{
    for (unsigned i = 0; i < size; ++i)
    {
        vec.push_back(*(((const unsigned char*)ptr) + i));
    }
}

unsigned int populateMapDclName(VISAKernelImpl* kernel, std::map<G4_Declare*, std::pair<const char*, unsigned int>>& mapDclName)
{
    std::list<CISA_GEN_VAR*> dclList;
    for (uint32_t ctr = 0; ctr < kernel->getGenVarCount(); ctr++)
    {
        // Pre-defined gen vars are included in this list,
        // but we dont want to emit them to debug info.
        if (kernel->getGenVar((unsigned int)ctr)->index >= kernel->getNumPredVars())
        {
            dclList.push_back(kernel->getGenVar((unsigned int)ctr));
        }
    }

    for (uint32_t ctr = 0; ctr < kernel->getAddrVarCount(); ctr++)
    {
        dclList.push_back(kernel->getAddrVar((unsigned int)ctr));
    }

    for (uint32_t ctr = 0; ctr < kernel->getPredVarCount(); ctr++)
    {
        dclList.push_back(kernel->getPredVar((unsigned int)ctr));
    }

    for (uint32_t ctr = 0; ctr < kernel->getSurfaceVarCount(); ctr++)
    {
        dclList.push_back(kernel->getSurfaceVar((unsigned int)ctr));
    }

    for (uint32_t ctr = 0; ctr < kernel->getSamplerVarCount(); ctr++)
    {
        dclList.push_back(kernel->getSamplerVar((unsigned int)ctr));
    }

    auto start = dclList.begin();
    auto end = dclList.end();

    for (auto it = start;
        it != end;
        it++)
    {
        CISA_GEN_VAR* var = (*it);

        if (var->type == GENERAL_VAR)
        {
            mapDclName.insert(std::make_pair(var->genVar.dcl, std::make_pair("V", var->index)));
        }
        else if (var->type == ADDRESS_VAR)
        {
            mapDclName.insert(std::make_pair(var->addrVar.dcl, std::make_pair("A", var->index)));
        }
        else if (var->type == PREDICATE_VAR)
        {
            mapDclName.insert(std::make_pair(var->predVar.dcl, std::make_pair("P", var->index)));
        }
        else if (var->type == SURFACE_VAR)
        {
            mapDclName.insert(std::make_pair(var->stateVar.dcl, std::make_pair("T", var->index)));
        }
        else if (var->type == SAMPLER_VAR)
        {
            mapDclName.insert(std::make_pair(var->stateVar.dcl, std::make_pair("S", var->index)));
        }
    }

    return (uint32_t) dclList.size();
}

uint32_t KernelDebugInfo::getVarIndex(G4_Declare* dcl)
{
    uint32_t retval = 0xffffffff;
    for (uint32_t i = 0, size = varsMap.size(); i < size; i++)
    {
        if (dcl == varsMap[i]->dcl)
        {
            retval = i;
            break;
        }
    }
    return retval;
}

template<class T>
void emitDataName(const char* name, T& t)
{
    auto length = (uint8_t)strlen(name);
    // Length
    insertData(&length, sizeof(uint8_t), t);
    // Actual name
    insertData(name, (uint32_t) (sizeof(uint8_t) * length), t);
}

template<class T>
void emitDataUInt32(uint32_t data, T& t)
{
    insertData(&data, sizeof(uint32_t), t);
}

template<class T>
void emitDataUInt16(uint16_t data, T& t)
{
    insertData(&data, sizeof(uint16_t), t);
}

template<class T>
void emitDataUInt8(uint8_t data, T& t)
{
    insertData(&data, sizeof(uint8_t), t);
}

template<class T>
void emitDataVarLiveInterval(VISAKernelImpl* visaKernel, LiveIntervalInfo* lrInfo, uint32_t i, uint16_t size, T& t)
{
    // given lrs and saverestore, prepare assembled list of ranges to write out
    KernelDebugInfo* dbgInfo = visaKernel->getKernel()->getKernelDebugInfo();

    // start cisa index, end cisa index
    std::vector<std::pair<uint32_t, uint32_t>> lrs;
    if (lrInfo)
    {
        lrInfo->getLiveIntervals(lrs);
    }
    uint16_t numLRs = (uint16_t)lrs.size();
    std::sort(lrs.begin(), lrs.end(), [](std::pair<uint32_t, uint32_t>& a, std::pair<uint32_t, uint32_t>& b) { return a.first < b.first; });
    emitDataUInt16(numLRs, t);
    for (auto& it : lrs)
    {
        const uint32_t start = (uint32_t)it.first;
        const uint32_t end = (uint32_t)it.second;

        if (size == 2)
        {
            emitDataUInt16((uint16_t)start, t);
            emitDataUInt16((uint16_t)end, t);
        }
        else
        {
            emitDataUInt32(start, t);
            emitDataUInt32(end, t);
        }

        auto& varsMap = dbgInfo->getVarsMap();
        const unsigned char virtualType = varsMap[i]->virtualType;
        // Write virtual register type
        emitDataUInt8((uint8_t)virtualType, t);

        const unsigned char physicalType = varsMap[i]->physicalType;
        // Write physical register type
        emitDataUInt8((uint8_t)physicalType, t);

        // If physical register assigned then write register number and
        // sub-register number. Else write memory spill offset.
        if (physicalType == VARMAP_PREG_FILE_MEMORY)
        {
            unsigned int memOffset = (unsigned int)varsMap[i]->Mapping.Memory.memoryOffset;
            if (visaKernel->getKernel()->fg.getHasStackCalls() == false)
            {
                memOffset |= 0x80000000;
            }
            // Emit memory offset
            emitDataUInt32((uint32_t)memOffset, t);
        }
        else
        {
            const unsigned int regNum = varsMap[i]->Mapping.Register.regNum;
            const unsigned int subRegNum = varsMap[i]->Mapping.Register.subRegNum;

            // Emit register number
            emitDataUInt16((uint16_t)regNum, t);

            // Emit sub-register number
            emitDataUInt16((uint16_t)subRegNum, t);
        }
    }
}

template<class T>
void emitFrameDescriptorOffsetLiveInterval(LiveIntervalInfo* lrInfo, StackCall::FrameDescriptorOfsets memOffset, T& t)
{
    // Used to emit fields of Frame Descriptor
    // location = [start, end) @ BE_FP+offset
    std::vector<std::pair<uint32_t, uint32_t>> lrs;
    if (lrInfo)
        lrInfo->getLiveIntervals(lrs);
    else
        return;

    uint32_t start = 0, end = 0;
    if (lrs.size() > 0)
    {
        start = lrs.front().first;
        end = lrs.back().second;
    }

    std::sort(lrs.begin(), lrs.end(), [](std::pair<uint32_t, uint32_t>& a, std::pair<uint32_t, uint32_t>& b) { return a.first < b.first; });

    emitDataUInt16(1, t);

    emitDataUInt32(start, t);
    emitDataUInt32(end, t);

    emitDataUInt8((uint8_t)VARMAP_PREG_FILE_GRF, t);

    emitDataUInt8((uint8_t)VARMAP_PREG_FILE_MEMORY, t);

    emitDataUInt32((uint32_t)memOffset, t);
}

void populateUniqueSubs(G4_Kernel* kernel, std::unordered_map<G4_BB*, bool>& uniqueSubs)
{
    // Traverse kernel and populate all unique subs.
    // Iterating over all BBs of kernel visits all
    // subroutine call sites.
    auto isStackObj = kernel->fg.getHasStackCalls() || kernel->fg.getIsStackCallFunc();
    for (auto bb : kernel->fg)
    {
        if (&bb->getParent() != &kernel->fg)
            continue;

        if (bb->isEndWithCall())
        {
            if (!isStackObj || // definitely a subroutine since kernel has no stack calls
                (isStackObj && // a subroutine iff call dst != pre-defined reg as per ABI
                    bb->back()->getDst()->getTopDcl()->getRegVar()->getPhyReg()->asGreg()->getRegNum() != kernel->getFPSPGRF()))
            {
                // This is a subroutine call
                uniqueSubs[bb->Succs.front()] = false;
            }
        }
    }
}

template<class T>
void emitDataSubroutines(VISAKernelImpl* visaKernel, T& t)
{
    auto kernel = visaKernel->getKernel();
    // map<Label, Written to t>
    std::unordered_map<G4_BB*, bool> uniqueSubs;

    populateUniqueSubs(kernel, uniqueSubs);

    emitDataUInt16((uint16_t) uniqueSubs.size(), t);

    for (auto bb : kernel->fg)
    {
        G4_INST* firstInst = nullptr;
        G4_INST* lastInst = nullptr;
        unsigned int start = 0, end = 0;
        G4_Declare* retval = nullptr;
        G4_Label* subLabel = nullptr;

        if (bb->isEndWithCall())
        {
            auto subInfo = uniqueSubs.find(bb->Succs.front());
            if (subInfo != uniqueSubs.end() &&
                subInfo->second == false)
            {
                subInfo->second = true;
                G4_BB* calleeBB = bb->Succs.front();
                while (firstInst == NULL && calleeBB != NULL)
                {
                    if (calleeBB->size() > 0)
                    {
                        firstInst = calleeBB->front();
                        start = firstInst->getCISAOff();
                        subLabel = firstInst->getSrc(0)->asLabel();
                    }
                }

                calleeBB = bb->BBAfterCall()->Preds.front();
                while (lastInst == NULL && calleeBB != NULL)
                {
                    if (calleeBB->size() > 0)
                    {
                        lastInst = calleeBB->back();
                        end = lastInst->getCISAOff();
                        MUST_BE_TRUE(lastInst->isReturn(), "Expecting to see G4_return as last inst in sub-routine");
                        retval = lastInst->getSrc(0)->asSrcRegRegion()->getBase()->asRegVar()->getDeclare()->getRootDeclare();
                    }

                    calleeBB = calleeBB->Preds.front();
                }
                emitDataName(subLabel->getLabel(), t);
                emitDataUInt32(start, t);
                emitDataUInt32(end, t);

                if (kernel->getKernelDebugInfo()->getLiveIntervalInfo(retval, false) != NULL)
                {
                    auto lv = kernel->getKernelDebugInfo()->getLiveIntervalInfo(retval, false);
                    uint32_t idx = kernel->getKernelDebugInfo()->getVarIndex(retval);
                    emitDataVarLiveInterval(visaKernel, lv, idx, sizeof(uint16_t), t);
                }
            }
        }
    }
}

template<class T>
void emitDataPhyRegSaveInfoPerIP(VISAKernelImpl* visaKernel, SaveRestoreManager& mgr, T& t)
{
    auto& srInfo = mgr.getSRInfo();
    auto relocOffset = visaKernel->getKernel()->getKernelDebugInfo()->getRelocOffset();

    for (auto sr : srInfo)
    {
        if (sr.getInst()->getGenOffset() == UNDEFINED_GEN_OFFSET)
        {
            continue;
        }

        emitDataUInt32((uint32_t)sr.getInst()->getGenOffset() +
            getBinInstSize(sr.getInst()) - relocOffset, t);
        emitDataUInt16((uint16_t) sr.saveRestoreMap.size(), t);
        for (auto mapIt : sr.saveRestoreMap)
        {
            emitDataUInt16((uint16_t)mapIt.first * numEltPerGRF(Type_UB), t);
            emitDataUInt16((uint16_t)numEltPerGRF(Type_UB), t);

            if (mapIt.second.first == SaveRestoreInfo::RegOrMem::Reg)
            {
                emitDataUInt8((uint8_t)1, t);
                emitDataUInt16((uint16_t)mapIt.second.second.regNum, t);
                emitDataUInt16((uint16_t)0, t);
            }
            else if (mapIt.second.first == SaveRestoreInfo::RegOrMem::MemOffBEFP)
            {
                SaveRestoreInfo::RegMap tmp;
                emitDataUInt8((uint8_t)0, t);
                tmp = mapIt.second.second;
                tmp.isAbs = 0;
                uint32_t data = mapIt.second.second.memOff;
                emitDataUInt32(data, t);
            }
            else if (mapIt.second.first == SaveRestoreInfo::RegOrMem::MemAbs)
            {
                SaveRestoreInfo::RegMap tmp;
                emitDataUInt8((uint8_t)0, t);
                tmp = mapIt.second.second;
                tmp.isAbs = 1;
                uint32_t data = mapIt.second.second.memOff;
                emitDataUInt32(data, t);
            }
        }
    }
}

void SaveRestoreManager::sieveInstructions(CallerOrCallee c)
{
    // Remove entries that are not caller/callee
    // save/restore.
    for (auto& sr : srInfo)
    {
        for (auto entryIt = sr.saveRestoreMap.begin();
            entryIt != sr.saveRestoreMap.end();
           )
        {
            auto entry = (*entryIt);

            bool removeEntry = true;
            if (c == CallerOrCallee::Caller)
            {
                // r1 - r60
                // Remove temp movs emitted for send header
                // creation since they are not technically
                // caller save
                if (entry.first < visaKernel->getKernel()->calleeSaveStart() &&
                    entry.first >= 0 &&
                    entry.second.first == SaveRestoreInfo::RegOrMem::MemOffBEFP)
                {
                    removeEntry = false;
                }
            }
            else if (c == CallerOrCallee::Callee)
            {
                if (entry.first >= visaKernel->getKernel()->calleeSaveStart() &&
                    entry.second.first == SaveRestoreInfo::RegOrMem::MemOffBEFP)
                {
                    removeEntry = false;
                }
            }

            if (removeEntry)
            {
                entryIt = sr.saveRestoreMap.erase(entryIt);
                continue;
            }

            entryIt++;
        }
    }

#if _DEBUG
    // Ensure ordering of elements is correct, ie ascending in key value
    for (auto& sr : srInfo)
    {
        uint32_t prev = 0;
        for (auto& item : sr.saveRestoreMap)
        {
            MUST_BE_TRUE(item.first >= prev, "Unexpected ordering in container");
            prev = item.first;
        }
    }
#endif

    // Code below is to remove empty and duplicate entries
    // from both caller and callee save code.
    bool foundFirstNonEmpty = false;
    bool onSecond = false;
    SaveRestoreInfo prev;

    for (auto srIt = srInfo.begin();
        srIt != srInfo.end();
       )
    {
        auto& sr = (*srIt);

        if (!foundFirstNonEmpty)
        {
            if (sr.saveRestoreMap.size() == 0)
            {
                srIt = srInfo.erase(srIt);
                continue;
            }
            else
            {
                foundFirstNonEmpty = true;
            }
        }

        if (onSecond)
        {
            // If this one and previous one are same, eliminate this entry
            if (sr.isEqual(prev))
            {
                srIt = srInfo.erase(srIt);
                continue;
            }
        }

        prev = (*srIt);
        onSecond = true;
        srIt++;
    }
}

template<class T>
void emitDataCallerSave(VISAKernelImpl* visaKernel, T& t)
{
    auto kernel = visaKernel->getKernel();

    uint16_t numCallerSaveEntries = 0;
    // Compute total caller save entries to emit
    for (auto bbs : kernel->fg)
    {
        if (bbs->size() > 0 &&
            kernel->getKernelDebugInfo()->isFcallWithSaveRestore(bbs))
        {
            auto& callerSaveInsts = kernel->getKernelDebugInfo()->getCallerSaveInsts(bbs);
            auto& callerRestoreInsts = kernel->getKernelDebugInfo()->getCallerRestoreInsts(bbs);

            SaveRestoreManager mgr(visaKernel);
            for (auto callerSave : callerSaveInsts)
            {
                mgr.addInst(callerSave);
            }

            for (auto callerRestore : callerRestoreInsts)
            {
                mgr.addInst(callerRestore);
            }

            auto& srInfo = mgr.getSRInfo();

            mgr.sieveInstructions(SaveRestoreManager::CallerOrCallee::Caller);

            numCallerSaveEntries += (uint16_t) srInfo.size();
            for (auto sr : srInfo)
            {
                if (sr.getInst()->getGenOffset() == UNDEFINED_GEN_OFFSET)
                {
                    numCallerSaveEntries--;
                }
            }
        }
    }

    emitDataUInt16(numCallerSaveEntries, t);

    if (numCallerSaveEntries > 0)
    {
        for (auto bbs : kernel->fg)
        {
            if (bbs->size() > 0 &&
                kernel->getKernelDebugInfo()->isFcallWithSaveRestore(bbs))
            {
                auto& callerSaveInsts = kernel->getKernelDebugInfo()->getCallerSaveInsts(bbs);
                auto& callerRestoreInsts = kernel->getKernelDebugInfo()->getCallerRestoreInsts(bbs);

                SaveRestoreManager mgr(visaKernel);
                for (auto callerSave : callerSaveInsts)
                {
                    mgr.addInst(callerSave);
                }

                for (auto callerRestore : callerRestoreInsts)
                {
                    mgr.addInst(callerRestore);
                }

                mgr.sieveInstructions(SaveRestoreManager::CallerOrCallee::Caller);

                emitDataPhyRegSaveInfoPerIP(visaKernel, mgr, t);
            }
        }
    }
}

template<class T>
void emitDataCalleeSave(VISAKernelImpl* visaKernel, T& t)
{
    G4_Kernel* kernel = visaKernel->getKernel();

    SaveRestoreManager mgr(visaKernel);
    for (auto calleeSave : kernel->getKernelDebugInfo()->getCalleeSaveInsts())
    {
        mgr.addInst(calleeSave);
    }

    for (auto calleeRestore : kernel->getKernelDebugInfo()->getCalleeRestoreInsts())
    {
        mgr.addInst(calleeRestore);
    }

    uint16_t numCalleeSaveEntries = 0;
    auto& srInfo = mgr.getSRInfo();

    mgr.sieveInstructions(SaveRestoreManager::CallerOrCallee::Callee);

    numCalleeSaveEntries += (uint16_t) srInfo.size();
    for (auto sr : srInfo)
    {
        if (sr.getInst()->getGenOffset() == UNDEFINED_GEN_OFFSET)
        {
            numCalleeSaveEntries--;
        }
    }

    emitDataUInt16(numCalleeSaveEntries, t);

    emitDataPhyRegSaveInfoPerIP(visaKernel, mgr, t);
}

template<class T>
void emitDataCallFrameInfo(VISAKernelImpl* visaKernel, T& t)
{
    // Compute both be fp of current frame and previous frame
    auto kernel = visaKernel->getKernel();

    auto frameSize = kernel->getKernelDebugInfo()->getFrameSize();
    emitDataUInt16((uint16_t)frameSize, t);

    auto befpDcl = kernel->getKernelDebugInfo()->getBEFP();
    if (befpDcl)
    {
        auto befpLIInfo = kernel->getKernelDebugInfo()->getLiveIntervalInfo(befpDcl, false);
        if (befpLIInfo)
        {
            emitDataUInt8((uint8_t)1, t);
            uint32_t idx = kernel->getKernelDebugInfo()->getVarIndex(kernel->fg.framePtrDcl);
            emitDataVarLiveInterval(visaKernel, befpLIInfo, idx, sizeof(uint32_t), t);
        }
        else
        {
            emitDataUInt8((uint8_t)0, t);
        }
    }
    else
    {
        emitDataUInt8((uint8_t)0, t);
    }

    auto callerfpdcl = kernel->getKernelDebugInfo()->getCallerBEFP();
    if (callerfpdcl)
    {
        auto callerfpLIInfo = kernel->getKernelDebugInfo()->getLiveIntervalInfo(callerfpdcl, false);
        if (callerfpLIInfo)
        {
            emitDataUInt8((uint8_t)1, t);
            // Caller's be_fp is stored in frame descriptor
            emitFrameDescriptorOffsetLiveInterval(callerfpLIInfo, StackCall::FrameDescriptorOfsets::BE_FP, t);
        }
        else
        {
            emitDataUInt8((uint8_t)0, t);
        }
    }
    else
    {
        emitDataUInt8((uint8_t)0, t);
    }

    auto fretVar = kernel->getKernelDebugInfo()->getFretVar();
    if (fretVar)
    {
        auto fretVarLIInfo = kernel->getKernelDebugInfo()->getLiveIntervalInfo(fretVar, false);
        if (fretVarLIInfo)
        {
            emitDataUInt8((uint8_t)1, t);
            emitFrameDescriptorOffsetLiveInterval(fretVarLIInfo, StackCall::FrameDescriptorOfsets::Ret_IP, t);
        }
        else
        {
            emitDataUInt8((uint8_t)0, t);
        }
    }
    else
    {
        emitDataUInt8((uint8_t)0, t);
    }

    emitDataCalleeSave(visaKernel, t);

    emitDataCallerSave(visaKernel, t);
}

// compilationUnits has 1 kernel and stack call functions
// referenced by it. In case stack call functions dont
// exist in input, it only has a kernel.
template<class T>
void emitData(std::list<VISAKernelImpl*>& compilationUnits, T t)
{
    const unsigned int magic = DEBUG_MAGIC_NUMBER;
    const unsigned int numKernels = (uint32_t) compilationUnits.size();
    // Magic
    emitDataUInt32((uint32_t)magic, t);
    // Num Kernels
    emitDataUInt16((uint16_t)numKernels, t);

    auto cunitsItEnd = compilationUnits.end();
    for (auto cunitsIt = compilationUnits.begin();
        cunitsIt != cunitsItEnd;
        cunitsIt++)
    {
        VISAKernelImpl* curKernel = (*cunitsIt);

        emitDataName(curKernel->getName(), t);

        uint32_t reloc_offset = 0;
        if (curKernel->getIsKernel())
        {
            emitDataUInt32((uint32_t)reloc_offset, t);
        }
        else
        {
            reloc_offset = curKernel->getKernel()->getKernelDebugInfo()->getRelocOffset();
            emitDataUInt32((uint32_t)reloc_offset, t);
        }

        // Emit CISA Offset:Gen Offset mapping
        const unsigned int numElementsCISAOffsetMap = (uint32_t) curKernel->getKernel()->getKernelDebugInfo()->getMapCISAOffsetGenOffset().size();
        // Num elements
        emitDataUInt32((uint32_t)numElementsCISAOffsetMap, t);

        // Emit out actual CISA Offset:Gen Offset mapping elements
        for (unsigned int i = 0; i < numElementsCISAOffsetMap; i++)
        {
            const unsigned int cisaOffset = curKernel->getKernel()->getKernelDebugInfo()->getMapCISAOffsetGenOffset()[i].first;
            const unsigned int genOffset = curKernel->getKernel()->getKernelDebugInfo()->getMapCISAOffsetGenOffset()[i].second -
                (unsigned int)reloc_offset;

            // Write cisa offset and gen offset
            emitDataUInt32((uint32_t)cisaOffset, t);
            emitDataUInt32((uint32_t)genOffset, t);
        }

        // Emit CISA index:Gen Offset mapping
        const unsigned int numElementsCISAIndexMap = (uint32_t) curKernel->getKernel()->getKernelDebugInfo()->getMapCISAIndexGenOffset().size();
        // Num elements
        emitDataUInt32((uint32_t)numElementsCISAIndexMap, t);

        // Emit out actual CISA index:Gen Offset mapping
        for (unsigned int i = 0; i < numElementsCISAIndexMap; i++)
        {
            const unsigned int cisaIndex = curKernel->getKernel()->getKernelDebugInfo()->getMapCISAIndexGenOffset()[i].first;
            const unsigned int genOffset = curKernel->getKernel()->getKernelDebugInfo()->getMapCISAIndexGenOffset()[i].second -
                (unsigned int)reloc_offset;

            // Write cisa index and gen offset
            emitDataUInt32((uint32_t)cisaIndex, t);
            emitDataUInt32((uint32_t)genOffset, t);
        }

        // All variables present in varMap need not be present in
        // mapDclName. Only those variables seen when constructing
        // symbol table will be added to mapDclName. So compute
        // number of elements that will be written out.
        unsigned int numItems = 0;
        std::map<G4_Declare*, std::pair<const char*, unsigned int>> mapDclName;
        // Compute items to write to debug info.
        // Sum variables of all types present in symbol table
        // created at build time, and subtract number of pre-
        // defined variables.
        populateMapDclName(curKernel, mapDclName);

        const unsigned int numElementsVarMap = (uint32_t) curKernel->getKernel()->getKernelDebugInfo()->getVarsMap().size();

        for (unsigned int i = 0; i < numElementsVarMap; i++)
        {
            G4_Declare* dcl = curKernel->getKernel()->getKernelDebugInfo()->getVarsMap()[i]->dcl;
            if (mapDclName.find(dcl) == mapDclName.end())
            {
                continue;
            }

            numItems++;
        }

        // Emit out number of variable mapping items
        emitDataUInt32((uint32_t)numItems, t);

        // Emit out actual Virtual Register:Physical Register mapping elements
        for (unsigned int i = 0; i < numElementsVarMap; i++)
        {
            G4_Declare* dcl = curKernel->getKernel()->getKernelDebugInfo()->getVarsMap()[i]->dcl;
            if (mapDclName.find(dcl) == mapDclName.end())
            {
                continue;
            }

            const std::pair<const char*, unsigned int>& dclInfo = mapDclName.find(dcl)->second;
            std::string varName(dclInfo.first);
            // to_string support not present prior to gcc 4.6 and is a c++11 feature

#if ANDROID
            {
                char t_char[128];
                snprintf(t_char, sizeof(t_char), "%d", dclInfo.second);
                varName += std::string(t_char);
            }
#elif defined(_MSC_VER) && _MSC_VER < 1700
            varName += std::to_string((_ULonglong)dclInfo.second);
#else
            varName += std::to_string(dclInfo.second);
#endif
            emitDataName(varName.c_str(), t);

            // Insert live-interval information
            LiveIntervalInfo* lrInfo = curKernel->getKernel()->getKernelDebugInfo()->getLiveIntervalInfo(dcl, false);
            emitDataVarLiveInterval(curKernel, lrInfo, i, sizeof(uint16_t), t);
        }

        // emit sub-routine data
        emitDataSubroutines(curKernel, t);

        emitDataCallFrameInfo(curKernel, t);
    }
}

void emitDebugInfo(VISAKernelImpl* curKernel, std::string filename)
{
    std::list<VISAKernelImpl*> functions;
    emitDebugInfo(curKernel, functions, filename);
}

extern "C" void* allocCodeBlock(size_t sz);

void emitDebugInfoToMem(VISAKernelImpl* kernel, std::list<VISAKernelImpl*>& functions, void*& info, unsigned& size)
{
    std::vector<unsigned char>  vec;
    std::list<VISAKernelImpl*> compilationUnits;
    compilationUnits.push_back(kernel);
    auto funcItEnd = functions.end();
    for (auto funcIt = functions.begin();
        funcIt != funcItEnd;
        funcIt++)
    {
        if ((*funcIt)->getKernel()->getKernelDebugInfo()->getRelocOffset() != 0)
        {
            compilationUnits.push_back((*funcIt));
        }
    }

    emitData<std::vector<unsigned char>&>(compilationUnits, vec);

    info = allocCodeBlock(vec.size());
    memcpy_s(info, vec.size(), vec.data(), vec.size());
    size = (uint32_t) vec.size();
}

void emitDebugInfoToMem(VISAKernelImpl* curKernel, void*& info, unsigned& size)
{
    std::list<VISAKernelImpl*> compilationUnits;

    emitDebugInfoToMem(curKernel, compilationUnits, info, size);
}

void* KernelDebugInfo::operator new(size_t sz, Mem_Manager& m)
{
    return m.alloc(sz);
}

KernelDebugInfo::KernelDebugInfo()
{
    visaKernel = nullptr;
    saveCallerFP = nullptr;
    restoreCallerFP = nullptr;
    setupFP = nullptr;
    restoreSP = nullptr;
    frameSize = 0;
    fretVar = nullptr;
    reloc_offset = 0;
    missingVISAIdsComputed = false;
}

void KernelDebugInfo::updateRelocOffset()
{
    // This function updates reloc_offset field of kernel
    // reloc_offset field for kernels is 0.
    // reloc_offset field for stack call function is set
    // to byte offset of first gen binary instruction
    // in binary buffer.

    bool done = false;
    BB_LIST_ITER bbItEnd = getKernel().fg.end();
    for (auto bbIt = getKernel().fg.begin();
        bbIt != bbItEnd && done == false;
        bbIt++)
    {
        G4_BB* bb = (*bbIt);
        INST_LIST_ITER instItEnd = bb->end();
        for (auto instIt = bb->begin();
            instIt != instItEnd;
            instIt++)
        {
            G4_INST* inst = (*instIt);
            if (inst->getGenOffset() != UNDEFINED_GEN_OFFSET)
            {
                reloc_offset = (uint32_t)inst->getGenOffset();
                done = true;
                break;
            }
        }
    }
}

void emitDebugInfo(VISAKernelImpl* kernel, std::list<VISAKernelImpl*>& functions, std::string debugFileNameStr)
{
    std::list<VISAKernelImpl*> compilationUnits;
    compilationUnits.push_back(kernel);
    auto funcItEnd = functions.end();
    for (auto funcIt = functions.begin();
        funcIt != funcItEnd;
        funcIt++)
    {
        if ((*funcIt)->getKernel()->getKernelDebugInfo()->getRelocOffset() != 0)
        {
            // Include compilation unit only if
            // it is referenced, ie reloc_offset
            // for gen binary is non-zero.
            compilationUnits.push_back((*funcIt));
        }
    }
#ifdef DEBUG_VERBOSE_ON
    addCallFrameInfo(kernel);

    for (auto& funcIt : functions)
    {
        addCallFrameInfo(funcIt);
    }
#endif

    FILE* dbgFile = fopen(debugFileNameStr.c_str(), "wb+");

    if (dbgFile == NULL)
    {
        std::cerr << "Error opening debug file " << debugFileNameStr << ". Not emitting debug info.\n";
        return;
    }

    emitData(compilationUnits, dbgFile);

    fclose(dbgFile);
}

void resetGenOffsets(G4_Kernel& kernel)
{
    // Iterate over all instructions in kernel and set gen
    // offset of BinInst instance to 0.
    auto bbItEnd = kernel.fg.end();
    for (auto bbIt = kernel.fg.begin();
        bbIt != bbItEnd;
        bbIt++)
    {
        G4_BB* bb = (*bbIt);

        auto instItEnd = bb->end();
        for (auto instIt = bb->begin();
            instIt != instItEnd;
            instIt++)
        {
            G4_INST* inst = (*instIt);

            if (inst->getGenOffset() != UNDEFINED_GEN_OFFSET)
            {
                inst->setGenOffset(UNDEFINED_GEN_OFFSET);
            }
        }
    }
}

void updateDebugInfo(G4_Kernel& kernel, G4_INST* inst, LivenessAnalysis& liveAnalysis, LiveRange* lrs[], BitSet& live, DebugInfoState* state,
    bool closeAllOpenIntervals)
{
    if (closeAllOpenIntervals && !state->getPrevInst())
        return;

    auto krnlDbgInfo = kernel.getKernelDebugInfo();

    // Update live-intervals only when bits change in bit-vector.
    // state parameter contains previous instruction and bit-vector.
    for (unsigned int i = 0; i < liveAnalysis.getNumSelectedVar(); i += NUM_BITS_PER_ELT)
    {
        auto elt = live.getElt(i / NUM_BITS_PER_ELT);
        auto prevElt = state->getPrevBitset() ? state->getPrevBitset()->getElt(i / NUM_BITS_PER_ELT) : 0;

        if (elt != prevElt)
        {
            // Some variables have changed state in bit-vector, so update their states accordingly.
            //
            // If elt is set and prevElt is reset, it means the variable became live at current inst,
            // If elt is reset and prevElt is set, it means the variable was killed at current inst
            //
            for (unsigned int j = 0; j < NUM_BITS_PER_ELT; j++)
            {
                unsigned char eltJ = (elt >> j) & 0x1;
                unsigned char prevEltJ = (prevElt >> j) & 0x1;

                if (eltJ == 1 && prevEltJ == 0)
                {
                    if (inst->getCISAOff() != UNMAPPABLE_VISA_INDEX)
                    {
                        // This check guarantees that for an open
                        // interval, at least the same CISA offset
                        // can be used to close it. If there is no
                        // instruction with valid CISA offset
                        // between open/close IR instruction, then
                        // the interval will not be recorded.
                        auto idx = (i + j);
                        G4_Declare* dcl = lrs[idx]->getVar()->getDeclare();
                        auto lr = krnlDbgInfo->getLiveIntervalInfo(dcl);

                        lr->setStateOpen(inst->getCISAOff());
                    }
                }
                else if (eltJ == 0 && prevEltJ == 1)
                {
                    auto idx = (i + j);
                    G4_Declare* dcl = lrs[idx]->getVar()->getDeclare();

                    auto lr = krnlDbgInfo->getLiveIntervalInfo(dcl);

                    if (lr->getState() == LiveIntervalInfo::DebugLiveIntervalState::Open)
                    {
                        auto closeAt = state->getPrevInst()->getCISAOff();
                        while (closeAt >= 1 &&
                            krnlDbgInfo->isMissingVISAId(closeAt - 1))
                        {
                            closeAt--;
                        }
                        lr->setStateClosed(closeAt);
                    }
                }
            }
        }

        if (closeAllOpenIntervals)
        {
            for (unsigned int j = 0; j < NUM_BITS_PER_ELT; j++)
            {
                unsigned char eltJ = (elt >> j) & 0x1;

                if (eltJ)
                {
                    auto idx = (i + j);
                    G4_Declare* dcl = lrs[idx]->getVar()->getDeclare();
                    auto lr = krnlDbgInfo->getLiveIntervalInfo(dcl);

                    if (lr->getState() == LiveIntervalInfo::DebugLiveIntervalState::Open)
                    {
                        uint32_t lastCISAOff = (inst->getCISAOff() != UNMAPPABLE_VISA_INDEX) ?
                            inst->getCISAOff() : state->getPrevInst()->getCISAOff();

                        while (lastCISAOff >= 1 &&
                            krnlDbgInfo->isMissingVISAId(lastCISAOff - 1))
                        {
                            lastCISAOff--;
                        }

                        lr->setStateClosed(lastCISAOff);
                    }
                }
            }
        }
    }

    if (inst->getCISAOff() != UNMAPPABLE_VISA_INDEX &&
        !inst->isPseudoKill())
    {
        state->setPrevBitset(live);
        state->setPrevInst(inst);
    }
}

void updateDebugInfo(G4_Kernel& kernel,  std::vector<LocalLiveRange*>& liveIntervals)
{
    for (auto lr : liveIntervals)
    {
        if (lr->getAssigned())
        {
            uint32_t start, end;
            G4_INST* startInst = lr->getFirstRef(start);
            G4_INST* endInst = lr->getLastRef(end);
            start = startInst->getCISAOff();
            end = endInst->getCISAOff();

            auto lrInfo = kernel.getKernelDebugInfo()->getLiveIntervalInfo(lr->getTopDcl());
            if (start != UNMAPPABLE_VISA_INDEX &&
                end != UNMAPPABLE_VISA_INDEX)
            {
                lrInfo->addLiveInterval(start, end);
            }
        }
    }
}

void updateDebugInfo(G4_Kernel& kernel, std::vector<std::tuple<G4_Declare*, G4_INST*, G4_INST*>> augmentationLiveIntervals)
{
    // Invoked via augmentation pass
    for (auto& lr : augmentationLiveIntervals)
    {
        uint32_t start, end;
        G4_INST* startInst = std::get<1>(lr);
        G4_INST* endInst = std::get<2>(lr);
        start = startInst->getCISAOff();
        end = endInst->getCISAOff();

        G4_Declare* topdcl = std::get<0>(lr);
        while (std::get<0>(lr)->getAliasDeclare() != NULL)
        {
            topdcl = topdcl->getAliasDeclare();
        }

        auto lrInfo = kernel.getKernelDebugInfo()->getLiveIntervalInfo(topdcl);
        if (start != UNMAPPABLE_VISA_INDEX &&
            end != UNMAPPABLE_VISA_INDEX)
        {
            lrInfo->addLiveInterval(start, end);
        }
    }
}

void updateDebugInfo(G4_Kernel& kernel, G4_Declare* dcl, uint32_t start, uint32_t end)
{
    auto lrInfo = kernel.getKernelDebugInfo()->getLiveIntervalInfo(dcl);
    if (start != UNMAPPABLE_VISA_INDEX &&
        end != UNMAPPABLE_VISA_INDEX)
    {
        lrInfo->addLiveInterval(start, end);
    }
}

void updateDebugInfo(G4_Kernel& kernel, G4_Declare* dcl, uint32_t offset)
{
    auto lrInfo = kernel.getKernelDebugInfo()->getLiveIntervalInfo(dcl);
    lrInfo->liveAt(offset);
}

uint32_t getBinInstSize(G4_INST* inst)
{
    uint32_t size = inst->isCompactedInst() ?
        (BYTES_PER_INST / 2) : BYTES_PER_INST;

    return size;
}

void KernelDebugInfo::computeDebugInfo(std::list<G4_BB*>& stackCallEntryBBs)
{
    updateMapping(stackCallEntryBBs);
    updateRelocOffset();
    if (reloc_offset > 0)
    {
        updateCallStackLiveIntervals();
    }
    else
    {
        updateCallStackMain();
    }
}

void KernelDebugInfo::updateCallStackMain()
{
    if (!getKernel().fg.getHasStackCalls())
        return;

    // Set live-interval for BE_FP
    auto befp = getBEFP();
    if (befp)
    {
        uint32_t start = 0;
        if (getBEFPSetupInst())
        {
            start = (uint32_t)getBEFPSetupInst()->getGenOffset() +
                (uint32_t)getBinInstSize(getBEFPSetupInst());
        }
        updateDebugInfo(getKernel(), befp, start, mapCISAIndexGenOffset.back().second);
    }
}

void KernelDebugInfo::updateCallStackLiveIntervals()
{
    if (!getKernel().fg.getIsStackCallFunc() &&
        !getKernel().fg.getHasStackCalls())
    {
        return;
    }

    uint32_t reloc_offset = 0;
    uint32_t start = 0xffffffff, end = 0;

    // Update live-interval for following ranges:
    // be_fp, caller_be_fp, retval
    if (getKernel().fg.getIsStackCallFunc())
    {
        // Only stack call function has return variable
        auto fretVar = getKernel().getKernelDebugInfo()->getFretVar();
        auto fretVarLI = getKernel().getKernelDebugInfo()->getLiveIntervalInfo(fretVar);
        fretVarLI->clearLiveIntervals();

        for (auto bbs : getKernel().fg)
        {
            for (auto insts : *bbs)
            {
                if (insts->getGenOffset() != UNDEFINED_GEN_OFFSET)
                {
                    reloc_offset = (reloc_offset == 0) ?
                        (uint32_t)insts->getGenOffset() : reloc_offset;
                    break;
                }
            }
            if (reloc_offset > 0)
                break;
        }

        uint32_t start = 0;
        if (getBEFPSetupInst())
        {
            // Frame descriptor can be addressed once once BE_FP is defined
            start = (uint32_t)getBEFPSetupInst()->getGenOffset() +
                getBinInstSize(getBEFPSetupInst());
        }

        if (getCallerBEFPRestoreInst())
        {
            end = (uint32_t)getCallerBEFPRestoreInst()->getGenOffset();
        }

        MUST_BE_TRUE(end >= reloc_offset, "Failed to update live-interval for retval");
        MUST_BE_TRUE(start >= reloc_offset, "Failed to update start for retval");
        MUST_BE_TRUE(end >= start, "end less then start for retval");
        for (uint32_t i = start - reloc_offset; i <= end - reloc_offset; i++)
        {
            updateDebugInfo(*kernel, fretVar, i);
        }
    }

    auto befp = getBEFP();
    if (befp)
    {
        auto befpLIInfo = getLiveIntervalInfo(befp);
        befpLIInfo->clearLiveIntervals();
        auto befpSetupInst = getBEFPSetupInst();
        if (befpSetupInst)
        {
            start = (uint32_t)befpSetupInst->getGenOffset() +
                getBinInstSize(befpSetupInst);
            auto spRestoreInst = getCallerSPRestoreInst();
            if (spRestoreInst)
            {
                end = (uint32_t)spRestoreInst->getGenOffset();
            }
            for (uint32_t i = start - reloc_offset; i <= end - reloc_offset; i++)
            {
                updateDebugInfo(*kernel, befp, i);
            }
        }

        MUST_BE_TRUE(start != 0xffffffff, "Cannot update stack vars1");
        MUST_BE_TRUE(end != 0, "Cannot update stack vars2");
    }

    auto callerbefp = getCallerBEFP();
    if (callerbefp)
    {
        auto callerbefpLIInfo = getLiveIntervalInfo(callerbefp);
        callerbefpLIInfo->clearLiveIntervals();
        auto callerbeSaveInst = getCallerBEFPSaveInst();
        if (callerbeSaveInst)
        {
            auto callerbefpRestoreInst = getCallerBEFPRestoreInst();
            MUST_BE_TRUE(callerbefpRestoreInst != nullptr,
                "Instruction destroying caller be fp not found in epilog");
            start = (uint32_t)callerbeSaveInst->getGenOffset() - reloc_offset +
                getBinInstSize(callerbeSaveInst);
            end = (uint32_t)callerbefpRestoreInst->getGenOffset() - reloc_offset;
            for (uint32_t i = start;
                i <= end;
                i++)
            {
                updateDebugInfo(*kernel, callerbefp, i);
            }
        }
    }
}

void KernelDebugInfo::updateExpandedIntrinsic(G4_InstIntrinsic* spillOrFill, G4_INST* inst)
{
    // This function looks up all caller/callee save code added.
    // Once it finds "spillOrFill", it adds inst to it. This is
    // because VISA now uses spill/fill intrinsics to model
    // save/restore. These intrinsics are expanded after RA is
    // done. So this method gets invoked after RA is done and
    // when intrinsics are expanded.
    for (auto& k : callerSaveRestore)
    {
        for (auto it = k.second.first.begin(); it != k.second.first.end(); ++it)
        {
            if ((*it) == spillOrFill)
            {
                k.second.first.insert(it, inst);
                return;
            }
        }

        for (auto it = k.second.second.begin(); it != k.second.second.end(); ++it)
        {
            if ((*it) == spillOrFill)
            {
                k.second.second.insert(it, inst);
                return;
            }
        }
    }

    for (auto it = calleeSaveRestore.first.begin(); it != calleeSaveRestore.first.end(); ++it)
    {
        if ((*it) == spillOrFill)
        {
            calleeSaveRestore.first.insert(it, inst);
            return;
        }
    }

    for (auto it = calleeSaveRestore.second.begin(); it != calleeSaveRestore.second.end(); ++it)
    {
        if ((*it) == spillOrFill)
        {
            calleeSaveRestore.second.insert(it, inst);
            return;
        }
    }
}

void KernelDebugInfo::addCallerSaveInst(G4_BB* fcallBB, G4_INST* inst)
{
    callerSaveRestore[fcallBB].first.push_back(inst);
}

void KernelDebugInfo::addCallerRestoreInst(G4_BB* fcallBB, G4_INST* inst)
{
    callerSaveRestore[fcallBB].second.push_back(inst);
}

void KernelDebugInfo::addCalleeSaveInst(G4_INST* inst)
{
    calleeSaveRestore.first.push_back(inst);
}

void KernelDebugInfo::addCalleeRestoreInst(G4_INST* inst)
{
    calleeSaveRestore.second.push_back(inst);
}

std::vector<G4_INST*>& KernelDebugInfo::getCallerSaveInsts(G4_BB* fcallBB)
{
    return callerSaveRestore[fcallBB].first;
}

std::vector<G4_INST*>& KernelDebugInfo::getCallerRestoreInsts(G4_BB* fcallBB)
{
    return callerSaveRestore[fcallBB].second;
}

std::vector<G4_INST*>& KernelDebugInfo::getCalleeSaveInsts()
{
    return calleeSaveRestore.first;
}

std::vector<G4_INST*>& KernelDebugInfo::getCalleeRestoreInsts()
{
    return calleeSaveRestore.second;
}

bool KernelDebugInfo::isFcallWithSaveRestore(G4_BB* bb)
{
    // Debug emission happens after binary encoding
    // at which point all fcalls are converted to
    // calls. So G4_INST::isFCall() will always
    // be false
    bool retval = false;
    auto it = callerSaveRestore.find(bb);
    if (it != callerSaveRestore.end())
    {
        retval = true;
    }

    return retval;
}

// Compute extra instructions in insts over oldInsts list and
// return a new list.
INST_LIST KernelDebugInfo::getDeltaInstructions(G4_BB* bb)
{
    INST_LIST deltaInsts;
    for (auto instIt = bb->begin(); instIt != bb->end(); instIt++)
        deltaInsts.push_back(*instIt);

    for (auto oldInstsIt : oldInsts)
    {
        deltaInsts.remove(oldInstsIt);
    }

    return deltaInsts;
}

void SaveRestoreManager::addInst(G4_INST* inst)
{
    SaveRestoreInfo newSVInfo;
    srInfo.push_back(newSVInfo);
    if (srInfo.size() > 1)
    {
        // Copy over from previous
        // so emitted data is
        // cumulative per IP.
        srInfo[srInfo.size() - 1].saveRestoreMap = srInfo[srInfo.size() - 2].saveRestoreMap;
    }

    if (inst->opcode() == G4_add &&
        inst->getSrc(1) &&
        inst->getSrc(1)->isImm() &&
        inst->getSrc(0) &&
        inst->getSrc(0)->isSrcRegRegion() &&
        GetTopDclFromRegRegion(inst->getSrc(0)) == visaKernel->getKernel()->fg.builder->getBEFP())
    {
        memOffset = (int32_t)inst->getSrc(1)->asImm()->getImm();
        regWithMemOffset = inst->getDst()->getLinearizedStart() / numEltPerGRF(Type_UB);
        absOffset = false;
    }

    if (inst->opcode() == G4_mov &&
        inst->getSrc(0) &&
        inst->getSrc(0)->isImm() &&
        inst->getExecSize() == g4::SIMD1 &&
        inst->getDst() &&
        inst->getDst()->getLinearizedStart() % numEltPerGRF(Type_UB) == 8)
    {
        memOffset = (int32_t)inst->getSrc(0)->asImm()->getImm();
        regWithMemOffset = inst->getDst()->getLinearizedStart() / numEltPerGRF(Type_UB);
        absOffset = true;
    }

    srInfo.back().update(inst, memOffset, regWithMemOffset, absOffset);
}

void SaveRestoreManager::emitAll()
{
    for (auto it : srInfo)
    {
#ifdef DEBUG_VERBOSE_ON
        it.getInst()->emit(std::cerr);
#endif
        DEBUG_VERBOSE("\n");

        for (auto mapIt : it.saveRestoreMap)
        {
            DEBUG_VERBOSE("\tr" << mapIt.first << ".0 (8):d saved to ");
            if (mapIt.second.first == SaveRestoreInfo::RegOrMem::Reg)
            {
                DEBUG_VERBOSE("r" << mapIt.second.second.regNum << ".0 (8):d\n");
            }
            else if (mapIt.second.first == SaveRestoreInfo::RegOrMem::MemAbs)
            {
                DEBUG_VERBOSE("mem at offset " << mapIt.second.second.offset << " bytes (abs)\n");
            }
            else if (mapIt.second.first == SaveRestoreInfo::RegOrMem::MemOffBEFP)
            {
                DEBUG_VERBOSE("mem at offset " << mapIt.second.second.offset << " bytes (off befp)\n");
            }
        }
    }
}

void SaveRestoreInfo::update(G4_INST* inst, int32_t memOffset, uint32_t regWithMemOffset, bool isOffAbs)
{
    i = inst;

    if (inst->getDst() &&
        inst->getDst()->isDstRegRegion())
    {
        auto dstreg = inst->getDst()->getLinearizedStart() / numEltPerGRF(Type_UB);

        // Remove any item in map that is saved as storage for some other reg.
        for (auto mapIt : saveRestoreMap)
        {
            if (mapIt.second.first == RegOrMem::Reg &&
                mapIt.second.second.regNum == dstreg)
            {
                DEBUG_VERBOSE("Removed r" << mapIt.second.second.regNum << ".0 (8):d\n");
                saveRestoreMap.erase(mapIt.first);
                break;
            }
        }
    }

    if (inst->opcode() == G4_mov &&
        inst->getDst()->isDstRegRegion() &&
        inst->getSrc(0)->isSrcRegRegion())
    {
        unsigned int srcreg, dstreg;
        srcreg = inst->getSrc(0)->getLinearizedStart() / numEltPerGRF(Type_UB);
        dstreg = inst->getDst()->getLinearizedStart() / numEltPerGRF(Type_UB);

        bool done = false;
        for (auto mapIt : saveRestoreMap)
        {
            if (mapIt.second.first == RegOrMem::Reg &&
                mapIt.second.second.regNum == srcreg &&
                mapIt.first == dstreg)
            {
                saveRestoreMap.erase(mapIt.first);
                done = true;
                DEBUG_VERBOSE("Restored r" << dstreg << ".0 (8):d from r" << srcreg << ".0 (8):d\n");
                break;
            }
        }

        if (done == false)
        {
            auto it = saveRestoreMap.find(srcreg);
            if (it == saveRestoreMap.end())
            {
                // Entry not found so update map
                RegMap mapping;
                mapping.regNum = dstreg;

                saveRestoreMap.insert(std::make_pair(srcreg, std::make_pair(RegOrMem::Reg, mapping)));

                DEBUG_VERBOSE("Saved r" << srcreg << ".0 (8):d to r" << dstreg << ".0 (8):d\n");
            }
        }
    }
    else if (inst->isSend())
    {
        // send/read, send/write
        // sends/read, sends/write
        MUST_BE_TRUE(inst->getMsgDesc()->isScratchRW() == false,
            "Not expecting scratch msg in save/restore code");
        if (inst->getMsgDesc()->isDataPortWrite())
        {
            uint32_t srcreg, extsrcreg = 0;
            srcreg = inst->getSrc(0)->getLinearizedStart() / numEltPerGRF(Type_UB);
            if (inst->getMsgDesc()->extMessageLength() > 0)
            {
                extsrcreg = inst->getSrc(1)->getLinearizedStart()/numEltPerGRF(Type_UB);
            }

            MUST_BE_TRUE(memOffset != 0xffff, "Invalid mem offset");
            MUST_BE_TRUE(regWithMemOffset == srcreg, "Send src not initialized with offset");

            std::vector<uint32_t> payloadRegs;
            for (uint32_t i = 1; i < inst->getMsgDesc()->MessageLength(); i++)
            {
                payloadRegs.push_back(i + srcreg);
            }
            for (uint32_t i = 0; i < inst->getMsgDesc()->extMessageLength(); i++)
            {
                payloadRegs.push_back(i + extsrcreg);
            }

            for (uint32_t i = 0; i < payloadRegs.size(); i++)
            {
                uint32_t payloadReg = payloadRegs[i];
                RegMap m;
                m.offset = (int32_t)((memOffset * numEltPerGRF(Type_UB)/2) + (i * numEltPerGRF(Type_UB)));
                m.isAbs = isOffAbs;
                saveRestoreMap.insert(std::make_pair(payloadReg,
                    std::make_pair(isOffAbs ? RegOrMem::MemAbs : RegOrMem::MemOffBEFP, m)));

#ifdef DEBUG_VERBOSE_ON
                const char* offstr = isOffAbs ? "(abs)" : "(off besp)";

                DEBUG_VERBOSE("Saved r" << payloadReg << ".0 (8):d to mem at offset "
                    << m.offset << " bytes" << offstr << "\n");
#endif
            }
        }
        else if (inst->getMsgDesc()->isDataPortRead())
        {
            uint32_t srcreg, dstreg;
            srcreg = inst->getSrc(0)->getLinearizedStart() / numEltPerGRF(Type_UB);
            dstreg = inst->getDst()->getLinearizedStart() / numEltPerGRF(Type_UB);

            MUST_BE_TRUE(memOffset != 0xffff, "Invalid mem offset");
            MUST_BE_TRUE(regWithMemOffset == srcreg, "Send src not initialized with offset");

            auto responselen = inst->getMsgDesc()->ResponseLength();
            int32_t startoff;
            startoff = memOffset * numEltPerGRF(Type_UB) / 2;

            for (auto reg = dstreg; reg < (responselen + dstreg); reg++)
            {
                int32_t offsetForReg = startoff + ((reg - dstreg) * numEltPerGRF(Type_UB));

                for (auto mapIt : saveRestoreMap)
                {
                    if (mapIt.first == reg &&
                        (mapIt.second.first == RegOrMem::MemAbs ||
                        mapIt.second.first == RegOrMem::MemOffBEFP) &&
                        mapIt.second.second.offset == offsetForReg)
                    {
                        saveRestoreMap.erase(mapIt.first);

#ifdef DEBUG_VERBOSE_ON
                        const char* offstr = RegOrMem::MemAbs ? "abs" : "off befp";
                        DEBUG_VERBOSE("Restored r" << reg << ".0 (8):d from mem offset " <<
                            offsetForReg << " bytes (" << offstr << ")\n");
#endif
                        break;
                    }
                }
            }
        }
    }
}

#ifdef DEBUG_VERBOSE
void dumpLiveInterval(LiveIntervalInfo* lv)
{
    std::vector<std::pair<unsigned int, unsigned int>> v;
    lv->getLiveIntervals(v);
    for (auto it : v)
    {
        std::cerr << "(" << it.first << ", " << it.second << ")\n";
    }
}

void emitSubRoutineInfo(VISAKernelImpl* visaKernel)
{
    auto kernel = visaKernel->getKernel();

    for (auto bb : kernel->fg)
    {
        G4_INST* firstInst = nullptr;
        G4_INST* lastInst = nullptr;
        unsigned int start = 0, end = 0;
        G4_Declare* retval = nullptr;
        G4_Label* subLabel = nullptr;
        if (bb->isEndWithCall())
        {
            G4_BB* calleeBB = bb->Succs.front();
            while (firstInst == NULL && calleeBB != NULL)
            {
                if (calleeBB->size() > 0)
                {
                    firstInst = calleeBB->front();
                    start = firstInst->getCISAOff();
                    subLabel = firstInst->getSrc(0)->asLabel();
                }
            }

            calleeBB = bb->BBAfterCall()->Preds.front();
            while (lastInst == NULL && calleeBB != NULL)
            {
                if (calleeBB->size() > 0)
                {
                    lastInst = calleeBB->back();
                    end = lastInst->getCISAOff();
                    MUST_BE_TRUE(lastInst->isReturn(), "Expecting to see G4_return as last inst in sub-routine");
                    retval = lastInst->getSrc(0)->asSrcRegRegion()->getBase()->asRegVar()->getDeclare()->getRootDeclare();
                }

                calleeBB = calleeBB->Preds.front();
            }
            std::cerr << "Func info id " << subLabel->getLabel() << "\n";
            std::cerr << "First inst " << start << ", last inst " << end << "\n";
            std::cerr << "Return value in dcl " << retval->getName() << "\n";

            if (kernel->getKernelDebugInfo()->getLiveIntervalInfo(retval, false) != NULL)
            {
                std::cerr << "Found live-interval for retval range\n";
                auto lv = kernel->getKernelDebugInfo()->getLiveIntervalInfo(retval, false);
                dumpLiveInterval(lv);
                std::cerr << "\n";
            }
        }
    }
}

void emitBEFP(VISAKernelImpl* visaKernel)
{
    // Compute both be fp of current frame and previous frame
    auto kernel = visaKernel->getKernel();
    auto befpDcl = kernel->getKernelDebugInfo()->getBEFP();
    if (befpDcl &&
        kernel->getKernelDebugInfo()->getLiveIntervalInfo(befpDcl, false))
    {
        std::cerr << "Found befp dcl at " << befpDcl->getName() << "\n";
        dumpLiveInterval(kernel->getKernelDebugInfo()->getLiveIntervalInfo(befpDcl, false));
        std::cerr << "\n";
    }

    auto befpSetup = kernel->getKernelDebugInfo()->getBEFPSetupInst();
    if (befpSetup)
    {
        std::cerr << "befp setup inst found:\n";
        befpSetup->emit(std::cerr);
        std::cerr << "\n";
    }

    auto spRestore = kernel->getKernelDebugInfo()->getCallerSPRestoreInst();
    if (spRestore)
    {
        std::cerr << "sp restore inst found:\n";
        spRestore->emit(std::cerr);
        std::cerr << "\n";
    }

    auto callerfpdcl = kernel->getKernelDebugInfo()->getCallerBEFP();
    if (callerfpdcl &&
        kernel->getKernelDebugInfo()->getLiveIntervalInfo(callerfpdcl, false))
    {
        std::cerr << "Found caller befp dcl at " << callerfpdcl->getName() << "\n";
        dumpLiveInterval(kernel->getKernelDebugInfo()->getLiveIntervalInfo(callerfpdcl, false));
        std::cerr << "\n";
    }

    auto fretVar = kernel->getKernelDebugInfo()->getFretVar();
    if (fretVar &&
        kernel->getKernelDebugInfo()->getLiveIntervalInfo(fretVar, false))
    {
        std::cerr << "fretvar " << fretVar->getName() << "\n";
        dumpLiveInterval(kernel->getKernelDebugInfo()->getLiveIntervalInfo(fretVar, false));
        std::cerr << "\n";
    }

    auto frameSize = kernel->getKernelDebugInfo()->getFrameSize();
    std::cerr << "frame size = " << frameSize << " bytes" << "\n";
}

void emitCallerSaveInfo(VISAKernelImpl* visaKernel)
{
    auto kernel = visaKernel->getKernel();

    for (auto bbs : kernel->fg)
    {
        if (bbs->size() > 0 &&
            kernel->getKernelDebugInfo()->isFcallWithSaveRestore(bbs))
        {
            auto& callerSaveInsts = kernel->getKernelDebugInfo()->getCallerSaveInsts(bbs);
            auto& callerRestoreInsts = kernel->getKernelDebugInfo()->getCallerRestoreInsts(bbs);

            std::cerr << "Caller save for ";
            bbs->back()->emit(std::cerr);
            std::cerr << "\n";

            SaveRestoreManager mgr(visaKernel);
            for (auto callerSave : callerSaveInsts)
            {
                mgr.addInst(callerSave);
            }

            for (auto callerRestore : callerRestoreInsts)
            {
                mgr.addInst(callerRestore);
            }

            mgr.emitAll();

            std::cerr << "\n";
        }
    }
}

void emitCalleeSaveInfo(VISAKernelImpl* visaKernel)
{
    G4_Kernel* kernel = visaKernel->getKernel();

    std::cerr << "\nCallee save:\n";
    SaveRestoreManager mgr(visaKernel);
    for (auto calleeSave : kernel->getKernelDebugInfo()->getCalleeSaveInsts())
    {
        mgr.addInst(calleeSave);
    }

    for (auto calleeRestore : kernel->getKernelDebugInfo()->getCalleeRestoreInsts())
    {
        mgr.addInst(calleeRestore);
    }

    mgr.emitAll();

    std::cerr << "\n";
}

void dumpCFG(VISAKernelImpl* visaKernel)
{
    G4_Kernel* kernel = visaKernel->getKernel();
    auto reloc_offset = 0;
    bool done = false;

    for (auto bbs : kernel->fg)
    {
        for (auto insts : *bbs)
        {
            if (insts->getGenOffset() != UNDEFINED_GEN_OFFSET)
            {
                if (!done)
                {
                    reloc_offset = (uint32_t)insts->getGenOffset();
                    done = true;
                }
                std::cerr << insts->getGenOffset() - reloc_offset;
            }
            std::cerr << "\t";
            insts->emit(std::cerr);
            std::cerr << "\n";
        }
    }
}

void addCallFrameInfo(VISAKernelImpl* kernel)
{
    std::cerr << "\n\n\n";

    if (kernel->getKernel()->fg.getIsStackCallFunc())
    {
        std::cerr << "Stack call function " << kernel->getKernel()->getName() << "\n";
    }
    else
    {
        std::cerr << "Kernel " << kernel->getKernel()->getName() << "\n";
    }
    std::cerr << "\n";

    emitSubRoutineInfo(kernel);

    emitBEFP(kernel);

    emitCallerSaveInfo(kernel);

    emitCalleeSaveInfo(kernel);

    dumpCFG(kernel);
}
#endif

LiveIntervalInfo* KernelDebugInfo::getLiveIntervalInfo(G4_Declare* dcl, bool createIfNULL)
{
    dcl = dcl->getRootDeclare();

    LiveIntervalInfo* lr = NULL;
    auto it = debugInfoLiveIntervalMap.find(dcl);
    if (it == debugInfoLiveIntervalMap.end())
    {
        if (createIfNULL)
        {
            lr = new (kernel->fg.mem) LiveIntervalInfo();
            debugInfoLiveIntervalMap.insert(std::make_pair(dcl, lr));
        }
    }
    else
    {
        lr = it->second;
    }

    return lr;
}

// TODO: Check result in presence of spill code and stack calling convention
