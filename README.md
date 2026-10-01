# DLLParty

DLLParty is a proof-of-concept Windows DLL-injection technique that manipulates internally constructed thread-pool callback-instance storage to invoke `LoadLibraryA` directly from a worker thread—without custom callback code or shellcode.

The project also demonstrates how callback-instance addresses can be recovered through the internal `TP_POOL` worker list.

Read the [DLLParty research blog](https://medium.com/@persic.eno/dllparty-abusing-thread-pool-internals-for-shellcodeless-dll-injection-7c84106f7745?postPublishedType=repub).
> **Note:** This research relies on undocumented, version-specific Windows internals and is intended for educational and defensive security research only.
