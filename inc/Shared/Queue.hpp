#pragma once

#include <algorithm>
#include <list>
#include <memory>
#include <ranges>
#include <vector>

template <typename ElementIDType, typename SortableElement>
concept HasIDMember = requires(SortableElement element) {
    {
        element.id
    } -> std::convertible_to<ElementIDType>;
};

template <typename ElementIDType, typename SortableElement> class Queue
{
  public:
    Queue();
    Queue(const Queue &) = delete;
    Queue &operator=(const Queue &) = delete;

    using SortableElementIterator = typename std::list<std::shared_ptr<SortableElement>>::iterator;
    using SortableElementConstIterator = typename std::list<std::shared_ptr<SortableElement>>::const_iterator;
    SortableElementIterator begin();
    SortableElementIterator end();
    SortableElementConstIterator begin() const;
    SortableElementConstIterator end() const;
    SortableElementConstIterator cbegin() const;
    SortableElementConstIterator cend() const;
    using SearchResult = std::pair<SortableElementConstIterator, bool>;
    void append_element(const std::shared_ptr<SortableElement> &element);
    SearchResult element_exists(const ElementIDType &id) const;
    std::shared_ptr<SortableElement> get_element(const ElementIDType &id) const;
    SortableElementIterator remove_element(const ElementIDType &id);
    template <typename CompareFunc> void sort_queue(CompareFunc compareFunc);
    bool is_empty() const;
    size_t nb_elements() const;

  private:
    std::list<std::shared_ptr<SortableElement>> elements;
};

template <typename ElementIDType, typename SortableElement> Queue<ElementIDType, SortableElement>::Queue() = default;

template <typename ElementIDType, typename SortableElement>
auto Queue<ElementIDType, SortableElement>::begin() -> SortableElementIterator
{
    return elements.begin();
}

template <typename ElementIDType, typename SortableElement>
auto Queue<ElementIDType, SortableElement>::end() -> SortableElementIterator
{
    return elements.end();
}

template <typename ElementIDType, typename SortableElement>
auto Queue<ElementIDType, SortableElement>::begin() const -> SortableElementConstIterator
{
    return elements.begin();
}

template <typename ElementIDType, typename SortableElement>
auto Queue<ElementIDType, SortableElement>::end() const -> SortableElementConstIterator
{
    return elements.end();
}

template <typename ElementIDType, typename SortableElement>
auto Queue<ElementIDType, SortableElement>::cbegin() const -> SortableElementConstIterator
{
    return elements.cbegin();
}

template <typename ElementIDType, typename SortableElement>
auto Queue<ElementIDType, SortableElement>::cend() const -> SortableElementConstIterator
{
    return elements.cend();
}

template <typename ElementIDType, typename SortableElement>
void Queue<ElementIDType, SortableElement>::append_element(const std::shared_ptr<SortableElement> &element)
{
    const auto &[iterator, exists] = this->element_exists(element->get_id());
    PPK_ASSERT_ERROR(!exists, "element already exists");
    elements.emplace_back(element);
}

template <typename ElementIDType, typename SortableElement>
typename Queue<ElementIDType, SortableElement>::SearchResult
Queue<ElementIDType, SortableElement>::element_exists(const ElementIDType &id) const
{
    const auto &it = std::ranges::find_if(elements, [&id](const auto &element) { return element->get_id() == id; });
    return (it != elements.end()) ? std::make_pair(it, true) : std::make_pair(elements.cend(), false);
}

template <typename ElementIDType, typename SortableElement>
std::shared_ptr<SortableElement> Queue<ElementIDType, SortableElement>::get_element(const ElementIDType &id) const
{
    const auto &[it, exists] = this->element_exists(id);
    return exists ? *it : nullptr;
}
template <typename ElementIDType, typename SortableElement>
Queue<ElementIDType, SortableElement>::SortableElementIterator
Queue<ElementIDType, SortableElement>::remove_element(const ElementIDType &id)
{
    const auto &[it, exists] = this->element_exists(id);
    PPK_ASSERT_ERROR(it != elements.end(), "element was not found");
    return elements.erase(it);
}

template <typename ElementIDType, typename SortableElement>
template <typename CompareFunc>
void Queue<ElementIDType, SortableElement>::sort_queue(CompareFunc compareFunc)
{
    elements.sort(compareFunc);
}

template <typename ElementIDType, typename SortableElement> bool Queue<ElementIDType, SortableElement>::is_empty() const
{
    return elements.empty();
}

template <typename ElementIDType, typename SortableElement>
size_t Queue<ElementIDType, SortableElement>::nb_elements() const
{
    return elements.size();
}
