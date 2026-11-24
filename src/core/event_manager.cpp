#include "event_manager.hpp"

EventManager EventManager::instance;

void EventManager::Shutdown() {
	std::scoped_lock lock(m_mutex);
	if (g_pGameEventManager) {
		g_pGameEventManager->RemoveListener(this);
	}
	m_hookMap.clear();
	while (!m_eventStack.empty()) {
		if (auto copy = m_eventStack.top().copy; g_pGameEventManager && copy) {
			g_pGameEventManager->FreeEvent(copy);
		}
		m_eventStack.pop();
	}
}

EventHookError EventManager::HookEvent(std::string_view name, EventListenerCallback callback, HookMode mode) {
	std::scoped_lock lock(m_mutex);

	if (name.empty()) {
		plg::print(LS_WARNING, "Event name empty\n");
		return EventHookError::InvalidEvent;
	}

	if (!g_pGameEventManager->FindListener(this, name.data())) {
		if (g_pGameEventManager->AddListener(this, name.data(), true) == -1) {
			plg::print(LS_WARNING, "Event name invalid: {}\n", name);
			return EventHookError::InvalidEvent;
		}
	}

	std::shared_ptr<EventHook> hook;
	{
		auto it = m_hookMap.find(name);
		if (it != m_hookMap.end()) {
			hook = it->second;
		} else {
			hook = std::make_shared<EventHook>(name);
			m_hookMap.emplace(name, hook);
		}
	}
	return hook->callbacks[mode].Register(callback) ? EventHookError::Okay : EventHookError::InvalidCallback;
}

EventHookError EventManager::UnhookEvent(std::string_view name, EventListenerCallback callback, HookMode mode) {
	std::scoped_lock lock(m_mutex);

	if (name.empty()) {
		plg::print(LS_WARNING, "Event name empty\n");
		return EventHookError::InvalidEvent;
	}

	auto it = m_hookMap.find(name);
	if (it != m_hookMap.end()) {
		auto hook = it->second;
		if (!hook->callbacks[mode].Unregister(callback)) {
			return EventHookError::InvalidCallback;
		}
		if (hook->callbacks[HookMode::Pre].Empty() && hook->callbacks[HookMode::Post].Empty()) {
			m_hookMap.erase(it);
		}
		return EventHookError::Okay;
	}

	return EventHookError::NotActive;
}

void EventManager::FireGameEvent(IGameEvent* event) {
}

IGameEvent* EventManager::CreateEvent(std::string_view name, bool force) {
	return g_pGameEventManager->CreateEvent(name.data(), force);
}

void EventManager::FireEvent(IGameEvent* event, bool dontBroadcast) {
	g_pGameEventManager->FireEvent(event, dontBroadcast);
}

void EventManager::FireEventToClient(IGameEvent* event, CPlayerSlot slot) {
	IGameEventListener2* listener = addresses::GetLegacyGameEventListener(slot);
	if (!listener) {
		plg::print(LS_WARNING, "Could not get player event listener\n");
		return;
	}

	listener->FireGameEvent(event);
}

void EventManager::CancelCreatedEvent(IGameEvent* event) {
	g_pGameEventManager->FreeEvent(event);
}

ResultType EventManager::OnFireEvent(IGameEvent* event, const bool dontBroadcast) {
	if (!event)
		return ResultType::Continue;

	std::unique_lock lock(m_mutex);
	auto it = m_hookMap.find(event->GetName());
	if (it == m_hookMap.end()) {
		m_eventStack.push({});
		return ResultType::Continue;
	}

	auto hook = it->second;
	m_eventStack.push({hook});
	auto& preHook = hook->callbacks[HookMode::Pre];
	auto preFuncs = !preHook.Empty() ? preHook.Get() : nullptr;
	lock.unlock();

	bool localDontBroadcast = dontBroadcast;
	if (preFuncs) {
		plg::print(LS_DETAILED, "Pushing event `{}` pointer: {}, dont broadcast: {}, post: {}\n", event->GetName(), static_cast<const void*>(event), dontBroadcast, false);
		plg::string name(event->GetName());
		for (const auto& func : preFuncs->handlers) {
			auto result = func(name, event, dontBroadcast);
			localDontBroadcast = event->GetBool("dont_broadcast");

			if (result >= ResultType::Handled) {
				// PolyHook still calls Post for a superceded event.
				g_pGameEventManager->FreeEvent(event);
				return ResultType::Handled;
			}
		}
	}

	lock.lock();
	if (!m_eventStack.empty() && m_eventStack.top().hook == hook && !hook->callbacks[HookMode::Post].Empty()) {
		m_eventStack.top().copy = g_pGameEventManager->DuplicateEvent(event);
	}

	if (localDontBroadcast != dontBroadcast) {
		return ResultType::Changed;
	}

	return ResultType::Continue;
}

ResultType EventManager::OnFireEvent_Post(IGameEvent* event, bool dontBroadcast) {
	std::unique_lock lock(m_mutex);
	if (!event || m_eventStack.empty())
		return ResultType::Continue;

	auto [hook, eventCopy] = std::move(m_eventStack.top());
	m_eventStack.pop();
	lock.unlock();

	if (eventCopy) {
		auto& postHook = hook->callbacks[HookMode::Post];
		if (!postHook.Empty()) {
			plg::print(LS_DETAILED, "Pushing event `{}` pointer: {}, dont broadcast: {}, post: {}\n", eventCopy->GetName(), static_cast<const void*>(eventCopy), dontBroadcast, true);
			postHook(hook->name, eventCopy, dontBroadcast);
		}

		g_pGameEventManager->FreeEvent(eventCopy);
	}

	return ResultType::Continue;
}