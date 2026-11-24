#ifndef ERL_PRIORITY_DISPATCH_QUEUE_HPP
#define ERL_PRIORITY_DISPATCH_QUEUE_HPP

// Unit IV: generic class template. Compare(a, b) returning true means "a has LOWER
// priority than b" (same convention as std::priority_queue), so top() is the most urgent.

#include <algorithm>
#include <functional>
#include <stdexcept>
#include <vector>

namespace ReliefOps {

template <class T, class Compare = std::less<T> >
class PriorityDispatchQueue {
private:
    std::vector<T> heap_;
    Compare comp_;

public:
    explicit PriorityDispatchQueue(const Compare &c = Compare()) : heap_(), comp_(c) {}

    void push(const T &item) {
        heap_.push_back(item);
        std::push_heap(heap_.begin(), heap_.end(), comp_);
    }

    const T &top() const {
        if (heap_.empty())
            throw std::out_of_range("PriorityDispatchQueue is empty");
        return heap_.front();
    }

    T pop() {
        if (heap_.empty())
            throw std::out_of_range("PriorityDispatchQueue is empty");
        std::pop_heap(heap_.begin(), heap_.end(), comp_);
        T item = heap_.back();
        heap_.pop_back();
        return item;
    }

    bool empty() const { return heap_.empty(); }
    std::size_t size() const { return heap_.size(); }
};

}  // namespace ReliefOps

#endif
