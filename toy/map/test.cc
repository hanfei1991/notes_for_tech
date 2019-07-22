#include "map.h"
#include "hash_table.h"
#include <iostream>

template<class Container>
void test1(Container & m, const std::string name) {
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

int main() {
    toy::map<int, int> m;

    test1(m, "bst");

    toy::map<int, int, std::less<int>, toy::StepAllocator<true>, toy::HashTable<int, toy::HashMapCell<int, int, std::hash<int>> > > m1;

    test1(m1, "hash table");

}
