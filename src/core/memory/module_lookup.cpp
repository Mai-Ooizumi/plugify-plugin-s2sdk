#include "module_lookup.hpp"
#include "os.h"
#if PLATFORM_WINDOWS
#include <filesystem>
#include <tier0/platform.h>
#endif

ModuleHandle ModuleLookup::FindModule(std::string_view name) {
#if PLATFORM_WINDOWS
	if (name == "server.dll") {
		const auto path = std::filesystem::absolute(std::filesystem::path(Plat_GetGameDirectory()) / S2SDK_GAME_NAME / "bin" / "win64" / "server.dll");
		return static_cast<ModuleHandle>(GetModuleHandleW(path.c_str()));
	}
	HMODULE h = GetModuleHandleA(std::string(name).c_str());
	return static_cast<ModuleHandle>(h);
#else
	void* h = dlopen(name.data(), RTLD_NOLOAD | RTLD_NOW);
	return h;
#endif
}

SymbolPtr ModuleLookup::FindSymbol(ModuleHandle module, std::string_view symbol) {
	if (!module) return nullptr;

#if PLATFORM_WINDOWS
	FARPROC proc = GetProcAddress(static_cast<HMODULE>(module), symbol.data());
	return reinterpret_cast<SymbolPtr>(proc);
#else
	dlerror();
	void* sym = dlsym(module, symbol.data());
	const char* err = dlerror();
	if (err != nullptr) return nullptr;
	return sym;
#endif
}

SymbolPtr ModuleLookup::FindSymbolFromModule(std::string_view module, std::string_view symbol) {
	ModuleHandle mod = FindModule(module);
	if (!mod) return nullptr;
	SymbolPtr sym = FindSymbol(mod, symbol);
#if !PLATFORM_WINDOWS
	dlclose(mod);
#endif
	return sym;
}
