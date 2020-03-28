#include "FsMinifilterHooking.h"
#include "PrivateFunctions.h"

PCALLBACK_NODE getCallbackNodeListByInstance(PFLT_INSTANCE instance)
{
    for (unsigned int i = 0; i < instancesNumber; i++)
    {
        if (instance == listInstanceWithCallbackNodes[i].pFltInstance)
            return listInstanceWithCallbackNodes[i].callbackNodes;
    }

    return nullptr;
}

FLT_PREOP_CALLBACK_STATUS HookPreOperationFunction(PFLT_CALLBACK_DATA Data,
    PCFLT_RELATED_OBJECTS FltObjects,
    PVOID* CompletionContext)
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);

    PCALLBACK_NODE pCallbackNode = getCallbackNodeListByInstance(FltObjects->Instance);

    if (pCallbackNode == nullptr)
        return FLT_PREOP_SUCCESS_NO_CALLBACK;

    return ((PreCallbackProt)(pCallbackNode[(INT8)Data->Iopb->MajorFunction + 0x16].PreOperation))
                                            (Data, FltObjects, CompletionContext);
}

FLT_POSTOP_CALLBACK_STATUS HookPostOperationFunction(PFLT_CALLBACK_DATA Data,
    PCFLT_RELATED_OBJECTS FltObjects,
    PVOID CompletionContext,
    FLT_POST_OPERATION_FLAGS Flags)
{
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);

    PCALLBACK_NODE pCallbackNode = getCallbackNodeListByInstance(FltObjects->Instance);

    if (pCallbackNode == nullptr)
        return FLT_POSTOP_FINISHED_PROCESSING;

    return ((PostCallbackProt)(pCallbackNode[(INT8)Data->Iopb->MajorFunction + 0x16].PostOperation))
                                             (Data, FltObjects, CompletionContext, Flags);
}

void hookFunction(PVOID* pSourceFunction, PVOID hookFunction)
{
    if (MmIsAddressValid(*pSourceFunction))
    {
        InterlockedExchangePointer((PVOID*)(pSourceFunction), hookFunction);
    }
}

PFLT_INSTANCE* getInstancesForFilter(PFLT_FILTER pFilter, PULONG retInstancesNumber)
{
    PFLT_INSTANCE* instancesList = nullptr;
    ULONG instancesListSize = 0;
    NTSTATUS status = FltEnumerateInstances(NULL, pFilter, NULL, 0, retInstancesNumber);

    if ((status == STATUS_BUFFER_TOO_SMALL) && *retInstancesNumber)
    {
        instancesListSize = sizeof(PFLT_INSTANCE) * (*retInstancesNumber) * 2;
        instancesList = (PFLT_INSTANCE*)ExAllocatePool(NonPagedPool, instancesListSize);

        if (instancesList)
            if (NT_SUCCESS(FltEnumerateInstances(0, pFilter, instancesList, instancesListSize, retInstancesNumber)))
                return instancesList;
            else
                ExFreePool(instancesList);
    }

    return nullptr;
}


VOID HookCallbacks(PFLT_FILTER pFilter)
{
    PFLT_INSTANCE* instancesList = getInstancesForFilter(pFilter, &instancesNumber);

    if (!instancesList)
        return;

    listInstanceWithCallbackNodes = (PINSTANCE_CALLBACK_NODE)ExAllocatePool(NonPagedPool, sizeof(INSTANCE_CALLBACK_NODE) * instancesNumber);

    if (!listInstanceWithCallbackNodes)
        goto Cleanup;

    RtlZeroMemory(listInstanceWithCallbackNodes, sizeof(INSTANCE_CALLBACK_NODE) * instancesNumber);

    FltpGetCallbackNodeForInstance = getFltpGetCallbackNodeForInstance();

    if (FltpGetCallbackNodeForInstance == nullptr)
        goto Cleanup;

    for (unsigned int i = 0; i < instancesNumber; i++)
    {
        listInstanceWithCallbackNodes[i].pFltInstance = instancesList[i];
        listInstanceWithCallbackNodes[i].pFltFilter = pFilter;

        DbgPrint("PFLT_FILTER: %x, Instances: %x \n", pFilter, instancesList[i]);

        for (int j = 0; j < 50; j++)
        {
            PCALLBACK_NODE pCallbackNode = FltpGetCallbackNodeForInstance(instancesList[i], j);

            if (MmIsAddressValid(pCallbackNode))
            {
                listInstanceWithCallbackNodes[i].callbackNodes[j] = *pCallbackNode;

                hookFunction(&(pCallbackNode->PreOperation), &HookPreOperationFunction);

                hookFunction(&(pCallbackNode->PostOperation), &HookPostOperationFunction);
            }
        }
    }

Cleanup:
    if (instancesList)
    {
        //
        //  Release all the objects in the array
        //
        for (unsigned int i = 0; i < instancesNumber; i++)
        {
            FltObjectDereference(instancesList[i]);
            instancesList[i] = NULL;
        }

        ExFreePool(instancesList);
        instancesList = NULL;
    }
}

VOID UnHookCallbacks()
{
    if ((!FltpGetCallbackNodeForInstance) || (instancesNumber <= 0) || (!listInstanceWithCallbackNodes))
        return;

    for (unsigned int i = 0; i < instancesNumber; i++)
    {
        for (unsigned int j = 0; j < 50; j++)
        {
            PCALLBACK_NODE pCallbackNode = 
                FltpGetCallbackNodeForInstance(listInstanceWithCallbackNodes[i].pFltInstance, j);

            if (MmIsAddressValid(pCallbackNode))
            {
                hookFunction(&(pCallbackNode->PreOperation),
                    listInstanceWithCallbackNodes[i].callbackNodes[j].PreOperation);

                hookFunction(&(pCallbackNode->PostOperation),
                    listInstanceWithCallbackNodes[i].callbackNodes[j].PostOperation);
            }

        }
    }
}

VOID Unload(PDRIVER_OBJECT  DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);

    DbgPrint("Unload Driver \r\n");

    UnHookCallbacks();
}

extern "C" NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
    NTSTATUS status;
    UNICODE_STRING name;

    UNREFERENCED_PARAMETER(RegistryPath);

    DbgPrint("DriverEntry Called \r\n");

    DriverObject->DriverUnload = Unload;

    PFLT_FILTER pFilter;
    RtlInitUnicodeString(&name, L"wdfilter");

    if (NT_SUCCESS(status = FltGetFilterFromName(&name, &pFilter)))
    {
        HookCallbacks(pFilter);
    }
    else
    {
        DbgPrint("FltGetFilterFromName Failed: error - %d", status);
    }

    return status;
}