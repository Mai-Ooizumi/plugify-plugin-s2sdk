#pragma once

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

// Mount identifiers may be official map names or numeric Workshop IDs.
// Only the latter can be passed to Steam UGC and the numeric plugin API.
namespace addon {
inline uint64_t WorkshopId(std::string_view name) {
	uint64_t id{};
	auto [end, error] = std::from_chars(name.data(), name.data() + name.size(), id);
	return error == std::errc() && end == name.data() + name.size() ? id : 0;
}

inline std::string First(std::string_view names) {
	return std::string(names.substr(0, names.find(',')));
}

inline std::vector<std::string> Split(std::string_view names) {
	std::vector<std::string> result;
	while (!names.empty()) {
		auto end = names.find(',');
		if (end != 0)
			result.emplace_back(names.substr(0, end));
		if (end == std::string_view::npos)
			break;
		names.remove_prefix(end + 1);
	}
	return result;
}

template<typename Range>
std::vector<std::string> WithMap(std::string_view map, const Range& workshopAddons) {
	std::vector<std::string> result;
	if (!map.empty())
		result.emplace_back(map);
	for (auto id : workshopAddons) {
		auto name = std::to_string(id);
		if (!std::ranges::contains(result, name))
			result.push_back(std::move(name));
	}
	return result;
}

// Offer the first pending addon while retaining already acquired addons.
// Names must survive this handshake just like numeric Workshop IDs.
inline void ForConnection(std::vector<std::string>& names,
                         const std::vector<std::string>& downloaded,
                         std::string& pending) {
	if (names.empty()) {
		pending.clear();
		return;
	}
	if (!std::ranges::contains(downloaded, names.front()))
		pending = names.front();
	std::erase_if(names, [&](const auto& name) {
		return !std::ranges::contains(downloaded, name) && name != pending;
	});
}
} // namespace addon
