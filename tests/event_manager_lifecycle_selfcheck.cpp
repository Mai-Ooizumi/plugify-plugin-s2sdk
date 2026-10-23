#include <algorithm>
#include <array>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <format>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <ranges>
#include <shared_mutex>
#include <span>
#include <stack>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

#include "stl.hpp"
#include "core/event_hook.hpp"

#include <cstdlib>
#include <iostream>
#include <map>

namespace {
int preOneCalls = 0;
int preTwoCalls = 0;
int postCalls = 0;

#define CHECK(condition) \
	do { \
		if (!(condition)) { \
			std::cerr << "event lifecycle self-check failed at line " << __LINE__ << ": " << #condition << '\n'; \
			return EXIT_FAILURE; \
		} \
	} while (false)

ResultType PreOne(const plg::string&, IGameEvent*, bool) {
	++preOneCalls;
	return ResultType::Continue;
}
ResultType PreTwo(const plg::string&, IGameEvent*, bool) {
	++preTwoCalls;
	return ResultType::Continue;
}
ResultType PostOne(const plg::string&, IGameEvent*, bool) {
	++postCalls;
	return ResultType::Continue;
}
ResultType NotRegistered(const plg::string&, IGameEvent*, bool) { return ResultType::Continue; }
} // namespace

int main() {
	EventHook hook;
	hook.name = "player_death";
	CHECK(hook.callbacks[HookMode::Pre].Register(PreOne));
	CHECK(hook.callbacks[HookMode::Pre].Register(PreTwo));
	CHECK(hook.callbacks[HookMode::Post].Register(PostOne));

	CHECK(hook.RemoveCallback(PreOne, HookMode::Pre) == EventHookUnregisterResult::Retained);
	CHECK(hook.refCount == 1 && !hook.callbacks[HookMode::Post].Empty());
	auto postListeners = hook.callbacks[HookMode::Post].Get();
	CHECK(postListeners && postListeners->handlers.size() == 1);
	CHECK(postListeners->handlers.front()(hook.name, nullptr, false) == ResultType::Continue);
	CHECK(postCalls == 1);
	CHECK(hook.RemoveCallback(NotRegistered, HookMode::Pre) == EventHookUnregisterResult::NotRegistered);
	CHECK(hook.refCount == 1 && !hook.callbacks[HookMode::Post].Empty());
	CHECK(hook.RemoveCallback(PostOne, HookMode::Post) == EventHookUnregisterResult::Retained);
	CHECK(!hook.callbacks[HookMode::Pre].Empty());
	CHECK(hook.RemoveCallback(PreTwo, HookMode::Pre) == EventHookUnregisterResult::Empty);
	CHECK(hook.refCount == 1);

	// A live dispatch keeps its original hook alive after the map drops it.
	// Finishing that frame must not erase a newly registered same-name hook.
	auto active = std::make_shared<EventHook>();
	active->name = "round_start";
	CHECK(active->callbacks[HookMode::Post].Register(PostOne));
	++active->refCount;
	std::map<plg::string, std::shared_ptr<EventHook>, std::less<>> hooks;
	hooks.emplace(active->name, active);
	CHECK(active->RemoveCallback(PostOne, HookMode::Post) == EventHookUnregisterResult::Empty);
	CHECK(active->refCount == 2);
	hooks.erase(active->name);
	auto replacement = std::make_shared<EventHook>();
	replacement->name = active->name;
	CHECK(replacement->callbacks[HookMode::Pre].Register(PreOne));
	hooks.emplace(replacement->name, replacement);
	if (active->FinishDispatch()) {
		hooks.erase(active->name);
	}
	CHECK(active->refCount == 1);
	CHECK(hooks.find(replacement->name) != hooks.end());
	CHECK(hooks.find(replacement->name)->second == replacement);

	std::cout << "PASS: event hook mode, callback and in-flight ownership lifecycle\n";
	return EXIT_SUCCESS;
}
