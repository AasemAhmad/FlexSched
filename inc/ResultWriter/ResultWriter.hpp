#pragma once

#include "Shared/Utils.hpp"
#include <OpenXLSX.hpp>
#include <functional>
#include <string>
#include <unordered_map>
#include <vector>

class ResultWriter
{
  public:
    explicit ResultWriter(const std::string &file_name, const std::vector<std::string> &header);
    ResultWriter(const ResultWriter &) = delete;
    ResultWriter &operator=(const ResultWriter &) = delete;
    ~ResultWriter();

    using ColumnName = std::string;
    using ColumnIndex = uint16_t;
    using ColumnValue = std::string;
    using Row = std::unordered_map<ColumnName, ColumnValue, StringHash, std::equal_to<>>;

    void create_new_sheet(const std::string &sheet_name) const;
    void write_header(const std::string &sheet_name) const;
    void write_rows(const std::vector<Row> &rows, const std::string &sheet_name) const;
    void clear_cell_contents(const std::string &sheet_name) const;
    bool sheet_exists(const std::string &sheet_name) const;
    void flush();

  private:
    void close_work_book();
    OpenXLSX::XLDocument doc;
    std::map<ColumnIndex, ColumnName> column_map;
};
