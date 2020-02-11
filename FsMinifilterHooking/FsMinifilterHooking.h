#pragma once
//#pragma comment(lib, "fltMgr.lib")

//#include "fltMgr.h"
#include <fltKernel.h>

ULONG OffsetCallbackNode = 0xa0;
ULONG OffsetPreOperation = 0x18;
ULONG OffsetPostOperation = 0x20;
ULONG offsetName = 0x38;
const ULONG POOL_TAG = 5070;

extern "C" DRIVER_INITIALIZE DriverEntry; 
extern "C" NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
);

#pragma alloc_text(INIT, DriverEntry)

enum CALLBACK_NODE_FLAGS
{
    CBNFL_SKIP_PAGING_IO = 1,
    CBNFL_SKIP_CACHED_IO = 2,
    CBNFL_USE_NAME_CALLBACK_EX = 4,
    CBNFL_SKIP_NON_DASD_IO = 8,
};

typedef struct _CALLBACK_NODE
{
    LIST_ENTRY CallbackLinks;
    PFLT_INSTANCE Instance;
    PVOID PreOperation;
    PVOID PostOperation;
    LONG GenerateFileName;
    LONG NormalizeNameComponent;
    LONG NormalizeNameComponentEx;
    PVOID NormalizeContextCleanup;
    int Flags;
} CALLBACK_NODE, *PCALLBACK_NODE;

typedef FLT_PREOP_CALLBACK_STATUS
PreCallbackProt(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext
);

typedef FLT_POSTOP_CALLBACK_STATUS
PostCallbackProt(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags
);

//extern "C" PCallbackNode FltpGetCallbackNodeForInstance(PFLT_INSTANCE fltInstance, unsigned int a2);