#pragma once

#include <fltKernel.h>

extern "C" DRIVER_INITIALIZE DriverEntry; 

extern "C" NTSTATUS
DriverEntry(
    _In_ PDRIVER_OBJECT DriverObject,
    _In_ PUNICODE_STRING RegistryPath
);

#pragma alloc_text(INIT, DriverEntry)

typedef struct _CALLBACK_NODE
{
    LIST_ENTRY CallbackLinks;
    PFLT_INSTANCE Instance;
    PVOID PreOperation;
    PVOID PostOperation;
    LONG Flags;
} CALLBACK_NODE, *PCALLBACK_NODE;

typedef FLT_PREOP_CALLBACK_STATUS(*PreCallbackProt)(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _Flt_CompletionContext_Outptr_ PVOID* CompletionContext);

typedef FLT_POSTOP_CALLBACK_STATUS
(*PostCallbackProt)(
    _Inout_ PFLT_CALLBACK_DATA Data,
    _In_ PCFLT_RELATED_OBJECTS FltObjects,
    _In_opt_ PVOID CompletionContext,
    _In_ FLT_POST_OPERATION_FLAGS Flags);

typedef struct _INSTANCE_CALLBACK_NODE
{
    PFLT_INSTANCE pFltInstance;
    PFLT_FILTER pFltFilter;
    CALLBACK_NODE callbackNodes[50] = { 0 };
}INSTANCE_CALLBACK_NODE, *PINSTANCE_CALLBACK_NODE;

PINSTANCE_CALLBACK_NODE listInstanceWithCallbackNodes;
ULONG instancesNumber = 0;