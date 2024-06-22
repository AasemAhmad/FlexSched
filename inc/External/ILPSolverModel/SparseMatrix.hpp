/***********************************************
 * Author: Libur Bukata
 * Modified by: Aasem Ahmad
 ***********************************************/

#pragma once

#include "External/pempek_assert.hpp"
#include <algorithm>
#include <iostream>
#include <set>
#include <stdint.h>
#include <unordered_set>
#include <utility>
#include <vector>

template <class T> class SparseMatrix
{
  public:
    SparseMatrix(T default_value = T()) : default_value(default_value) {}
    SparseMatrix(const SparseMatrix &) = delete;
    SparseMatrix &operator=(const SparseMatrix &) = delete;

    using Row = std::vector<std::pair<size_t, T>>;
    using RowIterator = std::vector<Row>::iterator;
    using RowConstIterator = std::vector<Row>::const_iterator;

    RowIterator begin() { return sparse_matrix.begin(); }

    RowIterator end() { return sparse_matrix.end(); }

    RowConstIterator begin() const { return sparse_matrix.begin(); }

    RowConstIterator end() const { return sparse_matrix.end(); }

    RowConstIterator cbegin() const { return sparse_matrix.cbegin(); }

    RowConstIterator cend() const { return sparse_matrix.cend(); }

    void add_row(Row &&row)
    {
        PPK_ASSERT_ERROR(!row.empty() && check_column(row), "Invalid matrix row!");
        sparse_matrix.emplace_back(std::move(row));
    }

    Row &operator[](const size_t &i)
    {
        PPK_ASSERT_ERROR(i < sparse_matrix.size(), "Row index is out of range!");
        return sparse_matrix[i];
    }

    const Row &operator[](const size_t &i) const
    {
        PPK_ASSERT_ERROR(i < sparse_matrix.size(), "Row index is out of range!");
        return sparse_matrix[i];
    }

    T get(const size_t &i, const size_t &j) const
    {
        PPK_ASSERT_ERROR(i < sparse_matrix.size());
        for (const auto &row : sparse_matrix[i])
        {
            if (row.first == j)
            {
                return row.second;
            }
        }
        return default_value;
    }

    size_t get_nb_rows() const { return sparse_matrix.size(); }

    size_t get_nb_columns() const
    {
        size_t nb_columns = 0;
        for (const auto &row : sparse_matrix)
        {
            for (const auto &element : row)
            {
                nb_columns = std::max(nb_columns, element.first + 1);
            }
        }
        return nb_columns;
    }

    size_t get_nb_elements() const
    {
        size_t nb_elements = 0;
        for (const auto &row : sparse_matrix)
        {
            nb_elements += row.size();
        }
        return nb_elements;
    }

    double get_matrix_density() const
    {
        return ((double)get_nb_elements()) / ((double)get_nb_rows() * get_nb_columns());
    }

    void clear() { sparse_matrix.clear(); }

  private:
    bool check_column(const Row &row) const
    {
        std::unordered_set<size_t> unique_column;
        for (const auto &element : row)
        {
            if (!unique_column.insert(element.first).second)
            {
                return false;
            }
        }
        return true;
    }

    T default_value;
    std::vector<Row> sparse_matrix;
};
