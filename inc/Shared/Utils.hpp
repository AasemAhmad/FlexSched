#pragma once

#include "External/Batsched/pempek_assert.hpp"
#include <algorithm>
#include <cstdlib>
#include <format>
#include <iterator>
#include <list>
#include <random>
#include <rapidjson/document.h>
#include <source_location>
#include <span>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

template <typename CharT = char> std::basic_string<CharT> source_location_to_string(const std::source_location &loc)
{
    std::string full_function_name = loc.function_name();
    size_t pos = full_function_name.find('(');
    if (pos != std::string::npos)
    {
        return std::basic_string<CharT>(full_function_name.begin(), full_function_name.begin() + pos) + "()";
    } else
    {
        return std::basic_string<CharT>(full_function_name.begin(), full_function_name.end()) + "()";
    }
}

template <typename T> T random_range(T min, T max)
{
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<T> distrib(min, max);
    return distrib(gen);
}

template <class Container>
typename Container::mapped_type get_value(const Container &container, const typename Container::key_type &key,
                                          const std::source_location &loc)
{
    typename Container::const_iterator it = container.find(key);
    PPK_ASSERT_ERROR(it != container.cend(), "Invalid key %s", source_location_to_string(loc).c_str());
    return it->second;
}

template <class Container, class T>
void set_value_helper(Container &container, const typename Container::key_type &key, const T &value,
                      const std::source_location &loc)
{
    auto [iterator, emplaced] = container.try_emplace(key, value);
    PPK_ASSERT_ERROR(emplaced, "Value with the given key was found, function %s!",
                     source_location_to_string(loc).c_str());
}

template <class Container, class T = typename Container::mapped_type>
    requires(!std::is_pointer_v<T>)
void set_value(Container &container, const typename Container::key_type &key, const T &value,
               const std::source_location &loc)
{
    set_value_helper(container, key, value, loc);
}

template <class Container, class T = typename Container::mapped_type>
    requires(std::is_pointer_v<T>)
void set_value(Container &container, const typename Container::key_type &key, const T &value,
               const std::source_location &loc)
{
    PPK_ASSERT_ERROR(value != nullptr, "nullptr value is not allowed, function %s",
                     source_location_to_string(loc).c_str());
    set_value_helper(container, key, value, loc);
}

template <typename Container, typename T, typename Field>
Container sort_by_field(const Container &input, Field T::*field_ptr)
{
    Container sorted = input;
    std::ranges::sort(sorted, [&](const T &a, const T &b) { return a.*field_ptr < b.*field_ptr; });
    return sorted;
}

struct StringHash
{
    using is_transparent = void;
    std::size_t operator()(const std::string &key) const noexcept { return std::hash<std::string>{}(key); }
    std::size_t operator()(const char *key) const noexcept { return std::hash<std::string>{}(key); }
};

struct NumericalStringComparator
{
    using is_transparent = void;
    bool operator()(const std::string &lhs, const std::string &rhs) const { return std::stoul(lhs) < std::stoul(rhs); }
    bool operator()(const std::string &lhs, size_t rhs) const { return std::stoul(lhs) < rhs; }
    bool operator()(size_t lhs, const std::string &rhs) const { return lhs < std::stoul(rhs); }
};

template <typename T>
concept Scalar =
    std::same_as<T, std::string> || std::same_as<T, double> || std::same_as<T, bool> || std::same_as<T, size_t>;

template <Scalar T> inline T parse_scalar(const rapidjson::Value &json_desc, const std::string &field_name);

template <> inline std::string parse_scalar(const rapidjson::Value &json_desc, const std::string &field_name)
{
    PPK_ASSERT_ERROR(json_desc.HasMember(field_name.data()), "%s is missing", field_name.c_str());
    const auto &value = json_desc[field_name.data()];
    PPK_ASSERT_ERROR(value.IsString() || value.IsInt(), "%s field is invalid, it should be a string or integer",
                     field_name.c_str());
    return value.IsInt() ? std::to_string(value.GetInt()) : value.GetString();
}

template <> inline double parse_scalar(const rapidjson::Value &json_desc, const std::string &field_name)
{
    PPK_ASSERT_ERROR(json_desc.HasMember(field_name.data()), "%s field is missing", field_name.c_str());
    const auto &value = json_desc[field_name.data()];
    PPK_ASSERT_ERROR(value.IsNumber() || value.IsDouble(), "%s field is invalid, it should be a number or double",
                     field_name.c_str());
    return value.GetDouble();
}

template <> inline bool parse_scalar(const rapidjson::Value &json_desc, const std::string &field_name)
{
    PPK_ASSERT_ERROR(json_desc.HasMember(field_name.data()), "%s field is missing", field_name.c_str());
    const auto &value = json_desc[field_name.data()];
    PPK_ASSERT_ERROR(value.IsBool(), "%s field is invalid, it should be true or false", field_name.c_str());
    return value.GetBool();
}

template <> inline size_t parse_scalar(const rapidjson::Value &json_desc, const std::string &field_name)
{
    PPK_ASSERT_ERROR(json_desc.HasMember(field_name.data()), "%s field is missing", field_name.c_str());
    const auto &value = json_desc[field_name.data()];
    PPK_ASSERT_ERROR(value.IsUint(), "%s field is invalid, it should be a UINT", field_name.c_str());
    return value.GetUint();
}

template <typename T> void validate_array_field(const rapidjson::Value &json_desc, const std::string &field_name)
{
    PPK_ASSERT_ERROR(json_desc.HasMember(field_name.data()), "%s field is missing", field_name.c_str());
    const auto &array_value = json_desc[field_name.data()];
    PPK_ASSERT_ERROR(array_value.IsArray(), "%s field is invalid, it should be an array", field_name.c_str());
    PPK_ASSERT_ERROR(array_value.Size() > 0, "%s invalid-sized array (size = %d): must be strictly positive",
                     field_name.c_str(), array_value.Size());
}

template <typename T>
concept ArrayElement = Scalar<T>;

template <ArrayElement T>
inline std::vector<T> parse_array(const rapidjson::Value &json_desc, const std::string &field_name);

template <> inline std::vector<double> parse_array(const rapidjson::Value &json_desc, const std::string &field_name)
{
    validate_array_field<double>(json_desc, field_name);

    const auto &array_value = json_desc[field_name.data()];
    std::vector<double> vec;
    vec.reserve(array_value.Size());

    for (const auto &element : array_value.GetArray())
    {
        PPK_ASSERT_ERROR(element.IsNumber(), "%s array element is invalid: all elements must be numbers",
                         field_name.c_str());

        vec.push_back(element.GetDouble());

        PPK_ASSERT_ERROR(vec.back() >= 0, "%s array element is invalid: all elements must be non-negative",
                         field_name.c_str());
    }
    return vec;
}

template <> inline std::vector<size_t> parse_array(const rapidjson::Value &json_desc, const std::string &field_name)
{
    validate_array_field<unsigned>(json_desc, field_name);

    const auto &array_value = json_desc[field_name.data()];
    std::vector<size_t> vec;
    vec.reserve(array_value.Size());

    for (const auto &element : array_value.GetArray())
    {
        PPK_ASSERT_ERROR(element.IsUint(), "%s array element is invalid: all elements must be Uint",
                         field_name.c_str());

        vec.push_back(element.GetUint());
    }

    return vec;
}

template <>
inline std::vector<std::string> parse_array(const rapidjson::Value &json_desc, const std::string &field_name)
{
    validate_array_field<std::string>(json_desc, field_name);

    const auto &array_value = json_desc[field_name.data()];
    std::vector<std::string> vec;
    vec.reserve(array_value.Size());

    for (const auto &element : array_value.GetArray())
    {
        PPK_ASSERT_ERROR(element.IsUint(), "%s array element is invalid: all elements must be string",
                         field_name.c_str());

        vec.emplace_back(element.GetString());
    }
    return vec;
}