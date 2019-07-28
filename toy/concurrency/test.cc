#include "map.h"
#include "hash_table.h"
#include <iostream>
#include <time.h>
#include <map>
#include <unordered_map>

#include <thread>

template<class Map>
void test1(Map & m, const std::string name) {
    m.insert(std::make_pair(2,2));
    m.insert(std::make_pair(1,2));
    m.insert(std::make_pair(3,2));

    for (auto it = m.begin(); it != m.end(); it++) {
        std:: cout<< it-> first<<" "<< it-> second<<std::endl;
    }

    auto it = m.find(2);
    std::cout<< it-> first <<" "<< it-> second<<std::endl;

    for (auto it = m.begin(); it != m.end(); it++) {
        std:: cout<< it-> first<<" "<< it-> second<<std::endl;
    }

    std::cout<<name<<" pass test1"<<std::endl;
}

uint64_t getTime()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}

template<class Map>
void single_insert(int mid, Map & m) {
    int insert_time = 80000;
    m.insert(std::make_pair(mid,mid));
    for (int i = 0; i < insert_time; i++)
    {
        if (i % 2) {
            m.insert(std::make_pair(mid+i,mid+i));
        } else {
            m.insert(std::make_pair(mid+i,mid+i));
        }
    }
}


template<class Map>
void bench(const std::string & name) {
    Map m;

    auto begin_time = getTime();

    std::vector<std::thread> threads;
    for (int i = 0; i < 20 ; i++) {
        threads.push_back(std::thread(single_insert<Map>, i * 1000 + 500 , std::ref(m)));
    }

    for (int i = 0; i < 20; i++) {
        threads[i].join();
    }

    auto end_time = getTime();

    std::cout<< "structure " << name << " cost time : "<< end_time - begin_time << std::endl;
}

template <typename Map>
struct LockMap {
    Map m;

    std::mutex mutex_;

    using iterator = typename Map::iterator;

    std::pair<iterator, bool> insert(const typename Map::value_type & v) {
        std::lock_guard<std::mutex> lock(mutex_);
        return m.insert(v);
    }
};

int main() {
    toy::map<int, int> m;

    test1(m, "bst");
    using hash_map = toy::map<int, int, std::less<int>, toy::StepAllocator<true>, toy::HashTable<int, toy::HashMapCell<int, int, std::hash<int>> > >;
    hash_map m1;

    test1(m1, "hash table");

    bench<LockMap<std::map<int,int>>>(std::string("std::map"));

//    bench<toy::map<int, int>>(std::string("toy::map"));

    bench<hash_map>(std::string("toy::hash_map"));


}
