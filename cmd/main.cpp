#include <print>
#include <string_view>

auto main() -> int
{
    constexpr std::string_view message = "Hello, C++23!";
    std::println("{}", message);
}
