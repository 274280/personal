#include<iostream>
#include<thread>
#include<chrono>
using namespace std;

int main() {
 // using namespace std::literals::chrono_literals;
  auto start =  std::chrono::high_resolution_clock::now();
  std::this_thread::sleep_for(1s);
  auto end = std::chrono::high_resolution_clock::now();
  std::chrono::duration<float> durationn = end - start;

  std::cout<< "duration " << durationn.count() << "s" << std::endl;
}
