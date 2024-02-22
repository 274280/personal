#include <thread>
#include <iostream>
#include <vector>
#include <numeric>
#include <future>

int main() {
    std::future<std::vector<int>> iotaFuture = std::async(std::launch::async, 
         [startArg = 1]() {
            std::vector<int> numbers(25);
            std::iota(numbers.begin(), numbers.end(), startArg);
            std::cout << "calling from: " << std::this_thread::get_id() << " id\n";
           // std::cout << numbers.data() << '\n';
            return numbers;
        }
    );

    auto vec = iotaFuture.get(); // make sure we get the results...
    //std::cout << vec.data() << '\n';
    std::cout << "printing in main (id " << std::this_thread::get_id() << "):\n";
    for (auto& num : vec)
        std::cout << num << ", ";
    std::cout << '\n';
    
    
    std::future<int> sumFuture = std::async(std::launch::async, [&vec]() {
        const auto sum = std::accumulate(vec.begin(), vec.end(), 0);
        std::cout << "accumulate in: " << std::this_thread::get_id() << " id\n";
        return sum;
    });
    
    const auto sum = sumFuture.get();
    std::cout << "sum of numbers is: " << sum;
    
    return 0;
}