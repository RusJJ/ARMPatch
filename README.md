### An example of usage
```sh
...
void (*orig)();
void hooked()
{
	__android_log_print(ANDROID_LOG_INFO, "HOOKED!", "HOOKED()");
	orig(); // Call original function (thanks Cydia!)
}

// DECLFN(fn name, returen type, args)
DECLFN(ApplicationStart, void, CApplication* self)
{
	__android_log_print(ANDROID_LOG_INFO, "HOOKED!", "Application::Start");
	CALLFN(ApplicationStart, self); // Call original function (only if exists!!!). Dont try to self->Start()
}

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
	uintptr_t lib = ARMPatch::getLib("libMYGAME.so");
	//ARMPatch::hook(dlsym((void*)lib, "FunctionSym"), &hooked, &orig);
	ARMPatch::hook(lib + 0xABC123, &hooked, &orig);
	ARMPatch::hook(&CApplication::Start, USEFN(ApplicationStart)); // You can link a SO library using for example: LDLIBS += -lMYAPP
	return JNI_VERSION_1_6;
}
...
```
