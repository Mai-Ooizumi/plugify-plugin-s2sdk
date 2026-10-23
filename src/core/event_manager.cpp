#include "event_manager.hpp"

EventManager EventManager::instance;

EventManager::~EventManager() = default;

void EventManager::Shutdown() {
	std::scoped_lock lock(m_mutex);
	// Engine registration outlives individual callbacks, including an empty hook map.
	if (g_pGameEventManager) {
		g_pGameEventManager->RemoveListener(this);
	}
	m_hookMap.clear();
	while (!m_eventCopies.empty()) {
		if (g_pGameEventManager) {
			g_pGameEventManager->FreeEvent(m_eventCopies.top());
		}
		m_eventCopies.pop();
	}
	while (!m_eventStack.empty()) {
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
			if (mode == HookMode::Post) {
				hook->postCopy = true;
			}
		} else {
			hook = std::make_shared<EventHook>(name, mode == HookMode::Post);
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
		auto status = hook->RemoveCallback(callback, mode);
		if (status == EventHookUnregisterResult::Empty) {
			m_hookMap.erase(it);
		}
		return status == EventHookUnregisterResult::NotRegistered ? EventHookError::InvalidCallback : EventHookError::Okay;
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

	bool localDontBroadcast = dontBroadcast;

	std::scoped_lock lock(m_mutex);
	auto it = m_hookMap.find(event->GetName());
	if (it != m_hookMap.end()) {
		auto hook = it->second;
		++hook->refCount;
		m_eventStack.push(hook);

		auto& preHook = hook->callbacks[HookMode::Pre];
		if (!preHook.Empty()) {
			plg::print(LS_DETAILED, "Pushing event `{}` pointer: {}, dont broadcast: {}, post: {}\n", event->GetName(), static_cast<const void*>(event), dontBroadcast, false);

			if (auto funcs = preHook.Get()) {
				plg::string name(event->GetName());
				for (const auto& func : funcs->handlers) {
					auto result = func(name, event, dontBroadcast);
					localDontBroadcast = event->GetBool("dont_broadcast");

					if (result >= ResultType::Handled) {
						// m_EventCopies.push(g_gameEventManager->DuplicateEvent(pEvent));
						g_pGameEventManager->FreeEvent(event);
						return ResultType::Handled;
					}
				}
			}
		}

		if (hook->postCopy) {
			m_eventCopies.push(g_pGameEventManager->DuplicateEvent(event));
		}
	} else {
		m_eventStack.push(nullptr);
	}

	if (localDontBroadcast != dontBroadcast) {
		return ResultType::Changed;
	}

	return ResultType::Continue;
}

ResultType EventManager::OnFireEvent_Post(IGameEvent* event, bool dontBroadcast) {
	if (!event || m_eventStack.empty())
		return ResultType::Continue;

	if (auto hook = m_eventStack.top()) {
		auto& postHook = hook->callbacks[HookMode::Post];
		if (!postHook.Empty()) {
			if (hook->postCopy) {
				auto eventCopy = m_eventCopies.top();
				plg::print(LS_DETAILED, "Pushing event `{}` pointer: {}, dont broadcast: {}, post: {}\n", eventCopy->GetName(), static_cast<const void*>(eventCopy), dontBroadcast, true);

				postHook(hook->name, eventCopy, dontBroadcast);

				g_pGameEventManager->FreeEvent(eventCopy);

				m_eventCopies.pop();
			} else {
				postHook(hook->name, nullptr, dontBroadcast);
			}
		}

		if (hook->FinishDispatch()) {
			m_hookMap.erase(hook->name);
		}
	}

	m_eventStack.pop();

	return ResultType::Continue;
}
