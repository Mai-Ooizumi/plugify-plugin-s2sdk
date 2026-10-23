#pragma once

#include "event_hook.hpp"

enum class EventHookError {
	Okay = 0,
	InvalidEvent,
	NotActive,
	InvalidCallback,
};

class EventManager : public IGameEventListener2 {
	EventManager() = default;
	~EventManager() override;
	NONCOPYABLE(EventManager)

	static EventManager instance;
public:
	static auto& Instance() noexcept {
		return instance;
	}

	EventHookError HookEvent(std::string_view name, EventListenerCallback callback, HookMode mode = HookMode::Post);
	EventHookError UnhookEvent(std::string_view name, EventListenerCallback callback, HookMode mode = HookMode::Post);
	void Shutdown();

	static IGameEvent* CreateEvent(std::string_view name, bool force = false);
	static void FireEvent(IGameEvent* event, bool dontBroadcast);
	static void FireEventToClient(IGameEvent* event, CPlayerSlot slot);
	static void CancelCreatedEvent(IGameEvent* event);

	ResultType OnFireEvent(IGameEvent* event, bool dontBroadcast);
	ResultType OnFireEvent_Post(IGameEvent* event, bool dontBroadcast);

private:
	void FireGameEvent(IGameEvent* event) override;

private:
	plg::flat_hash_map<plg::string, std::shared_ptr<EventHook>, plg::string_hash, std::equal_to<>> m_hookMap;
	std::recursive_mutex m_mutex;
	std::stack<std::shared_ptr<EventHook>> m_eventStack;
	std::stack<IGameEvent*> m_eventCopies;
};
inline EventManager& g_EventManager = EventManager::Instance();
