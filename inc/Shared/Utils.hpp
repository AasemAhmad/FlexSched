#pragma once

#include "External/Batsched/pempek_assert.hpp"
#include <algorithm>
#include <iostream>
#include <list>
#include <random>
#include <rapidjson/document.h>
#include <source_location>
#include <string>
#include <type_traits>
#include <unordered_map>
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

template <typename T> inline T parse_scalar(const rapidjson::Value &json_desc, const std::string &field_name);

template <> inline std::string parse_scalar(const rapidjson::Value &json_desc, const std::string &field_name)
{
    PPK_ASSERT_ERROR(json_desc.HasMember(field_name.c_str()), "Missing: %s Field", field_name.c_str());
    PPK_ASSERT_ERROR(json_desc[field_name.c_str()].IsString() || json_desc[field_name.c_str()].IsInt(),
                     "Invalid Value: %s Field should be a string or an integer", field_name.c_str());

    if (json_desc[field_name.c_str()].IsInt())
    {
        return std::to_string(json_desc[field_name.c_str()].GetInt());
    }
    return json_desc[field_name.c_str()].GetString();
}

template <> inline long double parse_scalar(const rapidjson::Value &json_desc, const std::string &field_name)
{
    PPK_ASSERT_ERROR(json_desc.HasMember(field_name.c_str()), "Missing: %s Field", field_name.c_str());
    PPK_ASSERT_ERROR(json_desc[field_name.c_str()].IsNumber() || json_desc[field_name.c_str()].IsDouble(),
                     "Invalid Value: %s Field should be a number  or double", field_name.c_str());
    return json_desc[field_name.c_str()].GetDouble();
}

template <> inline bool parse_scalar(const rapidjson::Value &json_desc, const std::string &field_name)
{
    PPK_ASSERT_ERROR(json_desc.HasMember(field_name.c_str()), "Missing: %s Field", field_name.c_str());
    PPK_ASSERT_ERROR(json_desc[field_name.c_str()].IsBool(), "Invalid Value: %s  Field should be true or false",
                     field_name.c_str());
    return json_desc[field_name.c_str()].GetBool();
}

template <> inline size_t parse_scalar(const rapidjson::Value &json_desc, const std::string &field_name)
{
    PPK_ASSERT_ERROR(json_desc.HasMember(field_name.c_str()), "Missing: %s Field", field_name.c_str());
    PPK_ASSERT_ERROR(json_desc[field_name.c_str()].IsUint(), "invalid Value: %s Field should be a UINT", field_name.c_str());
    return json_desc[field_name.c_str()].GetUint();
}

template <typename T> inline std::vector<T> parse_array(const rapidjson::Value &json_desc, const std::string &field_name);

template <> inline std::vector<long double> parse_array(const rapidjson::Value &json_desc, const std::string &field_name)
{
    PPK_ASSERT_ERROR(json_desc.HasMember(field_name.c_str()), "Missing: Field", field_name.c_str());
    PPK_ASSERT_ERROR(json_desc[field_name.c_str()].IsArray(), "Invalid Value: %s Field should be an array", field_name.c_str());
    const rapidjson::Value &array_value = json_desc[field_name.c_str()];
    PPK_ASSERT_ERROR(array_value.IsArray(), "Invalid Value: %s Field should be an array", field_name.c_str());

    PPK_ASSERT_ERROR(array_value.Size() > 0, "Invalid Array Size:  %s Field, array size must be strictly positive (size = % d)",
                     field_name.c_str(), array_value.Size());

    std::vector<long double> vec(array_value.Size(), 0);

    for (size_t i = 0; i < vec.size(); ++i)
    {
        PPK_ASSERT_ERROR(array_value[i].IsNumber(), "Invalid Value: %s Field, array elements must be numbers",
                         field_name.c_str());

        vec[i] = array_value[i].GetDouble();

        PPK_ASSERT_ERROR(vec[i] >= 0, "Invalid Value: %s Field, all array elements must be non-negative", field_name.c_str());
    }

    return vec;
}

template <> inline std::vector<size_t> parse_array(const rapidjson::Value &json_desc, const std::string &field_name)
{
    PPK_ASSERT_ERROR(json_desc.HasMember(field_name.c_str()), "Missing: %s Field", field_name.c_str());

    PPK_ASSERT_ERROR(json_desc[field_name.c_str()].IsArray(), "Invalid Value: Field %s should be an array", field_name.c_str());

    const rapidjson::Value &array_value = json_desc[field_name.c_str()];

    PPK_ASSERT_ERROR(array_value.IsArray(), "Invalid Value: %s Field should be an array", field_name.c_str());

    PPK_ASSERT_ERROR(array_value.Size() > 0, "Invalid Array Size:  %s Field, array size must be strictly positive (size = % d)",
                     field_name.c_str(), array_value.Size());

    std::vector<size_t> vec(array_value.Size(), 0);

    for (size_t i = 0; i < vec.size(); ++i)
    {
        PPK_ASSERT_ERROR(array_value[i].IsUint(), "Invalid Value: %s Field array element should be Uint", field_name.c_str());

        vec[i] = array_value[i].GetUint();
    }
    return vec;
}

template <> inline std::vector<std::string> parse_array(const rapidjson::Value &json_desc, const std::string &field_name)
{
    PPK_ASSERT_ERROR(json_desc.HasMember(field_name.c_str()), "Missing: %s Field", field_name.c_str());

    PPK_ASSERT_ERROR(json_desc[field_name.c_str()].IsArray(), "Invalid Value: Field %s should be an array", field_name.c_str());

    const rapidjson::Value &array_value = json_desc[field_name.c_str()];

    PPK_ASSERT_ERROR(array_value.IsArray(), "Invalid Value: %s Field should be an array", field_name.c_str());

    PPK_ASSERT_ERROR(array_value.Size() > 0, "Invalid Array Size:  %s Field, array size must be strictly positive (size = % d)",
                     field_name.c_str(), array_value.Size());

    std::vector<std::string> vec(array_value.Size(), "");

    for (size_t i = 0; i < array_value.Size(); ++i)
    {
        PPK_ASSERT_ERROR(array_value[i].IsString(), "Invalid Value: %s: array elements should be string", field_name.c_str());

        vec[i] = array_value[i].GetString();
    }
    return vec;
}

template <typename K, typename V>
inline std::unordered_map<K, V> parse_map(const rapidjson::Value &json_desc, const std::string &field_name,
                                          const std::string &key_name, const std::string &value_name);
template <>
inline std::unordered_map<size_t, std::string> parse_map(const rapidjson::Value &json_desc, const std::string &field_name,
                                                         const std::string &key_name, const std::string &value_name)
{
    std::unordered_map<size_t, std::string> map;

    PPK_ASSERT_ERROR(json_desc.HasMember(field_name.c_str()), " Missing:  %s Field", field_name.c_str());
    PPK_ASSERT_ERROR(json_desc[field_name.c_str()].IsArray(), "Invalid Type: %s Field should be an array", field_name.c_str());
    const rapidjson::Value &array_value = json_desc[field_name.c_str()];
    PPK_ASSERT_ERROR(array_value.IsArray(), "Invalid Type: %s Field should be an array", field_name.c_str());

    PPK_ASSERT_ERROR(array_value.Size() > 0, "Invalid Array Size:  %s Field, array size must be strictly positive (size = % d)",
                     field_name.c_str(), array_value.Size());

    for (size_t i = 0; i < array_value.Size(); ++i)
    {
        PPK_ASSERT_ERROR(array_value[i].IsObject(), "Invalid Type: %s Field, all elements must be objects", field_name.c_str());
        PPK_ASSERT_ERROR(array_value[i].HasMember(key_name.c_str()), "Invalid Type: %s Field, all elements must have a %s key",
                         field_name.c_str(), key_name.c_str());
        PPK_ASSERT_ERROR(array_value[i].HasMember(value_name.c_str()), "Invalid Type: %s Field all elements must have a %s Value",
                         field_name.c_str(), value_name.c_str());

        size_t key = array_value[i][key_name.c_str()].GetUint();
        PPK_ASSERT_ERROR(key > 0, "Invalid Key: %s Field, key must be strictly positive");
        std::string value = array_value[i][value_name.c_str()].GetString();

        map.try_emplace(key, value);
    }

    return map;
}

template <typename Container, typename T, typename Field> Container sort_by_field(const Container &input, Field T::*field_ptr)
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