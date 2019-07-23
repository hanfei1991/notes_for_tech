#include "map.h"
#include "hash_table.h"
#include <iostream>
#include <time.h>
#include <map>
#include <unordered_map>

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

    m.erase(2);
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
void bench(const std::string & name) {
    Map m;

    auto begin_time = getTime();

    int insert_time = 30000;
    for (int i = 0; i < insert_time; i++)
    {
        m.insert(std::make_pair(i, i));
    }

    auto end_time = getTime();

    std::cout<< "structure " << name << " cost time : "<< end_time - begin_time << std::endl;
}

int main() {
    toy::map<int, int> m;

    test1(m, "bst");
    using hash_map = toy::map<int, int, std::less<int>, toy::StepAllocator<true>, toy::HashTable<int, toy::HashMapCell<int, int, std::hash<int>> > >;
    hash_map m1;

    test1(m1, "hash table");

    bench<std::map<int,int>>(std::string("std::map"));

    bench<hash_map>(std::string("hash table"));

    bench<std::unordered_map<int,int>>(std::string("std::unordered_map"));

}
