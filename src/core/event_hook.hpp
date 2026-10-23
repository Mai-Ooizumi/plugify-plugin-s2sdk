#pragma once

#include "listener_manager.hpp"
#include <igameevents.h>

using EventListenerCallback = ResultType (*)(const plg::string& name, IGameEvent* event, bool dontBroadcast);
inline char EventListenerStr[] = S2SDK_PACKAGE "::EventListener";

enum class EventHookUnregisterResult {
	NotRegistered,
	Retained,
	Empty,
};

struct EventHook {
	plg::string name;
	bool postCopy{};
	uint32_t refCount{1};
	plg::enum_array<ListenerManager<EventListenerStr, EventListenerCallback>, HookMode> callbacks;

	EventHookUnregisterResult RemoveCallback(EventListenerCallback callback, HookMode mode) {
		if (!callbacks[mode].Unregister(callback)) {
			return EventHookUnregisterResult::NotRegistered;
		}
		return callbacks[HookMode::Pre].Empty() && callbacks[HookMode::Post].Empty()
			   ? EventHookUnregisterResult::Empty
			   : EventHookUnregisterResult::Retained;
	}

	bool FinishDispatch() { return --refCount == 0; }
};
