//
// Created by generalsuslik on 16.03.25.
//

#ifndef DEQUE_HPP
#define DEQUE_HPP

#include <new>
#include <stdexcept>
#include <utility>

template <typename T, std::size_t BucketSize = 64>
struct allocator {
    T* allocate() {
        return reinterpret_cast<T*>(new char[BucketSize * sizeof(T)]);
    }

    T** allocate_map(const std::size_t blocks_count) {
        T** res = reinterpret_cast<T**>(new char[BucketSize * blocks_count * sizeof(T)]);
        for (std::size_t i = 0; i < blocks_count; ++i) {
            res[i] = allocate();
        }

        return res;
    }

    void deallocate(T* ptr) {
        delete[] reinterpret_cast<char*>(ptr);
    }

    template <typename... Args>
    void construct(T* ptr, Args&... args) {
        new (ptr) T(std::forward<Args>(args)...);
    }

    void destroy(T* ptr) {
        ptr->~T();
    }
};

template <typename T, typename Alloc = allocator<T>>
class Deque {
private:
    using size_type = int64_t;
    
    T** buckets_;  // pointer to c-arrays of T
    size_type bucket_size_ = 64; // size of c-array

    size_type size_;
    size_type cap_;
    size_type buckets_cap_;

    std::pair<size_type, size_type> front_index_;
    std::pair<size_type, size_type> back_index_;

    Alloc alloc_;

private:
    template <bool IsConst, size_type BucketSize = 64>
    class rand_access_iterator {
    public:
        using ptr_t = std::conditional_t<IsConst, const T*, T*>;
        using ref_t = std::conditional_t<IsConst, const T&, T&>;
        using val_t = T;

    private:
        ptr_t* parent_map_;
        std::pair<size_type, size_type> pos_;
        ptr_t ptr_;
        explicit rand_access_iterator(ptr_t* parent_map, const std::pair<size_type, size_type>& pos)
            : parent_map_(parent_map)
            , pos_(pos)
            , ptr_(parent_map_[pos_.first] + pos_.second) {}

        rand_access_iterator(T** parent_map, const size_type pos_first, const size_type pos_second)
            : parent_map_(parent_map)
            , pos_(pos_first, pos_second)
            , ptr_(parent_map_[pos_.first] + pos_.second) {}

        friend class Deque<T>;

    public:
        rand_access_iterator(const rand_access_iterator&) = default;
        rand_access_iterator& operator=(const rand_access_iterator&) = default;

        ref_t operator*() const {
            return *ptr_;
        }

        ptr_t operator->() const {
            return ptr_;
        }

        rand_access_iterator& operator++() {
            if (pos_.second == BucketSize - 1) {
                ++pos_.first;
                pos_.second = 0;
            } else {
                ++pos_.second;
            }

            ptr_ = parent_map_[pos_.first] + pos_.second;
            return *this;
        }

        rand_access_iterator operator++(int) {
            rand_access_iterator copy = *this;
            ++this;
            return copy;
        }

        rand_access_iterator& operator--() {
            if (pos_.second == 0) {
                --pos_.first;
                pos_.second = BucketSize - 1;
            } else {
                --pos_.second;
            }

            ptr_ = parent_map_[pos_.first] + pos_.second;
            return *this;
        }

        rand_access_iterator operator--(int) {
            rand_access_iterator copy = *this;
            --this;
            return copy;
        }

        rand_access_iterator& operator+=(const size_type rhs) {
            if (rhs == 0) {
                return *this;
            }

            if (rhs < 0) {
                this -= std::abs(rhs);
                return *this;
            }
            
            if (pos_.second + rhs < BucketSize) {
                pos_.second += rhs;
                ptr_ = parent_map_[pos_.first][pos_.second];
            } else {
                const size_type first_to_add = (pos_.second + rhs) / BucketSize + ((pos_.second + rhs) % BucketSize != 0);
                pos_.first += first_to_add;
                pos_.second = (pos_.second + rhs) % BucketSize;
            }

            ptr_ = parent_map_[pos_.first][pos_.second];
            return *this;   
        }

        rand_access_iterator& operator-=(const size_type rhs) {
            if (rhs == 0) {
                return *this;
            }

            if (rhs < 0) {
                this += std::abs(rhs);
                return *this;
            }
            
            if (pos_.second - rhs >= 0) {
                pos_.second -= rhs;
                ptr_ = parent_map_[pos_.first][pos_.second];
            } else {
                const size_type first_to_remove = (rhs - pos_.second) / BucketSize + ((rhs - pos_.second) % BucketSize != 0);
                pos_.first -= first_to_remove;
                pos_.second = (BucketSize - (rhs - pos_.second) % BucketSize) % BucketSize;
            }

            ptr_ = parent_map_[pos_.first][pos_.second];
            return *this;   
        }

        rand_access_iterator operator+(const size_type rhs) {
            auto copy = *this;
            copy += rhs;
            return copy;
        }

        rand_access_iterator operator-(const size_type rhs) {
            auto copy = *this;
            copy -= rhs;
            return copy;
        }

        bool operator==(const rand_access_iterator& rhs) const {
            return pos_ == rhs.pos_;
        }

        bool operator!=(const rand_access_iterator& rhs) const {
            return !(*this == rhs);
        }
    };

public:
    using iterator = rand_access_iterator<false>;
    using const_iterator = rand_access_iterator<true>;
    using reverse_iterator = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    iterator begin() {
        return iterator(buckets_, front_index_.first, front_index_.second);
    }

    const_iterator begin() const {
        return const_iterator(static_cast<const T**>(buckets_), front_index_.first, front_index_.second);
    }

    iterator end() {
        return iterator(buckets_, back_index_.first, back_index_.second);
    }

    const_iterator end() const {
        return const_iterator(buckets_, back_index_.first, back_index_.second);
    }

    iterator front() {
        if (size_ == 0) {
            throw std::out_of_range("back() - deque is empty");
        }

        return iterator(buckets_, front_index_.first, front_index_.second);
    }

    const_iterator front() const {
        if (size_ == 0) {
            throw std::out_of_range("front - deque is empty");
        }

        return const_iterator(static_cast<const T**>(buckets_), front_index_.first, front_index_.second);
    }

    iterator back() {
        if (size_ == 0) {
            throw std::out_of_range("back() - deque is empty");
        }

        size_type pos_first = back_index_.first;
        size_type pos_second = back_index_.second - 1;
        if (pos_second == -1) {
            pos_second = bucket_size_ - 1;
            --pos_first;
        }
        return iterator(buckets_, pos_first, pos_second);
    }

    const_iterator back() const {
        if (size_ == 0) {
            return std::out_of_range("back() - deque is empty");
        }

        size_type pos_first = back_index_.first;
        size_type pos_second = back_index_.second - 1;
        if (pos_second == -1) {
            pos_second = bucket_size_ - 1;
            --pos_first;
        }
        return const_iterator(static_cast<const T**>(buckets_), pos_first, pos_second);
    }

    Deque()
            : buckets_(nullptr)
            , size_(0)
            , cap_(0)
            , buckets_cap_(0)
            , front_index_(0, 0)
            , back_index_(0, 0) {}

    explicit Deque(const size_type cap)
            : buckets_(nullptr)
            , size_(cap)
            , cap_(cap)
            , buckets_cap_(0)
            , front_index_(0, 0)
            , back_index_(0, 0) {
        reallocate_at_back(buckets_cap_);
    }

    Deque(const size_type n, const size_type val)
            : size_(0)
            , cap_(n)
            , buckets_cap_(cap_ / bucket_size_ + 1)
            , front_index_(0, 0)
            , back_index_(size_ / bucket_size_, size_ % bucket_size_) {
        reallocate_at_back(buckets_cap_);
        for (size_type i = 0; i < n; ++i) {
            push_back(val);
        }
    }

    Deque(const Deque& other)
            : size_(other.size_)
            , cap_(other.cap_)
            , buckets_cap_(other.buckets_cap_)
            , front_index_(other.front_index_)
            , back_index_(other.back_index_) {
        reallocate_at_back(buckets_cap_);
        for (size_type i = front_index_.first; i < back_index_.first; ++i) {
            for (size_type j = front_index_.second; j < bucket_size_; ++j) {
                buckets_[i][j] = other.buckets_[i][j];
            }
        }

        for (size_type j = 0; j < back_index_.second; ++j) {
            buckets_[back_index_.first][j] = other.buckets_[back_index_.first][j];
        }
    }

    Deque& operator=(const Deque& other) {
        if (this != &other) {
            size_ = other.size_;
            cap_ = other.cap_;
            buckets_cap_ = other.buckets_cap_;
            front_index_ = other.front_index_;
            back_index_ = other.back_index_;

            reallocate_at_back(buckets_cap_);
            for (size_type i = front_index_.first; i < back_index_.first; ++i) {
                for (size_type j = front_index_.second; j < bucket_size_; ++j) {
                    buckets_[i][j] = other.buckets_[i][j];
                }
            }

            for (size_type j = 0; j < back_index_.second; ++j) {
                buckets_[back_index_.first][j] = other.buckets_[back_index_.first][j];
            }
        }

        return *this;
    }

    Deque(Deque&& other) noexcept
            : buckets_(other.buckets_)
            , size_(other.size_)
            , cap_(other.cap_)
            , buckets_cap_(other.buckets_cap_)
            , front_index_(other.front_index_)
            , back_index_(other.back_index_) {

        other.buckets_ = nullptr;
    }

    Deque& operator=(Deque&& other) noexcept {
        if (this != &other) {
            clear();

            size_ = other.size_;
            cap_ = other.cap_;
            buckets_cap_ = other.buckets_cap_;
            front_index_ = other.front_index_;
            back_index_ = other.back_index_;
            buckets_ = other.buckets_;

            other.buckets_ = nullptr;
        }

        return *this;
    }

    ~Deque() {
        clear();
    }

    void push_back(const T& val) {
        // size == cap or last bucket is filled
        if (size_ == cap_ || back_index_.first == buckets_cap_ - 1 && back_index_.second == bucket_size_ - 1) {
            reallocate_at_back(buckets_cap_ > 0 ? buckets_cap_ * 2 : 1);
        }
        buckets_[back_index_.first][back_index_.second] = val;

        ++back_index_.second;
        if (back_index_.second == bucket_size_) {
            ++back_index_.first;
            back_index_.second = 0;
        }

        ++size_;
    }

    void pop_back() {
        if (size_ == 0) {
            throw std::out_of_range("deque is empty");
        }

        alloc_.destroy(buckets_[back_index_.first] + back_index_.second);
        if (back_index_.second == 0) {
            if (size_ > 0) {
                --back_index_.first;
                back_index_.second = bucket_size_ - 1;
            }
        } else {
            --back_index_.second;
        }

        --size_;
    }

    void push_front(const T& val) {
        if (size_ == cap_ || front_index_.first == 0 && front_index_.second == 0) {
            reallocate_at_front(buckets_cap_ > 0 ? buckets_cap_ * 2 : 1);
        }

        --front_index_.second;
        if (front_index_.second == -1) {
            --front_index_.first;
            if (front_index_.first == -1) {
                front_index_.first = 0;
            }
            front_index_.second = bucket_size_ - 1;
        }
        buckets_[front_index_.first][front_index_.second] = val;

        ++size_;
    }

    void pop_front() {
        if (size_ == 0) {
            throw std::out_of_range("deque is empty");
        }

        alloc_.destroy(buckets_[front_index_.first] + front_index_.second);
        ++front_index_.second;
        if (front_index_.second == bucket_size_) {
            ++front_index_.first;
            front_index_.second = 0;
        }

        --size_;
    }

    [[nodiscard]]
    size_type size() const {
        return size_;
    }

    void clear() {
        for (size_type i = 0; i < size_ / bucket_size_; ++i) {
            for (size_type j = 0; j < bucket_size_; ++j) {
                alloc_.destroy(buckets_[i] + j);
            }

            alloc_.deallocate(buckets_[i]);
        }

        if (size_ % bucket_size_ != 0) {
            for (size_type j = 0; j < size_ % bucket_size_; ++j) {
                alloc_.destroy(buckets_[size_ / bucket_size_] + j);
            }

            alloc_.deallocate(buckets_[size_ / bucket_size_]);
        }

        size_ = 0;
        cap_ = 0;
        buckets_cap_ = 0;
        front_index_ = { 0, 0 };
        back_index_ = { 0, 0 };
    }

    const T& operator[](const size_type idx) const {
        size_type bucket = idx / bucket_size_;
        size_type bucket_idx = idx % bucket_size_;

        return buckets_[bucket][bucket_idx];
    }

    T& operator[](const size_type idx) {
        size_type bucket = idx / bucket_size_;
        size_type bucket_idx = idx % bucket_size_;

        return buckets_[bucket][bucket_idx];
    }

    const T& at(const int idx) const {
        const size_type bucket = idx / bucket_size_;
        const size_type bucket_idx = idx % bucket_size_;

        if (idx >= size_ || idx < 0) {
            throw std::out_of_range("index is out of range");
        }

        return buckets_[bucket][bucket_idx];
    }

    T& at(const int idx) {
        const size_type bucket = idx / bucket_size_;
        const size_type bucket_idx = idx % bucket_size_;

        if (idx >= size_ || idx < 0) {
            throw std::out_of_range("index is out of range");
        }

        return buckets_[bucket][bucket_idx];
    }

private:
    void reallocate_at_back(const size_type count) {
        T** new_buckets = alloc_.allocate_map(count);

        size_type i = front_index_.first;
        size_type j = 0;
        try {
            for (; i < size_ / bucket_size_; ++i) {
                for (j = front_index_.second; j < bucket_size_; ++j) {
                    alloc_.construct(new_buckets[i] + j, buckets_[i][j]);
                }
            }
            for (j = 0; j < size_ % bucket_size_; ++j) {
                alloc_.construct(new_buckets[count - 1] + j, buckets_[buckets_cap_ - 1][j]);
            }
        } catch (...) {
            for (size_type old_i = 0; old_i < i; ++old_i) {
                for (size_type old_j = 0; old_j < j; ++old_j) {
                    alloc_.destroy(new_buckets[old_i] + old_j);
                }

                alloc_.deallocate(new_buckets[old_i]);
            }

            throw std::bad_alloc();
        }

        for (i = front_index_.first; i < size_ / bucket_size_; ++i) {
            for (j = front_index_.second; j < bucket_size_; ++j) {
                alloc_.destroy(buckets_[i] + j);
            }

            alloc_.deallocate(buckets_[i]);
        }

        buckets_ = new_buckets;
        buckets_cap_ = count;
        cap_ = buckets_cap_ * bucket_size_;
    }

    void reallocate_at_front(const size_type count) {
        T** new_buckets = alloc_.allocate_map(count);

        size_type i = front_index_.first;
        size_type j = 0;
        try {
            for (; i < size_ / bucket_size_; ++i) {
                for (j = front_index_.second; j < bucket_size_; ++j) {
                    alloc_.construct(new_buckets[count - buckets_cap_ + i] + j, buckets_[i][j]);
                }
            }
            for (j = 0; j < size_ % bucket_size_; ++j) {
                alloc_.construct(new_buckets[count - 1] + j, buckets_[buckets_cap_ - 1][j]);
            }
        } catch (...) {
            for (size_type old_i = 0; old_i < i; ++old_i) {
                for (size_type old_j = 0; old_j < j; ++old_j) {
                    alloc_.destroy(new_buckets[count - buckets_cap_ + old_i] + old_j);
                }

                alloc_.deallocate(new_buckets[count - buckets_cap_ + old_i]);
            }

            throw std::bad_alloc();
        }

        for (i = 0; i < size_ / bucket_size_; ++i) {
            for (j = 0; j < bucket_size_; ++j) {
                alloc_.destroy(buckets_[i] + j);
            }

            alloc_.deallocate(buckets_[i]);
        }

        front_index_.first = count - buckets_cap_;
        back_index_.first += front_index_.first;
        buckets_ = new_buckets;
        buckets_cap_ = count;
        cap_ = buckets_cap_ * bucket_size_;
    }
};

#endif //DEQUE_HPP
