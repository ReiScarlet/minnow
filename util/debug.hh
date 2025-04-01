#pragma once

#define FMT_HEADER_ONLY
#include <string_view>
#include <fmt/format.h>

// The `debug` function can be called from anywhere and tries to print debugging
// information in the most convenient place.

// If running a test, debug outputs are associated with each test step and printed
// as part of the "unsuccessful test" output. Otherwise, debug outputs go to stderr.
void debug_str( std::string_view message );

template<typename... Args>
// NOLINTNEXTLINE(cppcoreguidelines-missing-std-forward)
void debug( fmt::format_string<Args...> fmt [[maybe_unused]], Args&&... args [[maybe_unused]] )
{
#ifndef NDEBUG
  debug_str( format( fmt, std::forward<Args>( args )... ) );
#endif
}

void set_debug_handler( void ( *handler )( void*, std::string_view ), void* arg );
void reset_debug_handler();
