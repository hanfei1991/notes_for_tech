#include "alloc.h"
#include <cstring>

#include <iostream>

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
        //std::cout<<"increase: "<<size_degree<<std::endl;
    }

    /// Set the buffer size by the number of elements in the hash table. Used when deserializing a hash table.
};

template <typename Key, typename TMapped, typename Hash_ = std::hash<Key>>
struct HashMapCell
{
    using Mapped = TMapped;

    using Hash = Hash_;

    using value_type = std::pair<Key, Mapped>;

    value_type value;

    bool deleted = false;

    HashMapCell() {}
    HashMapCell(const value_type & value_) : value(value_), deleted(false) {}

    value_type & getValue() { return value; }
    const value_type & getValue() const { return value; }

    static Key & getKey(value_type & value) { return value.first; }
    static const Key & getKey(const value_type & value) { return value.first; }

    bool keyEquals(const Key & key_) const { return value.first == key_; }

    size_t getHash(const Hash & hash) const { return hash(value.first); }

    bool isZero() const { return ZeroTraits::check(value.first); }

    static bool isZero(const Key & key) { return ZeroTraits::check(key); }

    bool isInsertable() const {return isZero() || isDeleted();}

    /// Set the key value to zero.
    void setZero() { 
        ZeroTraits::set(value.first); 
        deleted = false;
    }

    /// Do I need to store the zero key separately (that is, can a zero key be inserted into the hash table).
    static constexpr bool need_zero_value_storage = true;

    /// Whether the cell was deleted.
    bool isDeleted() const { return deleted;}

    void setDeleted() {deleted = true;}

    void setMapped(const value_type & value_) { value.second = value_.second; }
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
    void reinsert(Cell & x, size_t hash_value)
    {
        size_t place_value = grower.place(hash_value);

        /// If the element is in its place.
        if (&x == &buf[place_value])
            return;

        /// Compute a new location, taking into account the collision resolution chain.
        place_value = findCell(Cell::getKey(x.getValue()), place_value);

        /// If the item remains in its place in the old collision resolution chain.
        if (!buf[place_value].isZero())
            return;

        /// Copy to a new location and zero the old one.
        memcpy(&buf[place_value], &x, sizeof(x));
        x.setZero();

        /// Then the elements that previously were in collision with this can move to the old place.
    }


    void resize(size_t for_num_elems = 0, size_t for_buf_size = 0)
    {

        size_t old_size = grower.bufSize();

        /** In case of exception for the object to remain in the correct state,
          *  changing the variable `grower` (which determines the buffer size of the hash table)
          *  is postponed for a moment after a real buffer change.
          * The temporary variable `new_grower` is used to determine the new size.
          */
        Grower new_grower = grower;

        new_grower.increaseSize();

        /// Expand the space.
        buf = reinterpret_cast<Cell *>(Allocator::realloc(buf, getBufferSizeInBytes(), new_grower.bufSize() * sizeof(Cell)));
        grower = new_grower;

        /** Now some items may need to be moved to a new location.
          * The element can stay in place, or move to a new location "on the right",
          *  or move to the left of the collision resolution chain, because the elements to the left of it have been moved to the new "right" location.
          */
        //std::cout<<"before insert\n";
        size_t i = 0;
        for (; i < old_size; ++i)
            if (!buf[i].isZero() && !buf[i].isDeleted())
                reinsert(buf[i], buf[i].getHash(hash));

        //std::cout<<"mid insert\n";
        for (; !buf[i].isZero() && !buf[i].isDeleted(); ++i)
            reinsert(buf[i], buf[i].getHash(hash));
        //std::cout<<"end insert\n";
    }


// FIXME:: friend class does not work in gcc :(
public:
    Grower grower;
    Cell * buf;
private:

    Hash hash;

    size_t size;

    using value_type = typename Cell::value_type;

    void emplaceNonZero(const value_type & value, iterator & it, bool & insert, size_t hash_value) {
        const Key & key = Cell::getKey(value);
        size_t place = findCell(key, grower.place(hash_value));
        it = iterator(this, &buf[place]);
        if (!buf[place].isInsertable()) {
            insert = false;
            return;
        }
        new(&buf[place]) Cell(value);
        insert = true;
        size ++;

        //std::cout<<"size: "<<size<<std::endl;
        if (grower.overflow(size)) {
            resize();

            it = find(key);
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
    size_t findCell(const Key & x, size_t place_value) const
    {
        int64_t first_deleted_place = -1;
        while (!buf[place_value].isZero() )
        {
            if(!buf[place_value].isDeleted() && buf[place_value].keyEquals(x)) {
                first_deleted_place = -1;
                break;
            }
            if (buf[place_value].isDeleted() && first_deleted_place == -1)
                first_deleted_place = place_value;
            place_value = grower.next(place_value);
        }

        if (first_deleted_place == -1)
            return place_value;
        else
            return (size_t)first_deleted_place;
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
        size_t hash_value = hash(key);
        size_t place = findCell(key, grower.place(hash_value));
        if(buf[place].isZero() || buf[place].isDeleted()) {
            return false;
        }
        size_t next_place = grower.next(place);
        if (buf[next_place].isZero())
            buf[place].setZero();
        else
            buf[place].setDeleted();

        size--;

        return true;
    }

    iterator find(const Key & x)
    {
        if (Cell::isZero(x))
            return this->has_zero ? iteratorToZero() : end();

        size_t hash_value = hash(x);
        size_t place_value = findCell(x, grower.place(hash_value));
        return !buf[place_value].isZero() ? iterator(this, &buf[place_value]) : end();
    }

    const_iterator find(const Key & x) const
    {
        if (Cell::isZero(x))
            return this->has_zero ? iteratorToZero() : end();

        size_t hash_value = hash(x);
        size_t place_value = findCell(x, grower.place(hash_value));
        return !buf[place_value].isZero() ? const_iterator(this, &buf[place_value]) : end();
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
