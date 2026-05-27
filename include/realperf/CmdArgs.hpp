#pragma once

#include <charconv>
#include <concepts>
#include <initializer_list>
#include <map>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>

namespace realperf {

class CmdArgs {
public:
    CmdArgs(int argc, char** argv)
    {
        for (int i = 1; i < argc; ++i) {
            const std::string arg{argv[i]};
            if (!is_option(arg)) {
                args_.emplace(positional_name(positional_count_++), arg);
                continue;
            }

            const auto equals = arg.find('=');
            if (equals != std::string::npos) {
                args_[option_name(std::string_view{arg}.substr(0, equals))] = arg.substr(equals + 1);
            } else if (i + 1 < argc && !is_option(argv[i + 1])) {
                args_[option_name(arg)] = argv[++i];
            } else {
                args_[option_name(arg)] = {};
            }
        }
    }

    [[nodiscard]] const std::map<std::string, std::string>& values() const
    {
        return args_;
    }

    [[nodiscard]] bool has(std::string_view name) const
    {
        return args_.contains(std::string{name});
    }

    [[nodiscard]] std::optional<std::string> find(std::initializer_list<std::string_view> names) const
    {
        for (const auto name : names) {
            if (has(name)) {
                return std::string{name};
            }
        }
        return std::nullopt;
    }

    [[nodiscard]] bool has(std::initializer_list<std::string_view> names) const
    {
        return find(names).has_value();
    }

    [[nodiscard]] std::size_t positional_count() const
    {
        return positional_count_;
    }

    [[nodiscard]] std::string_view positional(std::size_t index) const
    {
        return value_for(positional_name(index));
    }

    [[nodiscard]] bool is_positional(std::string_view name) const
    {
        if (name.empty()) {
            return false;
        }
        for (const char ch : name) {
            if (ch < '0' || ch > '9') {
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] std::string_view value_for(std::string_view name) const
    {
        const auto it = args_.find(std::string{name});
        if (it == args_.end()) {
            throw std::runtime_error("missing command line argument " + std::string{name});
        }
        if (it->second.empty()) {
            throw std::runtime_error("missing value for " + std::string{name});
        }
        return it->second;
    }

    template <typename T>
    [[nodiscard]] T as(std::string name) const
    {
        const auto it = args_.find(name);
        if (it == args_.end()) {
            throw std::runtime_error("missing command line argument " + name);
        }

        const auto& value = it->second;
        if constexpr (std::same_as<T, bool>) {
            if (value.empty()) {
                return true;
            }
            if (value == "true" || value == "1") {
                return true;
            }
            if (value == "false" || value == "0") {
                return false;
            }
            throw std::runtime_error("invalid boolean for " + name);
        } else {
            if (value.empty()) {
                throw std::runtime_error("missing value for " + name);
            }

            if constexpr (std::same_as<T, std::string>) {
                return value;
            } else if constexpr (std::same_as<T, std::string_view>) {
                return value;
            } else if constexpr (std::integral<T>) {
                T result{};
                const auto* begin = value.data();
                const auto* end = value.data() + value.size();
                const auto parsed = std::from_chars(begin, end, result);
                if (parsed.ec != std::errc{} || parsed.ptr != end) {
                    throw std::runtime_error("invalid integer for " + name);
                }
                return result;
            } else if constexpr (std::floating_point<T>) {
                T result{};
                const auto* begin = value.data();
                const auto* end = value.data() + value.size();
                const auto parsed = std::from_chars(begin, end, result);
                if (parsed.ec != std::errc{} || parsed.ptr != end) {
                    throw std::runtime_error("invalid number for " + name);
                }
                return result;
            } else {
                T result{};
                std::istringstream input{value};
                input >> result;
                if (!input || !input.eof()) {
                    throw std::runtime_error("invalid value for " + name);
                }
                return result;
            }
        }
    }

private:
    [[nodiscard]] static bool is_option(std::string_view arg)
    {
        return arg.size() > 1 && arg.front() == '-';
    }

    [[nodiscard]] static std::string option_name(std::string_view arg)
    {
        while (!arg.empty() && arg.front() == '-') {
            arg.remove_prefix(1);
        }
        return std::string{arg};
    }

    [[nodiscard]] static std::string positional_name(std::size_t index)
    {
        return std::to_string(index);
    }

    std::map<std::string, std::string> args_;
    std::size_t positional_count_ = 0;
};

} // namespace realperf
