#include "FsMinifilterHooking.h"


PCALLBACK_NODE getCachedCallbackNodesByInstance(PFLT_INSTANCE instance)
{
    for (size_t instanceIndex = 0; instanceIndex < originalInstances->getSize(); instanceIndex++)
    {
        if (instance == (*originalInstances)[instanceIndex]->get())
        {
            return (*originalInstances)[instanceIndex]->getCachedCallbackNodes();
        }
    }

    return nullptr;
}

bool isCallbackNode(
    PCALLBACK_NODE potentialCallbackNode,
    PFLT_INSTANCE pFltInstance,
    void* preCallback)
{
    return ((potentialCallbackNode->PreOperation == preCallback) &&
            (potentialCallbackNode->Instance == pFltInstance));
}

unsigned short getOffsetOfCallbackNodes(
    FltFilterGuard& filter,
    unsigned short limit,
    void * preCallbackFunc,
    unsigned short callbackIndex)
{
    void* potentialPointer;
    unsigned short offset = FltInstanceGuard::INVALID_OFFSET;
    ArrayGuard<FltInstanceGuard*, true> instancesArray;
    unsigned long fltInstancesNumber = filter.getInstances(instancesArray);

    if (fltInstancesNumber == 0)
    {
        return offset;
    }

    FltVolumeGuard volume(*instancesArray[0]);

    for (unsigned short i = 0; i < limit / sizeof(unsigned short); i++)
    {
        potentialPointer = reinterpret_cast<void*>(*(reinterpret_cast<PULONG_PTR>(
            reinterpret_cast<unsigned short*>(instancesArray[0]->get()) + i)));

        if (MmIsAddressValid(potentialPointer) &&
            (potentialPointer != filter.get()) &&
            (potentialPointer != volume.get()) &&
            (isCallbackNode(
                reinterpret_cast<PCALLBACK_NODE>(potentialPointer),
                instancesArray[0]->get(),
                preCallbackFunc)))
        {
            offset = i * sizeof(unsigned short) - callbackIndex * sizeof(PCALLBACK_NODE);
            break;
        }
    }

    return offset;
}

FLT_PREOP_CALLBACK_STATUS dummyCreatePreOperation(PFLT_CALLBACK_DATA Data,
    PCFLT_RELATED_OBJECTS FltObjects,
    PVOID* CompletionContext)
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(FltObjects);
    UNREFERENCED_PARAMETER(CompletionContext);

    return FLT_PREOP_SUCCESS_NO_CALLBACK;
}

FLT_PREOP_CALLBACK_STATUS hookPreOperationFunction(
    PFLT_CALLBACK_DATA Data,
    PCFLT_RELATED_OBJECTS FltObjects,
    PVOID* CompletionContext)
{
    UNREFERENCED_PARAMETER(Data);
    UNREFERENCED_PARAMETER(CompletionContext);

    PCALLBACK_NODE originalCallbackNodes = getCachedCallbackNodesByInstance(FltObjects->Instance);

    if (originalCallbackNodes == nullptr)
    {
        return FLT_PREOP_SUCCESS_NO_CALLBACK;
    }

    auto originalPreCallback = reinterpret_cast<PreCallbackProt>(
        originalCallbackNodes[static_cast<INT8>(Data->Iopb->MajorFunction) + 0x16].PreOperation);

    return originalPreCallback(Data, FltObjects, CompletionContext);
}

FLT_POSTOP_CALLBACK_STATUS hookPostOperationFunction(
    PFLT_CALLBACK_DATA Data,
    PCFLT_RELATED_OBJECTS FltObjects,
    PVOID CompletionContext,
    FLT_POST_OPERATION_FLAGS Flags)
{
    UNREFERENCED_PARAMETER(CompletionContext);
    UNREFERENCED_PARAMETER(Flags);

    PCALLBACK_NODE originalCallbackNodes = getCachedCallbackNodesByInstance(FltObjects->Instance);

    if (originalCallbackNodes == nullptr)
    {
        return FLT_POSTOP_FINISHED_PROCESSING;
    }

    auto originalPostCallback = reinterpret_cast<PostCallbackProt>(
        originalCallbackNodes[static_cast<INT8>(Data->Iopb->MajorFunction) + 0x16].PostOperation);

    return originalPostCallback(Data, FltObjects, CompletionContext, Flags);
}

void hookFunction(PVOID* pSourceFunction, PVOID hookFunction)
{
    if (MmIsAddressValid(*pSourceFunction))
    {
        InterlockedExchangePointer(pSourceFunction, hookFunction);
    }
}

VOID HookCallbacks(FltFilterGuard& filter)
{
    ArrayGuard<FltInstanceGuard*, true> instancesArray;
    ULONG instancesCount = filter.getInstances(instancesArray);

    if (instancesCount == 0)
    {
        return;
    }

    originalInstances = new ArrayGuard<FltInstanceGuard*, true>();
    originalInstances->allocate(NonPagedPool, instancesCount);

    for (size_t instanceIndex = 0; instanceIndex < instancesCount; instanceIndex++)
    {
        (*originalInstances)[instanceIndex] =
            new FltInstanceGuard(instancesArray[instanceIndex]->get(), false, false, true);
        (*originalInstances)[instanceIndex]->cachingCallbackNodes();

        PCALLBACK_NODE* callbackNodesList =
            (*originalInstances)[instanceIndex]->getPointerToCallbackNodesField();

        for (size_t callbackIndex = 0X16;
             callbackIndex <= IRP_MJ_MAXIMUM_FUNCTION + 0X16;
             callbackIndex++)
        {
            PCALLBACK_NODE callbackNode = callbackNodesList[callbackIndex];

            if (MmIsAddressValid(callbackNode))
            {
                hookFunction(&(callbackNode->PreOperation), &hookPreOperationFunction);
                hookFunction(&(callbackNode->PostOperation), &hookPostOperationFunction);
            }
        }
    }
}

VOID UnHookCallbacks()
{
    if (originalInstances == nullptr)
    {
        return;
    }

    for (size_t instanceIndex = 0;
         instanceIndex < originalInstances->getSize();
         instanceIndex++)
    {
        FltInstanceGuard instance((*originalInstances)[instanceIndex]->get(), true, true, false);

        if (!instance.isValid())
        {
            // The instance is tear down
            continue;
        }

        PCALLBACK_NODE* callbackNodesList = instance.getPointerToCallbackNodesField();
        auto cachedCallbackNodes = (*originalInstances)[instanceIndex]->getCachedCallbackNodes();

        for (size_t callbackIndex = 0x16;
             callbackIndex <= IRP_MJ_MAXIMUM_FUNCTION + 0x16;
             callbackIndex++)
        {
            PCALLBACK_NODE callbackNode = callbackNodesList[callbackIndex];

            if (MmIsAddressValid(callbackNode))
            {
                hookFunction(
                    &(callbackNode->PreOperation),
                    cachedCallbackNodes[callbackIndex].PreOperation);

                hookFunction(
                    &(callbackNode->PostOperation),
                    cachedCallbackNodes[callbackIndex].PostOperation);
            }

        }
    }

    delete originalInstances;
}

VOID Unload(PDRIVER_OBJECT  DriverObject)
{
    UNREFERENCED_PARAMETER(DriverObject);

    DbgPrint("Unload Driver \r\n");

    UnHookCallbacks();
}

extern "C" NTSTATUS DriverEntry(_In_ PDRIVER_OBJECT DriverObject, _In_ PUNICODE_STRING RegistryPath)
{
    UNREFERENCED_PARAMETER(RegistryPath);

    NTSTATUS status = STATUS_SUCCESS;

    DbgPrint("DriverEntry Called \r\n");

    DriverObject->DriverUnload = Unload;

    FltFilterGuard dummyFilter(DriverObject, FilterRegistration);
    dummyFilter.startFiltering();

    unsigned short offsetCallbackNodes = getOffsetOfCallbackNodes(
        dummyFilter,
        0x230,
        dummyCreatePreOperation,
        IRP_MJ_CREATE + 0X16);

    dummyFilter.~FltFilterGuard();

    if (offsetCallbackNodes != FltInstanceGuard::INVALID_OFFSET)
    {
        FltInstanceGuard::setOffsetCallbackNodes(offsetCallbackNodes);

        const UNICODE_STRING filterName = RTL_CONSTANT_STRING(L"luafv");

        FltFilterGuard filter(&filterName);

        if (filter.isValid())
        {
            HookCallbacks(filter);
        }
    }

    return status;
}