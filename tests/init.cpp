#include <threadsafe_queue.hpp>
#include <catch.hpp>

SCENARIO("threadsafe queue init", "[init]") {
	threadsafe_queue<int> queue;
	REQUIRE(queue.empty());
}
