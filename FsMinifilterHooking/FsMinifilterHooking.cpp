#include "FsMinifilterHooking.h"

FLT_PREOP_CALLBACK_STATUS HookPreOperationFunction(PFLT_CALLBACK_DATA Data,
                                                   PCFLT_RELATED_OBJECTS FltObjects,
                                                   PVOID* CompletionContext)
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    return FLT_PREOP_SUCCESS_WITH_CALLBACK;
}

FLT_POSTOP_CALLBACK_STATUS HookPostOperationFunction(PFLT_CALLBACK_DATA Data,
                                                     PCFLT_RELATED_OBJECTS FltObjects,
                                                     PVOID CompletionContext,
                                                     FLT_POST_OPERATION_FLAGS Flags)
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);
    return FLT_POSTOP_FINISHED_PROCESSING;
}

VOID HookCallbacks(PFLT_FILTER pFilter)
{
    NTSTATUS status;
    PFLT_INSTANCE* instancesList = nullptr;
    ULONG instancesListSize = 0;
    ULONG instancesNumber = 0;
    status = FltEnumerateInstances(NULL, pFilter, NULL, 0, &instancesNumber);

    if ((status == STATUS_BUFFER_TOO_SMALL) && instancesNumber)
    {
        instancesListSize = sizeof(PFLT_INSTANCE) * instancesNumber * 2;
        instancesList = (PFLT_INSTANCE*)ExAllocatePool(NonPagedPool, instancesListSize);
        if (instancesList)
        {
            status = FltEnumerateInstances(0, pFilter, instancesList, instancesListSize, &instancesNumber);

            for (ULONG i = 0; i < instancesNumber; i++)
            {
                DbgPrint("PFLT_FILTER: %x, Instances: %x \n", pFilter, instancesList[i]);

                for (INT j = 0; j < 50; j++)
                {
                    PCALLBACK_NODE pCallbackNode = (PCALLBACK_NODE)(*((ULONG_PTR*)((ULONG_PTR)instancesList[i] + OffsetCallbackNode + 8 * j)));

                    if ((pCallbackNode != NULL) && (MmIsAddressValid(pCallbackNode)))
                    {
                        if (MmIsAddressValid(pCallbackNode->PreOperation))
                        {
                            InterlockedExchange((PLONG)(&(pCallbackNode->PreOperation)), (LONG)(((ULONG_PTR)(&HookPreOperationFunction))));
                        }

                        if (MmIsAddressValid(pCallbackNode->PostOperation))
                        {
                            InterlockedExchange((PLONG)(&(pCallbackNode->PostOperation)), (LONG)(((ULONG_PTR)(&HookPostOperationFunction))));
                        }

                    }
                }
                FltObjectDereference(instancesList[i]);
            }
        }
    }
}

VOID Unload(PDRIVER_OBJECT  DriverObject)
{

    UNICODE_STRING usDosDeviceName;

/*    UNICODE_STRING name;
    RtlInitUnicodeString(&name, L"PROCMON23");
    PFLT_FILTER flt = Get_Filter_Object(&name);
    DbgPrint("PreCallbackSaved: 0x%x", preCallbackSaved)*/;
    //HookCallbacks(flt, (PVOID)preCallbackSaved, (PVOID)postCallbackSaved, DriverObject);

    DbgPrint("Unload Driver \r\n");

    RtlInitUnicodeString(&usDosDeviceName, L"\\DosDevices\\MinifilterHooking");
    IoDeleteSymbolicLink(&usDosDeviceName);

    IoDeleteDevice(DriverObject->DeviceObject);
}

extern "C" NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
    NTSTATUS status;

    UNREFERENCED_PARAMETER(RegistryPath);

    PDEVICE_OBJECT pDeviceObject = NULL;
    UNICODE_STRING usDriverName, usDosDeviceName;

    DbgPrint("DriverEntry Called \r\n");

    RtlInitUnicodeString(&usDriverName, L"\\Device\\MinifilterHooking");

    status = IoCreateDevice(DriverObject, 0, &usDriverName, FILE_DEVICE_UNKNOWN, FILE_DEVICE_SECURE_OPEN, FALSE, &pDeviceObject);

    if (NT_SUCCESS(status))
    {
        pDeviceObject->Flags &= (~DO_DEVICE_INITIALIZING);

        RtlInitUnicodeString(&usDosDeviceName, L"\\DosDevices\\MinifilterHooking");

        status = IoCreateSymbolicLink(&usDosDeviceName, &usDriverName);
        if (NT_SUCCESS(status))
        {
            DbgPrint("IoCreateSymbolicLink success");
        }
        else
        {
            DbgPrint("IoCreateSymbolicLink failed: error - %d", status);
        }

        DriverObject->DriverUnload = Unload;

        UNICODE_STRING name;
        RtlInitUnicodeString(&name, L"PROCMON24");
        //RtlInitUnicodeString(&name, L"luafv");
        PFLT_FILTER flt;
        if (NT_SUCCESS(FltGetFilterFromName(&name, &flt)))
        {
            HookCallbacks(flt);
        }
        //FltpGetCallbackNodeForInstance((PFLT_INSTANCE)0x02, 5);
    }
    else
    {
        DbgPrint("IoCreateDevice Failed: error - %d", status);
    }

    return status;
}