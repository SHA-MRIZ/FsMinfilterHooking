#pragma once
#include <ntddk.h>

PVOID getPrivateFunctionByPattern(PUCHAR startAddress, PUCHAR pattern, int patternSize, int limitFunctionOffset)
{
    int index = 0;
    PUCHAR func = nullptr;

    do
    {
        if (!memcmp(startAddress + index, pattern, patternSize))
        {
            signed int callcode = 0;

            RtlMoveMemory(&callcode, (PVOID)(startAddress + index + patternSize), 4);

            // func  = callAddress + 5 + offset (index point to callAddress +1)
            func = callcode + startAddress + index + patternSize + 4;

            if (!MmIsAddressValid(func))
                func = nullptr;
            
            break;
        }

    } while (++index != limitFunctionOffset - patternSize - 4);

    return func;
}

typedef PCALLBACK_NODE(*FltpGetCallbackNodeForInstanceDef)(PFLT_INSTANCE fltInstance, unsigned int index);
FltpGetCallbackNodeForInstanceDef FltpGetCallbackNodeForInstance;

FltpGetCallbackNodeForInstanceDef getFltpGetCallbackNodeForInstance()
{
    UCHAR pattern[5] = { 0x10, 0x41, 0x8B, 0xD4, 0xE8 };
    PVOID pCheckArea = FltGetRoutineAddress("FltGetDestinationFileNameInformation");
    return (FltpGetCallbackNodeForInstanceDef)getPrivateFunctionByPattern((PUCHAR)pCheckArea, pattern, sizeof(pattern), 0x20000);
}
