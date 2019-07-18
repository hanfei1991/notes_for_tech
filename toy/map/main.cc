#include "map.h"
#include <iostream>

int main() {
    toy::map<int, int> m;

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
}
