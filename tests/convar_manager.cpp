#include <algorithm>
#include <expected>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <string>
#include <vector>

#include "stl.hpp"
#include "core/logger.hpp"
#include "core/con_var_manager.hpp"

#include "common/assert.h"
#include "common/macros.h"
#include <tier0/interface.h>

Logger Logger::instance("convar_manager_tests");
Logger::Logger(const char*, RegisterTagsFunc, int, LoggingVerbosity_t, const Color&) {}
void Logger::Log(std::string_view, LoggingSeverity_t, const std::source_location&) const {}

REGISTER_NAMED_TEST("ConVarManager.LookupMissDoesNotPoisonCreation", ConVarManager_LookupMissDoesNotPoisonCreation)
{
	auto module = Plat_LoadModule("tier0");
	TEST_NOT_NULL(module);
	auto factory = Plat_GetModuleInterfaceFactory(module);
	TEST_NOT_NULL(factory);
	auto* cvar = static_cast<ICvar*>(factory(CVAR_INTERFACE_VERSION, nullptr));
	TEST_NOT_NULL(cvar);
	TEST_TRUE(cvar->Connect(factory) && cvar->Init() == INIT_OK);
	g_pCVar = cvar;
	struct TestRegistration {
		~TestRegistration() {
			ConVarManager::Terminate();
			g_pCVar = nullptr;
		}
	} registration;

	auto& vars = ConVarManager::Instance();
	constexpr std::string_view name{"test_s2sdk_manager_find_trailing"};
	constexpr auto key = name.substr(0, name.size() - std::string_view("_trailing").size());
	constexpr std::string_view description{"manager test help trailing"};
	constexpr auto help = description.substr(0, description.size() - std::string_view(" trailing").size());
	TEST_FALSE(vars.CreateConVar<int32>(key, help, 17, ConVarFlag::Release).IsValidRef());
	ConVarManager::Init();

	TEST_FALSE(vars.FindConVar(key).IsValidRef());
	TEST_FALSE(vars.FindConVar<int32>(key).IsValidRef());
	TEST_FALSE(cvar->FindConVar("test_s2sdk_manager_find").IsValidRef());
	auto created = vars.CreateConVar<int32>(key, help, 17, ConVarFlag::Release);
	TEST_TRUE(created.IsValidRef());
	ConVarRefAbstract ref(created);
	TEST_TRUE(ref.GetName() == key);
	TEST_TRUE(ref.GetHelpText() == help);
	TEST_FALSE(vars.FindConVar<bool>(key).IsValidRef());
	TEST_EQ(vars.FindConVar<int32>(key).GetAccessIndex(), created.GetAccessIndex());

	CConVar<int32> external("test_s2sdk_manager_external", FCVAR_RELEASE, "", 41);
	constexpr std::string_view externalName{"test_s2sdk_manager_external!"};
	constexpr auto externalKey = externalName.substr(0, externalName.size() - 1);
	TEST_FALSE(vars.FindConVar<bool>(externalKey).IsValidRef());
	TEST_EQ(vars.FindConVar(externalKey).GetAccessIndex(), external.GetAccessIndex());
}
