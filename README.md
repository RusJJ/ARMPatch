### An example of usage
```sh
...
DECL_HOOKv(LibFuncAtABC123) // "DECL_HOOK(void,..." is replaced by "DECL_HOOKv(..."
{
	__android_log_print(ANDROID_LOG_INFO, "HOOKED!", "HOOKED()");
	LibFuncAtABC123(); // Call original function (thanks Cydia and Rprop!)
}

// CApplication* self because that function is a non-static class method
DECL_HOOK(void, OnApplicationStart, CApplication* self) // Be aware that pointer may be 8 bytes long in 64BIT app!
{
	__android_log_print(ANDROID_LOG_INFO, "HOOKED!", "Application::Start");
	OnApplicationStart(self); // Call original function (only if exists!!!). Dont try to self->Start() otherwise it will loop FOREVER
}

jint JNI_OnLoad(JavaVM* vm, void* reserved)
{
	uintptr_t lib = ARMPatch::getLib("libMYGAME.so");
	//HOOK(LibFuncAtABC123, dlsym((void*)lib, "funcAtAddress")); // You can do the thing below if you know an address
	HOOK(LibFuncAtABC123, lib + 0xABC123);
	HOOK(OnApplicationStart, CApplication::Start); // You can link a SO library using LDLIBS += -lMYAPP and compiler will import it!
	return JNI_VERSION_1_6;
}
...
```