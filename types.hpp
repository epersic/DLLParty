#include<windows.h>
#include<winternl.h>
#ifndef STATUS_INFO_LENGTH_MISMATCH
#define  STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004)
#endif
#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS ((NTSTATUS)0x00000000L)
#endif

#ifndef STATUS_INFO_LENGTH_MISMATCH
#define STATUS_INFO_LENGTH_MISMATCH ((NTSTATUS)0xC0000004L)
#endif

typedef enum _WORKERFACTORYINFOCLASS
{
    WorkerFactoryTimeout,               // qs: LARGE_INTEGER
    WorkerFactoryRetryTimeout,          // qs: LARGE_INTEGER
    WorkerFactoryIdleTimeout,           // qs: LARGE_INTEGER
    WorkerFactoryBindingCount,          // qs: ULONG
    WorkerFactoryThreadMinimum,         // qs: ULONG
    WorkerFactoryThreadMaximum,         // qs: ULONG
    WorkerFactoryPaused,                // qs: ULONG or BOOLEAN
    WorkerFactoryBasicInformation,      // q: WORKER_FACTORY_BASIC_INFORMATION
    WorkerFactoryAdjustThreadGoal,      // s: ULONG
    WorkerFactoryCallbackType,          // qs: ULONG
    WorkerFactoryStackInformation,      // qs: ULONG/ULONG_PTR // 10
    WorkerFactoryThreadBasePriority,    // qs: ULONG
    WorkerFactoryTimeoutWaiters,        // qs: ULONG // since THRESHOLD
    WorkerFactoryFlags,                 // qs: ULONG
    WorkerFactoryThreadSoftMaximum,     // qs: ULONG
    WorkerFactoryThreadCpuSets,         // qs: ULONG[] // since REDSTONE5
    MaxWorkerFactoryInfoClass
} WORKERFACTORYINFOCLASS, *PWORKERFACTORYINFOCLASS;

typedef NTSTATUS (NTAPI *NtQueryInformationWorkerFactory_t)(
  HANDLE                     WorkerFactoryHandle,
  WORKERFACTORYINFOCLASS     WorkerFactoryInformationClass,
  PVOID                      WorkerFactoryInformation,
  ULONG                      WorkerFactoryInformationLength,
  PULONG                     ReturnLength
);

typedef struct _WORKER_FACTORY_BASIC_INFORMATION
{
    LARGE_INTEGER Timeout;
    LARGE_INTEGER RetryTimeout;
    LARGE_INTEGER IdleTimeout;
    BOOLEAN Paused;
    BOOLEAN TimerSet;
    BOOLEAN QueuedToExWorker;
    BOOLEAN MayCreate;
    BOOLEAN CreateInProgress;
    BOOLEAN InsertedIntoQueue;
    BOOLEAN Shutdown;
    ULONG BindingCount;
    ULONG ThreadMinimum;
    ULONG ThreadMaximum;
    ULONG PendingWorkerCount;
    ULONG WaitingWorkerCount;
    ULONG TotalWorkerCount;
    ULONG ReleaseCount;
    LONGLONG InfiniteWaitGoal;
    PVOID StartRoutine;
    PVOID StartParameter;
    HANDLE ProcessId;
    SIZE_T StackReserve;
    SIZE_T StackCommit;
    NTSTATUS LastThreadCreationStatus;
} WORKER_FACTORY_BASIC_INFORMATION, *PWORKER_FACTORY_BASIC_INFORMATION;


typedef struct _OBJECT_TYPE_INFORMATION_ENTRY {
    UNICODE_STRING TypeName;
    ULONG TotalNumberOfObjects;
    ULONG TotalNumberOfHandles;
    ULONG TotalPagedPoolUsage;
    ULONG TotalNonPagedPoolUsage;
    ULONG TotalNamePoolUsage;
    ULONG TotalHandleTableUsage;
    ULONG HighWaterNumberOfObjects;
    ULONG HighWaterNumberOfHandles;
    ULONG HighWaterPagedPoolUsage;
    ULONG HighWaterNonPagedPoolUsage;
    ULONG HighWaterNamePoolUsage;
    ULONG HighWaterHandleTableUsage;
    ULONG InvalidAttributes;
    GENERIC_MAPPING GenericMapping;
    ULONG ValidAccessMask;
    BOOLEAN SecurityRequired;
    BOOLEAN MaintainHandleCount;
    UCHAR TypeIndex;
    CHAR ReservedByte;
    ULONG PoolType;
    ULONG DefaultPagedPoolCharge;
    ULONG DefaultNonPagedPoolCharge;
} OBJECT_TYPE_INFORMATION_ENTRY, *POBJECT_TYPE_INFORMATION_ENTRY;

typedef struct _OBJECT_TYPES_INFORMATION {
    ULONG NumberOfTypes;
} OBJECT_TYPES_INFORMATION, *POBJECT_TYPES_INFORMATION;

// Prototype for NtQueryObject using winternl.h's OBJECT_INFORMATION_CLASS
typedef NTSTATUS(NTAPI* pfnNtQueryObject)(
    HANDLE Handle,
    OBJECT_INFORMATION_CLASS ObjectInformationClass,
    PVOID ObjectInformation,
    ULONG ObjectInformationLength,
    PULONG ReturnLength
);

// Explicit declaration of RtlEqualUnicodeString if winternl.h didn't declare it
NTSYSAPI BOOLEAN NTAPI RtlEqualUnicodeString(
    PCUNICODE_STRING String1,
    PCUNICODE_STRING String2,
    BOOLEAN CaseInSensitive
);

typedef struct _TPP_REFCOUNT {
    volatile LONG Refcount;
} TPP_REFCOUNT;

typedef union _TPP_POOL_QUEUE_STATE {
    LONGLONG Exchange;

    struct {
        LONG RunningThreadGoal   : 16;
        ULONG PendingReleaseCount : 16;
        ULONG QueueLength;
    };
} TPP_POOL_QUEUE_STATE;

typedef struct _TPP_QUEUE TPP_QUEUE;
typedef struct _TPP_NUMA_NODE TPP_NUMA_NODE;

typedef struct _FULL_TP_POOL_PREFIX {
    TPP_REFCOUNT Refcount;              // 0x00
    LONG Padding;                       // 0x04
    TPP_POOL_QUEUE_STATE QueueState;    // 0x08
    TPP_QUEUE *TaskQueue[3];            // 0x10
    TPP_NUMA_NODE *NumaNode;            // 0x28
    GROUP_AFFINITY *ProximityInfo;      // 0x30
    PVOID WorkerFactory;                // 0x38
    PVOID CompletionPort;               // 0x40
    SRWLOCK WorkerListLock;              // 0x48
    BYTE Unknown50[0x10];               // 0x50
    LIST_ENTRY WorkerListHead;           // 0x60
} FULL_TP_POOL_PREFIX;


typedef struct _TP_TASK_CALLBACKS
{
    PVOID ExecuteCallback;
    PVOID Unposted;
} TP_TASK_CALLBACKS, *PTP_TASK_CALLBACKS;

typedef struct _TP_TASK
{
    PTP_TASK_CALLBACKS Callbacks;
    UINT32 NumaNode;
    UINT8 IdealProcessor;
    UINT8 Padding[3];
    LIST_ENTRY ListEntry;
} TP_TASK, *PTP_TASK;

typedef struct _TP_DIRECT
{
    struct _TP_TASK Task;
    UINT64 Lock;
    struct _LIST_ENTRY IoCompletionInformationList;
    void* Callback;
    UINT32 NumaNode;
    UINT8 IdealProcessor;
    char __PADDING__[3];
} TP_DIRECT, * PTP_DIRECT; 