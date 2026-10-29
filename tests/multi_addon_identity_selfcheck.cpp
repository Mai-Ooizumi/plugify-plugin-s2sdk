#include "core/addon_identity.hpp"

#include <cstdlib>
#include <iostream>
#include <limits>

#define CHECK(condition) \
	do { \
		if (!(condition)) { \
			std::cerr << "FAIL line " << __LINE__ << ": " #condition "\n"; \
			return EXIT_FAILURE; \
		} \
	} while (false)

int main() {
	using Names = std::vector<std::string>;
	const std::vector<uint64_t> buttons{3763619947};
	CHECK(addon::WorkshopId("3763619947") == 3763619947);
	CHECK(addon::WorkshopId("18446744073709551615") == std::numeric_limits<uint64_t>::max());
	CHECK(addon::WorkshopId("") == 0);
	CHECK(addon::WorkshopId("de_debris") == 0);
	CHECK(addon::WorkshopId("3763619947junk") == 0);
	CHECK(addon::WorkshopId("18446744073709551616") == 0);

	// The three crash dumps' map groups must keep their resources before the button pack.
	for (const auto& map : {"de_debris", "de_fachwerk", "de_poseidon"}) {
		auto mounts = addon::WithMap(map, buttons);
		CHECK((mounts == Names{map, "3763619947"}));
		CHECK(addon::First(std::string(map) + ",3763619947") == map);
		CHECK((addon::Split(std::string(map) + ",3763619947") == mounts));

		// First connection acquires the map addon; the next signon acquires the button pack.
		Names downloaded;
		std::string pending;
		auto reply = mounts;
		addon::ForConnection(reply, downloaded, pending);
		CHECK((reply == Names{map}));
		CHECK(pending == map);
		downloaded.push_back(pending);
		pending.clear();
		auto remaining = mounts;
		for (const auto& name : downloaded)
			std::erase(remaining, name);
		CHECK((remaining == Names{"3763619947"}));
		pending = remaining.front();
		reply = mounts;
		addon::ForConnection(reply, downloaded, pending);
		CHECK(reply == mounts);
		downloaded.push_back(pending);
		pending.clear();
		reply = mounts;
		addon::ForConnection(reply, downloaded, pending);
		CHECK(reply == mounts);
		CHECK(pending.empty());
	}

	// Existing numeric callers and addon order/deduplication remain unchanged.
	CHECK((addon::WithMap("123", std::vector<uint64_t>{123, 3763619947, 3763619947}) == Names{"123", "3763619947"}));
	CHECK((addon::WithMap("", buttons) == Names{"3763619947"}));
	CHECK((addon::WithMap("de_poseidon", std::vector<uint64_t>{}) == Names{"de_poseidon"}));
	CHECK(addon::First("").empty());
	CHECK(addon::Split("").empty());
	Names empty;
	std::string pending = "old-map";
	addon::ForConnection(empty, {}, pending);
	CHECK(pending.empty());
	std::cout << "PASS: named map and Workshop addon identity, order and connection handshake\n";
	return EXIT_SUCCESS;
}
