#include "ResultWriter/ResultWriter.hpp"
#include "External/Batsched/pempek_assert.hpp"
#include "loguru.hpp"
#include <vector>

ResultWriter::ResultWriter(const std::string &file_name, const std::vector<std::string> &header)
{
    doc.create(file_name);
    PPK_ASSERT_ERROR(doc.isOpen(), "Failed to open the file");
    uint16_t col_index = 0;
    for (const auto &item : header)
    {
        column_map[col_index] = item;
        ++col_index;
    }
}

ResultWriter::~ResultWriter() { close_work_book(); }

void ResultWriter::create_new_sheet(const std::string &sheet_name) const
{
    PPK_ASSERT_ERROR(!sheet_exists(sheet_name), "sheet %s exists", sheet_name.c_str());
    doc.workbook().addWorksheet(sheet_name);
}

void ResultWriter::write_header(const std::string &sheet_name) const
{
    PPK_ASSERT_ERROR(sheet_exists(sheet_name), "sheet %s does not exist", sheet_name.c_str());
    auto sheet = doc.workbook().worksheet(sheet_name);
    uint32_t current_row = sheet.rowCount();
    PPK_ASSERT_ERROR(current_row == 0, "sheet %s is not empty", sheet_name.c_str());
    for (const auto &[column_index, column_name] : column_map)
    {
        sheet.cell(current_row + 1, column_index + 1).value() = column_name;
    }
}

void ResultWriter::write_rows(const std::vector<Row> &rows, const std::string &sheet_name) const
{
    PPK_ASSERT_ERROR(sheet_exists(sheet_name), "sheet %s does not exist", sheet_name.c_str());
    auto sheet = doc.workbook().worksheet(sheet_name);
    uint32_t current_row = sheet.rowCount();

    for (const auto &row : rows)
    {
        for (const auto &[column_index, column_name] : column_map)
        {
            PPK_ASSERT_ERROR(row.contains(column_name), "Invalid column name %s", column_name.c_str());
            sheet.cell(current_row + 1, column_index + 1).value() = row.at(column_name);
        }
        ++current_row;
    }
}

bool ResultWriter::sheet_exists(const std::string &sheet_name) const { return doc.workbook().sheetExists(sheet_name); }

void ResultWriter::clear_cell_contents(const std::string &sheet_name) const
{
    PPK_ASSERT_ERROR(sheet_exists(sheet_name), "sheet %s does not exist", sheet_name.c_str());
    auto sheet = doc.workbook().worksheet(sheet_name);
    sheet.range().clear();
}

void ResultWriter::close_work_book()
{
    doc.save();
    doc.close();
}

void ResultWriter::flush() { doc.save(); }