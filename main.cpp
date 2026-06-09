#include <cassert>
#include <memory>

namespace detail {
    template<typename T>
    class seq_base {
    protected:
        size_t size_{0};
        seq_base() = default;
    public:
        const size_t size() const { return size_; }
    };
}

template<typename Alloc, typename T>
void my_destroy(Alloc&, T* p) {
    std::destroy_at(p);
}
template<typename Alloc, typename T, typename... Args>
void my_construct(Alloc&, T* p, Args&&... args) {
    std::construct_at(p, std::forward<Args>(args)...);
}

template<typename T>
class MyVector : public detail::seq_base<T> {
    using base = detail::seq_base<T>;
private:
    std::allocator<T> alloc;
    size_t capacity_{16};
    T* data_{nullptr};

    void destroy_elements() {
        for (size_t i = 0; i < base::size(); ++ i) {
            my_destroy(alloc, data_ + i);
        }
    }
public:
    MyVector():data_{alloc.allocate(capacity_)} {}
    MyVector(const MyVector& other) {
        if (other.size() > capacity_) {
            capacity_ = other.size();
        }
        data_ = alloc.allocate(capacity_);
        std::uninitialized_copy(other.data_, other.data_ + other.size(), data_);
        base::size_ = other.size();
    }
    MyVector(MyVector&& other) :data_{other.data_}, capacity_ {other.capacity_} {
        base::size_ = other.size();
        other.data_ = nullptr;
        other.capacity_ = 0;
        other.size_ = 0;
    }

    MyVector& operator=(const MyVector& other) {
        if (this == &other) {
            return *this;
        }
        size_t new_cap = capacity_;
        if (other.size() > capacity_) {
            new_cap = other.size();
        }
        T* tmp = alloc.allocate(new_cap);
        try {
            std::uninitialized_copy(other.data_, other.data_ + other.size(), tmp);
        } catch (...) {
            alloc.deallocate(tmp, new_cap);
            throw;
        }
        if (nullptr != data_) {
            destroy_elements();
            alloc.deallocate(data_, capacity_);
        }
        data_ = tmp;
        base::size_ = other.size();
        capacity_ = new_cap;
        return *this;
    }

    MyVector& operator=(MyVector&& other) {
        if (this == &other) {
            return *this;
        }
        if (nullptr != data_) {
            destroy_elements();
            alloc.deallocate(data_, capacity_);
        }
        data_ = other.data_;
        other.data_ = nullptr;
        capacity_ = other.capacity_;
        base::size_ = other.size();
        other.capacity_ = 0;
        other.size_ = 0;
        return *this;
    }

    void push_back(const T& d) {
        if (base::size() >= capacity_) {
            size_t new_cap = (capacity_ == 0) ? 16 : capacity_ * 2;
            T* new_data = alloc.allocate(new_cap);
            try {
                if constexpr (std::is_nothrow_move_constructible_v<T>)
                    std::uninitialized_move(data_, data_ + base::size(), new_data);
                else std::uninitialized_copy(data_, data_ + base::size(), new_data);
            } catch (...) {
                alloc.deallocate(new_data, new_cap);
                throw;
            }
            if (nullptr != data_) {
                destroy_elements();
                alloc.deallocate(data_, capacity_);
            }
            data_ = new_data;
            capacity_ = new_cap;
        }
        my_construct(alloc, data_ + base::size(), d);
        ++ base::size_;
    }

    void push_back(T&& d) {
        if (base::size() >= capacity_) {
            size_t new_cap = (capacity_ == 0) ? 16 : capacity_ * 2;
            T* new_data = alloc.allocate(new_cap);
            try {
                if constexpr (std::is_nothrow_move_constructible_v<T>)
                    std::uninitialized_move(data_, data_ + base::size(), new_data);
                else std::uninitialized_copy(data_, data_ + base::size(), new_data);
            } catch (...) {
                alloc.deallocate(new_data, new_cap);
                throw;
            }
            if (nullptr != data_) {
                destroy_elements();
                alloc.deallocate(data_, capacity_);
            }
            data_ = new_data;
            capacity_ = new_cap;
        }
        my_construct(alloc, data_ + base::size(), std::move(d));
        ++ base::size_;
    }

    T& operator[](const size_t idx) {
        assert(idx < base::size());
        return data_[idx];
    }
    const T& operator[](const size_t idx) const {
        assert(idx < base::size());
        return data_[idx];
    }

    void clear() {
        destroy_elements();
        base::size_ = 0;
    }

    ~MyVector() {
        if (nullptr != data_) {
            destroy_elements();
            alloc.deallocate(data_, capacity_);
        }
    }
};



namespace detail {
    struct my_list_node_base {
        my_list_node_base * next_;
        my_list_node_base * prev_;
        my_list_node_base() : next_(this), prev_(this) {}
        ~my_list_node_base() = default;
    };

    template<typename T>
    struct my_list_node : my_list_node_base {
        T value_;
        my_list_node(const T& value) : value_(value) {}
        my_list_node(T&& value) : value_(std::move(value)) {}
    };
}

template<typename T>
class MyList: public detail::seq_base<T> {
    using base_ = detail::seq_base<T>;
    using node_ = detail::my_list_node<T> ;
    detail::my_list_node_base head_;
private:
    void insert_front(node_ * new_node) {
        new_node->next_ = head_.next_;
        new_node->prev_ = &head_;
        head_.next_->prev_ = new_node;
        head_.next_ = new_node;
    }
    void insert_back(node_ * new_node) {
        new_node->next_ = &head_;
        new_node->prev_ = head_.prev_;
        head_.prev_->next_ = new_node;
        head_.prev_ = new_node;
    }
public:
    const bool empty() const { return base_::size() == 0; }

    MyList() = default;
    MyList(const MyList& other) {
        MyList tmp;
        for (auto current = other.head_.next_; current != &other.head_; current = current->next_) {
            tmp.push_back(static_cast<const node_*>(current)->value_);
        }
        *this = std::move(tmp);
    }
    MyList(MyList&& other) noexcept {
        if (!other.empty()) {
            head_.next_ = other.head_.next_;
            head_.prev_ = other.head_.prev_;
            head_.next_->prev_ = &head_;
            head_.prev_->next_ = &head_;
            base_::size_ = other.base_::size();

            other.head_.next_ = &other.head_;
            other.head_.prev_ = &other.head_;
            other.base_::size_ = 0;
        }
    }

    MyList& operator=(const MyList& other){
        if (this != &other) {
            MyList temp(other);
            *this = std::move(temp);
        }
        return *this;
    }

    MyList& operator=(MyList&& other) noexcept {
        if (this != &other) {
            clear();
            if (!other.empty()) {
                head_.next_ = other.head_.next_;
                head_.prev_ = other.head_.prev_;
                head_.next_->prev_ = &head_;
                head_.prev_->next_ = &head_;
                base_::size_ = other.base_::size();

                other.head_.next_ = &other.head_;
                other.head_.prev_ = &other.head_;
                other.base_::size_ = 0;
            }
        }
        return *this;
    }

    void push_front(const T& d) {
        node_ * new_node = new node_(d);
        insert_front(new_node);
        ++base_::size_;
    }
    void push_front(T&& d) {
        node_ * new_node = new node_(std::move(d));
        insert_front(new_node);
        ++base_::size_;
    }
    void push_back (const T& d){
        node_ * new_node = new node_(d);
        insert_back(new_node);
        ++base_::size_;
    }
    void push_back(T&& d){
        node_ * new_node = new node_(std::move(d));
        insert_back(new_node);
        ++ base_::size_;
    }

    void pop_front() {
        if (empty()) {
            return;
        }
        node_ * to_delete = static_cast<node_*>(head_.next_);
        head_.next_ = to_delete->next_;
        to_delete->next_->prev_ = &head_;
        delete to_delete;
        -- base_::size_;
    }
    void pop_back() {
        if (empty()) {
            return;
        }
        node_* to_delete = static_cast<node_*>(head_.prev_);
        head_.prev_ = to_delete->prev_;
        to_delete->prev_->next_ = &head_;
        delete to_delete;
        -- base_::size_;
    }

    T& front() {
        assert(!empty());
        return static_cast<node_*>(head_.next_)->value_;
    }
    const T& front() const {
        assert(!empty());
        return static_cast<const node_*>(head_.next_)->value_;
    }

    T& back() {
        assert(!empty());
        return static_cast<node_*>(head_.prev_)->value_;
    }
    const T& back() const {
        assert(!empty());
        return static_cast<const node_*>(head_.prev_)->value_;
    }

    void clear() {
        auto current = head_.next_;
        while (current != &head_) {
            auto next = current->next_;
            delete static_cast<node_*>(current);
            -- base_::size_;
            current = next;
        }
        head_.next_ = &head_;
        head_.prev_ = &head_;
    }

    ~MyList() {
        clear();
    }
};


template <typename T>
class MyStack{
    MyVector<T> container_;
    size_t top_{0};
public:
    void push(const T& d) {
        if (top_ < container_.size()) {
            container_[top_] = d;
        }
        else {
            container_.push_back(d);
        }
        ++ top_;
    }
    void pop() {
        if (!empty()) {
            -- top_;
        }
    }

    T& top() {
        assert(!empty());
        return container_[top_ - 1];
    }

    size_t size() { return top_; }

    bool empty() { return top_ == 0; }
};

template <typename T>
class MyQueue {
    MyList<T> container_;
public:
    void push(const T& d) {
        container_.push_back(d);
    }
    void push(T&& d) {
        container_.push_back(std::move(d));
    }

    void pop() {
        container_.pop_front();
    }

    T& front() {
        assert(!empty());
        return container_.front();
    }
    T& back() {
        assert(!empty());
        return container_.back();
    }

    size_t size() {
        return container_.size();
    }

    bool empty() {
        return container_.empty();
    }
};

// ============================================================
// Test framework (lightweight, no external dependencies)
// ============================================================
#include <iostream>
#include <string>
#include <sstream>

static int tests_passed = 0;
static int tests_failed = 0;

#define TEST(name) do { \
    std::cout << "  TEST: " << name << " ... "; \
    std::cout.flush(); \
} while(0)

#define PASS() do { \
    std::cout << "PASSED" << std::endl; \
    ++tests_passed; \
} while(0)

#define FAIL(msg) do { \
    std::cout << "FAILED: " << msg << std::endl; \
    ++tests_failed; \
} while(0)

#define CHECK(expr) do { \
    if (!(expr)) { \
        FAIL(#expr); \
        return; \
    } \
} while(0)

#define CHECK_EQ(a, b) do { \
    if ((a) != (b)) { \
        std::ostringstream oss; \
        oss << #a " (" << (a) << ") != " #b " (" << (b) << ")"; \
        FAIL(oss.str()); \
        return; \
    } \
} while(0)

// Helper to count constructions/destructions
struct Tracker {
    inline static int alive = 0;
    inline static int copies = 0;
    inline static int moves = 0;
    int val;

    Tracker(int v = 0) : val(v) { ++alive; }
    Tracker(const Tracker& o) : val(o.val) { ++alive; ++copies; }
    Tracker(Tracker&& o) noexcept : val(o.val) { o.val = 0; ++alive; ++moves; }
    Tracker& operator=(const Tracker&) = default;
    Tracker& operator=(Tracker&&) = default;
    ~Tracker() { --alive; }

    static void reset() { alive = 0; copies = 0; moves = 0; }

    bool operator==(const Tracker& o) const { return val == o.val; }
    bool operator!=(const Tracker& o) const { return val != o.val; }
};

// ============================================================
// Tests for MyVector
// ============================================================
void test_vector_default_construct() {
    TEST("MyVector default construction");
    MyVector<int> v;
    CHECK_EQ(v.size(), 0u);
    PASS();
}

void test_vector_push_back_lvalue() {
    TEST("MyVector push_back (lvalue)");
    MyVector<int> v;
    for (int i = 0; i < 100; ++i) {
        v.push_back(i);
    }
    CHECK_EQ(v.size(), 100u);
    for (int i = 0; i < 100; ++i) {
        CHECK_EQ(v[i], i);
    }
    PASS();
}

void test_vector_push_back_rvalue() {
    TEST("MyVector push_back (rvalue)");
    MyVector<std::string> v;
    v.push_back(std::string("hello"));
    v.push_back("world");
    CHECK_EQ(v.size(), 2u);
    CHECK_EQ(v[0], "hello");
    CHECK_EQ(v[1], "world");
    PASS();
}

void test_vector_push_back_growth() {
    TEST("MyVector push_back triggers growth (capacity * 2)");
    MyVector<int> v;
    // Default capacity is 16, so pushing 32 elements should trigger at least one grow
    for (int i = 0; i < 200; ++i) {
        v.push_back(i);
    }
    CHECK_EQ(v.size(), 200u);
    for (int i = 0; i < 200; ++i) {
        CHECK_EQ(v[i], i);
    }
    PASS();
}

void test_vector_copy_construct() {
    TEST("MyVector copy construction");
    MyVector<int> v1;
    for (int i = 0; i < 50; ++i) v1.push_back(i);
    MyVector<int> v2(v1);
    CHECK_EQ(v1.size(), 50u);
    CHECK_EQ(v2.size(), 50u);
    for (int i = 0; i < 50; ++i) {
        CHECK_EQ(v1[i], i);
        CHECK_EQ(v2[i], i);
    }
    // Verify deep copy (modifying v1 doesn't affect v2)
    v1.push_back(999);
    CHECK_EQ(v1.size(), 51u);
    CHECK_EQ(v2.size(), 50u);
    CHECK_EQ(v2[49], 49);
    PASS();
}

void test_vector_move_construct() {
    TEST("MyVector move construction");
    MyVector<int> v1;
    for (int i = 0; i < 10; ++i) v1.push_back(i);
    MyVector<int> v2(std::move(v1));
    CHECK_EQ(v2.size(), 10u);
    for (int i = 0; i < 10; ++i) {
        CHECK_EQ(v2[i], i);
    }
    PASS();
}

void test_vector_copy_assign() {
    TEST("MyVector copy assignment");
    MyVector<int> v1;
    for (int i = 0; i < 10; ++i) v1.push_back(i);
    MyVector<int> v2;
    v2.push_back(100);
    v2 = v1;
    CHECK_EQ(v2.size(), 10u);
    for (int i = 0; i < 10; ++i) {
        CHECK_EQ(v2[i], i);
    }
    PASS();
}

void test_vector_move_assign() {
    TEST("MyVector move assignment");
    MyVector<int> v1;
    for (int i = 0; i < 10; ++i) v1.push_back(i);
    MyVector<int> v2;
    v2 = std::move(v1);
    CHECK_EQ(v2.size(), 10u);
    for (int i = 0; i < 10; ++i) {
        CHECK_EQ(v2[i], i);
    }
    PASS();
}

void test_vector_self_assign() {
    TEST("MyVector self-assignment");
    MyVector<int> v;
    for (int i = 0; i < 5; ++i) v.push_back(i);
    v = v;
    CHECK_EQ(v.size(), 5u);
    for (int i = 0; i < 5; ++i) {
        CHECK_EQ(v[i], i);
    }
    PASS();
}

void test_vector_clear() {
    TEST("MyVector clear");
    MyVector<int> v;
    for (int i = 0; i < 50; ++i) v.push_back(i);
    v.clear();
    CHECK_EQ(v.size(), 0u);
    // Reuse after clear
    v.push_back(42);
    CHECK_EQ(v.size(), 1u);
    CHECK_EQ(v[0], 42);
    PASS();
}

void test_vector_const_index() {
    TEST("MyVector const operator[]");
    MyVector<int> v;
    for (int i = 0; i < 5; ++i) v.push_back(i);
    const auto& cv = v;
    CHECK_EQ(cv[0], 0);
    CHECK_EQ(cv[4], 4);
    PASS();
}

void test_vector_allocator_handling() {
    TEST("MyVector tracks object lifetime correctly");
    Tracker::reset();
    {
        MyVector<Tracker> v;
        for (int i = 0; i < 10; ++i) {
            v.push_back(Tracker(i));
        }
        CHECK_EQ(Tracker::alive, 10);
        v.clear();
        CHECK_EQ(Tracker::alive, 0);
    }
    CHECK_EQ(Tracker::alive, 0);
    PASS();
}

void test_vector_copy_assign_larger() {
    TEST("MyVector copy assign to larger");
    MyVector<int> v1;
    for (int i = 0; i < 5; ++i) v1.push_back(i);
    MyVector<int> v2;
    for (int i = 0; i < 20; ++i) v2.push_back(i + 100);
    v2 = v1;
    CHECK_EQ(v2.size(), 5u);
    for (int i = 0; i < 5; ++i) CHECK_EQ(v2[i], i);
    PASS();
}

// ============================================================
// Tests for MyList
// ============================================================
void test_list_default_construct() {
    TEST("MyList default construction");
    MyList<int> lst;
    CHECK(lst.empty());
    CHECK_EQ(lst.size(), 0u);
    PASS();
}

void test_list_push_back() {
    TEST("MyList push_back");
    MyList<int> lst;
    for (int i = 0; i < 50; ++i) {
        lst.push_back(i);
    }
    CHECK_EQ(lst.size(), 50u);
    CHECK_EQ(lst.front(), 0);
    CHECK_EQ(lst.back(), 49);
    PASS();
}

void test_list_push_front() {
    TEST("MyList push_front");
    MyList<int> lst;
    for (int i = 0; i < 10; ++i) {
        lst.push_front(i);
    }
    CHECK_EQ(lst.size(), 10u);
    CHECK_EQ(lst.front(), 9);
    CHECK_EQ(lst.back(), 0);
    PASS();
}

void test_list_push_back_rvalue() {
    TEST("MyList push_back (rvalue)");
    MyList<std::string> lst;
    lst.push_back(std::string("first"));
    lst.push_back("second");
    CHECK_EQ(lst.size(), 2u);
    CHECK_EQ(lst.back(), "second");
    CHECK_EQ(lst.front(), "first");
    PASS();
}

void test_list_push_front_rvalue() {
    TEST("MyList push_front (rvalue)");
    MyList<std::string> lst;
    lst.push_front(std::string("A"));
    lst.push_front("B");
    CHECK_EQ(lst.size(), 2u);
    CHECK_EQ(lst.front(), "B");
    CHECK_EQ(lst.back(), "A");
    PASS();
}

void test_list_pop_front() {
    TEST("MyList pop_front");
    MyList<int> lst;
    for (int i = 0; i < 5; ++i) lst.push_back(i);
    lst.pop_front();
    CHECK_EQ(lst.size(), 4u);
    CHECK_EQ(lst.front(), 1);
    lst.pop_front();
    CHECK_EQ(lst.front(), 2);
    CHECK_EQ(lst.back(), 4);
    PASS();
}

void test_list_pop_back() {
    TEST("MyList pop_back");
    MyList<int> lst;
    for (int i = 0; i < 5; ++i) lst.push_back(i);
    lst.pop_back();
    CHECK_EQ(lst.size(), 4u);
    CHECK_EQ(lst.back(), 3);
    lst.pop_back();
    CHECK_EQ(lst.back(), 2);
    CHECK_EQ(lst.front(), 0);
    PASS();
}

void test_list_pop_empty() {
    TEST("MyList pop on empty list (no crash)");
    MyList<int> lst;
    lst.pop_front();
    lst.pop_back();
    CHECK(lst.empty());
    PASS();
}

void test_list_copy_construct() {
    TEST("MyList copy construction");
    MyList<int> lst1;
    for (int i = 0; i < 20; ++i) lst1.push_back(i);
    MyList<int> lst2(lst1);
    CHECK_EQ(lst2.size(), 20u);
    CHECK_EQ(lst2.front(), 0);
    CHECK_EQ(lst2.back(), 19);
    // Deep copy check
    lst1.push_back(999);
    CHECK_EQ(lst2.size(), 20u);
    PASS();
}

void test_list_move_construct() {
    TEST("MyList move construction");
    MyList<int> lst1;
    for (int i = 0; i < 10; ++i) lst1.push_back(i);
    MyList<int> lst2(std::move(lst1));
    CHECK_EQ(lst2.size(), 10u);
    CHECK_EQ(lst2.front(), 0);
    CHECK_EQ(lst2.back(), 9);
    PASS();
}

void test_list_copy_assign() {
    TEST("MyList copy assignment");
    MyList<int> lst1;
    for (int i = 0; i < 10; ++i) lst1.push_back(i);
    MyList<int> lst2;
    lst2.push_back(99);
    lst2 = lst1;
    CHECK_EQ(lst2.size(), 10u);
    CHECK_EQ(lst2.front(), 0);
    CHECK_EQ(lst2.back(), 9);
    PASS();
}

void test_list_move_assign() {
    TEST("MyList move assignment");
    MyList<int> lst1;
    for (int i = 0; i < 10; ++i) lst1.push_back(i);
    MyList<int> lst2;
    lst2 = std::move(lst1);
    CHECK_EQ(lst2.size(), 10u);
    CHECK_EQ(lst2.front(), 0);
    CHECK_EQ(lst2.back(), 9);
    PASS();
}

void test_list_clear() {
    TEST("MyList clear");
    MyList<int> lst;
    for (int i = 0; i < 30; ++i) lst.push_back(i);
    lst.clear();
    CHECK(lst.empty());
    CHECK_EQ(lst.size(), 0u);
    // Reuse after clear
    lst.push_back(100);
    CHECK_EQ(lst.size(), 1u);
    CHECK_EQ(lst.front(), 100);
    CHECK_EQ(lst.back(), 100);
    PASS();
}

void test_list_front_back_const() {
    TEST("MyList const front/back");
    MyList<int> lst;
    lst.push_back(10);
    lst.push_back(20);
    const auto& clst = lst;
    CHECK_EQ(clst.front(), 10);
    CHECK_EQ(clst.back(), 20);
    PASS();
}

void test_list_empty_front_back() {
    TEST("MyList empty after pop all");
    MyList<int> lst;
    lst.push_back(1);
    lst.push_back(2);
    lst.push_back(3);
    lst.pop_front();
    lst.pop_front();
    lst.pop_front();
    CHECK(lst.empty());
    CHECK_EQ(lst.size(), 0u);
    PASS();
}

// ============================================================
// Tests for MyStack
// ============================================================
void test_stack_default_construct() {
    TEST("MyStack default construction");
    MyStack<int> s;
    CHECK(s.empty());
    CHECK_EQ(s.size(), 0u);
    PASS();
}

void test_stack_push_pop() {
    TEST("MyStack push and pop");
    MyStack<int> s;
    s.push(10);
    s.push(20);
    s.push(30);
    CHECK_EQ(s.size(), 3u);
    CHECK_EQ(s.top(), 30);
    s.pop();
    CHECK_EQ(s.size(), 2u);
    CHECK_EQ(s.top(), 20);
    s.pop();
    CHECK_EQ(s.top(), 10);
    s.pop();
    CHECK(s.empty());
    PASS();
}

void test_stack_pop_empty() {
    TEST("MyStack pop on empty");
    MyStack<int> s;
    s.pop(); // should not crash
    CHECK(s.empty());
    PASS();
}

void test_stack_reuse_after_pop_all() {
    TEST("MyStack reuse after popping all");
    MyStack<int> s;
    s.push(1);
    s.push(2);
    s.pop();
    s.pop();
    CHECK(s.empty());
    s.push(42);
    CHECK_EQ(s.size(), 1u);
    CHECK_EQ(s.top(), 42);
    PASS();
}

void test_stack_many_elements() {
    TEST("MyStack with many elements");
    MyStack<int> s;
    for (int i = 0; i < 500; ++i) {
        s.push(i);
    }
    CHECK_EQ(s.size(), 500u);
    for (int i = 499; i >= 0; --i) {
        CHECK_EQ(s.top(), i);
        s.pop();
    }
    CHECK(s.empty());
    PASS();
}

// ============================================================
// Tests for MyQueue
// ============================================================
void test_queue_default_construct() {
    TEST("MyQueue default construction");
    MyQueue<int> q;
    CHECK(q.empty());
    CHECK_EQ(q.size(), 0u);
    PASS();
}

void test_queue_push_pop() {
    TEST("MyQueue push and pop (FIFO)");
    MyQueue<int> q;
    q.push(10);
    q.push(20);
    q.push(30);
    CHECK_EQ(q.size(), 3u);
    CHECK_EQ(q.front(), 10);
    CHECK_EQ(q.back(), 30);
    q.pop();
    CHECK_EQ(q.size(), 2u);
    CHECK_EQ(q.front(), 20);
    q.pop();
    CHECK_EQ(q.front(), 30);
    q.pop();
    CHECK(q.empty());
    PASS();
}

void test_queue_push_rvalue() {
    TEST("MyQueue push (rvalue)");
    MyQueue<std::string> q;
    q.push(std::string("hello"));
    q.push("world");
    CHECK_EQ(q.size(), 2u);
    CHECK_EQ(q.front(), "hello");
    CHECK_EQ(q.back(), "world");
    PASS();
}

void test_queue_pop_empty() {
    TEST("MyQueue pop on empty (no crash)");
    MyQueue<int> q;
    q.pop();
    CHECK(q.empty());
    PASS();
}

void test_queue_many_elements() {
    TEST("MyQueue with many elements");
    MyQueue<int> q;
    for (int i = 0; i < 300; ++i) {
        q.push(i);
    }
    CHECK_EQ(q.size(), 300u);
    for (int i = 0; i < 300; ++i) {
        CHECK_EQ(q.front(), i);
        q.pop();
    }
    CHECK(q.empty());
    PASS();
}

void test_queue_front_back_single() {
    TEST("MyQueue front/back with single element");
    MyQueue<int> q;
    q.push(99);
    CHECK_EQ(q.front(), 99);
    CHECK_EQ(q.back(), 99);
    CHECK_EQ(q.front(), q.back());
    PASS();
}

// ============================================================
// Main
// ============================================================
int main() {
    std::cout << "=== Testing MyVector ===" << std::endl;
    test_vector_default_construct();
    test_vector_push_back_lvalue();
    test_vector_push_back_rvalue();
    test_vector_push_back_growth();
    test_vector_copy_construct();
    test_vector_move_construct();
    test_vector_copy_assign();
    test_vector_move_assign();
    test_vector_self_assign();
    test_vector_clear();
    test_vector_const_index();
    test_vector_allocator_handling();
    test_vector_copy_assign_larger();

    std::cout << "\n=== Testing MyList ===" << std::endl;
    test_list_default_construct();
    test_list_push_back();
    test_list_push_front();
    test_list_push_back_rvalue();
    test_list_push_front_rvalue();
    test_list_pop_front();
    test_list_pop_back();
    test_list_pop_empty();
    test_list_copy_construct();
    test_list_move_construct();
    test_list_copy_assign();
    test_list_move_assign();
    test_list_clear();
    test_list_front_back_const();
    test_list_empty_front_back();

    std::cout << "\n=== Testing MyStack ===" << std::endl;
    test_stack_default_construct();
    test_stack_push_pop();
    test_stack_pop_empty();
    test_stack_reuse_after_pop_all();
    test_stack_many_elements();

    std::cout << "\n=== Testing MyQueue ===" << std::endl;
    test_queue_default_construct();
    test_queue_push_pop();
    test_queue_push_rvalue();
    test_queue_pop_empty();
    test_queue_many_elements();
    test_queue_front_back_single();

    std::cout << "\n========================================" << std::endl;
    int total = tests_passed + tests_failed;
    std::cout << "Results: " << tests_passed << "/" << total << " passed";
    if (tests_failed > 0) {
        std::cout << ", " << tests_failed << " FAILED";
    }
    std::cout << std::endl;
    std::cout << "========================================" << std::endl;

    return tests_failed > 0 ? 1 : 0;
}
