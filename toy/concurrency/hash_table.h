#include "alloc.h"
#include <cstring>

#include <condition_variable>
#include <mutex>
#include <atomic>
#include <iostream>
#include  <shared_mutex>

namespace toy {

namespace ZeroTraits
{

template <typename T>
bool check(const T x) { return x == 0; }

template <typename T>
void set(T & x) { x = 0; }
}

template <typename Cell>
struct ZeroStorage {
    bool has_zero = false;
    Cell zero_storage;
};

template <size_t initial_size_degree = 8>
struct HashTableGrower
{

    /// The state of this structure is enough to get the buffer size of the hash table.

    int8_t size_degree = initial_size_degree;

    /// The size of the hash table in the cells.
    size_t bufSize() const               { return 1ULL << size_degree; }

    size_t maxFill() const               { return 1ULL << (size_degree - 1); }
    size_t mask() const                  { return bufSize() - 1; }

    /// From the hash value, get the cell number in the hash table.
    size_t place(size_t x) const         { return x & mask(); }

    /// The next cell in the collision resolution chain.
    size_t next(size_t pos) const        { ++pos; return pos & mask(); }

    /// Whether the hash table is sufficiently full. You need to increase the size of the hash table, or remove something unnecessary from it.
    bool overflow(size_t elems) const    { return elems > maxFill(); }

    /// Increase the size of the hash table.
    void increaseSize()
    {
        size_degree += size_degree >= 23 ? 1 : 2;
    }

};

template <typename Key, typename TMapped, typename Hash_ = std::hash<Key>>
struct HashMapCell
{
    using Mapped = TMapped;

    using Hash = Hash_;

    using value_type = std::pair<Key, Mapped>;

    value_type value;

    mutable std::shared_mutex mutex_;

    std::atomic_bool inserting = false;

    HashMapCell() {}
    HashMapCell(const value_type & value_) : value(value_), inserting(false) {}

    value_type & getValue() { 
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return value; 
    }
    const value_type & getValue() const { 
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return value; 
    }

    static Key & getKey(value_type & value) { return value.first; }
    static const Key & getKey(const value_type & value) { return value.first; }

    bool keyEquals(const Key & key_) const {
        std::shared_lock <std::shared_mutex> lock(mutex_);
        return value.first == key_; 
    }

    size_t getHash(const Hash & hash) const { 
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return hash(value.first); 
    }

    bool isZero() const {
        std::shared_lock<std::shared_mutex> lock(mutex_);
        return ZeroTraits::check(value.first);
    }

    static bool isZero(const Key & key) { return ZeroTraits::check(key); }

    bool isInsertable() const {return isZero();}

    bool getInsertLock() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        if (inserting.load() == false && ZeroTraits::check(value.first)) {
            inserting.store(true);
            return true;
        }
        return false;
    }

    void setValue(const value_type & value_) {
        new (&value) value_type(value_);
        inserting.store(false);
    }

    /// Set the key value to zero.
    void setZero() {
        std::unique_lock<std::shared_mutex> lock(mutex_);
        ZeroTraits::set(value.first);
    }
};

template <class Container_, typename Cell, bool is_const>
class iterator_base
{
    using Container = std::conditional_t<is_const, const Container_, Container_>;
    using cell_type = std::conditional_t<is_const, const Cell, Cell>;

    using Self = iterator_base<Container_, Cell, is_const>;

    Container * container;
    cell_type * ptr;

public:
    iterator_base() {}
    iterator_base(Container * container_, cell_type * ptr_) : container(container_), ptr(ptr_) {}

    bool operator== (const iterator_base & rhs) const { return ptr == rhs.ptr; }
    bool operator!= (const iterator_base & rhs) const { return ptr != rhs.ptr; }

    Self & operator++()
    {
        if (ptr->isZero())
            ptr = container->buf;
        else
            ++ptr;

        while (ptr < container->buf + container->grower.bufSize() && ptr->isInsertable())
            ++ptr;

        return (*this);
    }

    Self operator ++(int)
    {
        Self ret = *this;
        ++(*this);
        return ret;
    }

    auto & operator* () const { return ptr->getValue(); }
    auto * operator->() const { return &ptr->getValue(); }
};

template<typename Key, typename Cell, typename Grower = HashTableGrower<>, typename Allocator = StepAllocator<true>>
class HashTable : public ZeroStorage<Cell>, public Allocator
{

    using Hash = typename Cell::Hash;

    using Self = HashTable<Key, Cell, Grower, Allocator>;


public:
    class iterator : public iterator_base<Self, Cell, false> {
    public:
        using iterator_base<Self, Cell, false>::iterator_base;
    };

    class const_iterator : public iterator_base<Self, Cell, true> {
    public:
        using iterator_base<Self, Cell, true>::iterator_base;
    };

    friend class iterator;
    friend class const_iterator;

private:
    void reinsert(const Cell & x, size_t hash_value, Cell * cur_buf)
    {
        size_t place_value = grower.place(hash_value);

        /// Compute a new location, taking into account the collision resolution chain.
        auto [ result_place_value,  empty] = findCell<false>(Cell::getKey(x.getValue()), place_value, cur_buf);

        /// If the item remains in its place in the old collision resolution chain.
        if (!empty)
            return;

        /// Copy to a new location and zero the old one.
        cur_buf[result_place_value].setValue(x.getValue());

    }

    std::condition_variable cv;

    bool resize()
    {
        std::unique_lock<std::mutex> lock(resize_mutex);

        cv.wait(lock, [this]{return insert_thread.load() == 0;});

        size_t old_size = grower.bufSize();

        if (!grower.overflow(size)) {
            return false;
        }

        //std::cout<<"resize!"<<std::endl;

        /** In case of exception for the object to remain in the correct state,
          *  changing the variable `grower` (which determines the buffer size of the hash table)
          *  is postponed for a moment after a real buffer change.
          * The temporary variable `new_grower` is used to determine the new size.
          */
        Grower new_grower = grower;

        new_grower.increaseSize();

        /// Expand the space.
        Cell* new_buf = reinterpret_cast<Cell *>(Allocator::alloc(new_grower.bufSize() * sizeof(Cell)));

        /** Now some items may need to be moved to a new location.
          * The element can stay in place, or move to a new location "on the right",
          *  or move to the left of the collision resolution chain, because the elements to the left of it have been moved to the new "right" location.
          */
        //std::cout<<"before insert\n";
        size_t i = 0;
       // std::cout<<"old: "<<old_size<<std::endl;
        Cell * old_buf = buf.load();
        for (; i < old_size; ++i)
            if (!old_buf[i].isZero())
                reinsert(old_buf[i], old_buf[i].getHash(hash), new_buf);
        buf.store(new_buf);
        Allocator::free(old_buf, getBufferSizeInBytes());
        grower = (new_grower);

        return true;
    }


public:
    Grower grower;
    std::atomic<Cell *> buf;
private:

    std::mutex resize_mutex;

    Hash hash;

    std::atomic_int insert_thread;

    std::atomic_int  size;

    using value_type = typename Cell::value_type;

    void emplaceNonZero(const value_type & value, iterator & it, bool & insert, size_t hash_value) {
        {
            std::lock_guard<std::mutex> lock(resize_mutex);
            insert_thread.fetch_add(1);
        }
        const Key & key = Cell::getKey(value);
        auto [place,  empty] = findCell<true>(key, grower.place(hash_value), buf);
        it = iterator(this, &buf[place]);
        if (!empty) {
            insert = false;
            insert_thread.fetch_sub(1);
            cv.notify_all();
            return;
        }
        buf[place].setValue(value);
        insert = true;

        size.fetch_add(1);

        insert_thread.fetch_sub(1);
        cv.notify_all();
        //std::cout<<"size: "<<size<<std::endl;
        if (grower.overflow(size)) {
            //std::cout<<"try resize\n";
            if(resize())
                it = find(key);
            //std::cout<<grower.bufSize()<<std::endl;
        }
    }

    bool emplaceIfZero(const value_type & value, iterator & it, bool & insert, size_t ) {
        const Key & key = Cell::getKey(value);
        if (!Cell::isZero(key)) {
            return false;
        }
        if (this->has_zero) {
            insert = false;
            it = iterator(this, &this->zero_storage);
        } else {
            insert = true;
            this->has_zero = true;
            new(&this->zero_storage) Cell(value);
            it = iterator(this, &this->zero_storage);
        }
        return true;
    }

    /// Find a cell with the same key or an empty cell, starting from the specified position and further along the collision resolution chain.
    template<bool insert>
    std::pair<size_t, bool> findCell(const Key & x, size_t place_value, Cell * cur_buf) const
    {
        for (;;) {
            while (!cur_buf[place_value].isZero() && !cur_buf[place_value].keyEquals(x))
            {
                place_value = grower.next(place_value);
//                std::cout<<place_value<<std::endl;
            }

            bool empty = cur_buf[place_value].isZero();

            if constexpr (insert) {
                if (empty && !cur_buf[place_value].getInsertLock()) {
                    //std::cout<<"conflict\n";
                    place_value = grower.next(place_value);
                    continue;
                }
            }

            //std::cout<<"return: "<< place_value<<std::endl;

            return std::make_pair(place_value, empty);
        }
    }

    const_iterator iteratorTo(const Cell * ptr) const { return const_iterator(this, ptr); }
    iterator iteratorTo(Cell * ptr)                   { return iterator(this, ptr); }
    const_iterator iteratorToZero() const             { return iteratorTo(&this->zero_storage); }
    iterator iteratorToZero()                         { return iteratorTo(&this->zero_storage); }

    void alloc(const Grower & new_grower)
    {
        buf = reinterpret_cast<Cell *>(Allocator::alloc(new_grower.bufSize() * sizeof(Cell)));
        grower = new_grower;
    }

    size_t getBufferSizeInBytes() const
    {
        return grower.bufSize() * sizeof(Cell);
    }


public:

    HashTable()
    {
        this->has_zero = false;
        size = 0;
        insert_thread = 0;
        alloc(grower);
    }

    /// Insert a value. In the case of any more complex values, it is better to use the `emplace` function.
    std::pair<iterator, bool> insert_unique(const value_type & x)
    {
        std::pair<iterator, bool> res;

        size_t hash_value = hash(Cell::getKey(x));
        if (!emplaceIfZero(x, res.first, res.second, hash_value))
            emplaceNonZero(x, res.first, res.second, hash_value);

        return res;
    }

    bool erase(const Key & key)
    {
        throw "";
    }

    iterator find(const Key & x)
    {
        std::lock_guard<std::mutex> lock(std::mutex);
        if (Cell::isZero(x))
            return this->has_zero ? iteratorToZero() : end();

        size_t hash_value = hash(x);
        size_t place_value; bool empty;
        std::tie(place_value, empty) = findCell<false>(x, grower.place(hash_value), buf);
        return !buf[place_value].isZero() ? iterator(this, &buf[place_value]) : end();
    }

    const_iterator find(const Key & x) const
    {
        std::lock_guard<std::mutex> lock(std::mutex);
        if (Cell::isZero(x))
            return this->has_zero ? iteratorToZero() : end();

        size_t hash_value = hash(x);
        size_t place_value; bool empty;
        std::tie(place_value, empty) = findCell<false>(x, grower.place(hash_value), buf);
        return empty ? const_iterator(this, &buf[place_value]) : end();
    }

   const_iterator begin() const
    {
        if (!buf)
            return end();

        if (this->has_zero)
            return iteratorToZero();

        const Cell * ptr = buf;
        while (ptr < buf + grower.bufSize() && ptr->isInsertable())
            ++ptr;

        return const_iterator(this, ptr);
    }

    iterator begin()
    {
        if (!buf)
            return end();

        if (this->has_zero)
            return iteratorToZero();

        Cell * ptr = buf;
        while (ptr < buf + grower.bufSize() && ptr->isInsertable()) {
            ++ptr;
        }

        return iterator(this, ptr);
    }

    const_iterator end() const         { return const_iterator(this, buf + grower.bufSize()); }
    iterator end()                     { return iterator(this, buf + grower.bufSize()); }

};

}
