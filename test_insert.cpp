#include <iostream>
#include <vector>
#include "deque.hpp"

int main() {
    sjtu::deque<long long> dInt;
    std::vector<long long> vInt;
    for (long long i = 0; i < 10005; ++i) {
        dInt.push_back(i);
        vInt.push_back(i);
    }
    vInt.insert(vInt.begin() + 2, 2);
    dInt.insert(dInt.begin() + 2, 2);
    vInt.insert(vInt.begin() + 23, 23);
    dInt.insert(dInt.begin() + 23, 23);
    vInt.insert(vInt.begin() + 233, 233);
    dInt.insert(dInt.begin() + 233, 233);
    vInt.erase(vInt.begin() + 2333);
    dInt.erase(dInt.begin() + 2333);
    for (long long i = 0; i < vInt.size(); ++i) {
        if (*(dInt.begin() + i) != vInt[i]) {
            std::cout << "Mismatch at " << i << ": expected " << vInt[i] << ", got " << *(dInt.begin() + i) << std::endl;
            return 1;
        }
    }
    std::cout << "OK" << std::endl;
    return 0;
}
