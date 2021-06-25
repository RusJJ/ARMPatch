### An example of usage
```sh
...
void (*orig)();
void hooked()
{
	__android_log_print(ANDROID_LOG_INFO, "HOOKED!", "HOOKED()");
	orig(); // Call original function (thanks Cydia!)
}
jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
	uintptr_t lib = ARMPatch::getLib("libMYGAME.so");
	//ARMPatch::hook((uintptr_t)dlsym((void*)lib, "FunctionSym"), (uintptr_t)&hooked, (uintptr_t)&orig);
	ARMPatch::hook(lib + 0xABC123, (uintptr_t)&hooked, (uintptr_t)&orig);
	return JNI_VERSION_1_6;
}
```