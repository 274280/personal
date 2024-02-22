#include<iostream>
#include<thread>
#include<chrono>
using namespace std;
struct timer {
  //using namespace std::literals::chrono_literals;
	std::chrono::time_point<std::chrono::system_clock> start,end;
	std::chrono::duration<float> duration;
  float ms;
  timer() {
    start = std::chrono::high_resolution_clock::now();
  }

  ~timer() {
    end = std::chrono::high_resolution_clock::now();
    duration = end - start;
    ms = duration.count()*1000.0f;
    cout << "Time_duration :" << ms << "ms\n";
  }
};






int main() {
 // using namespace std::literals::chrono_literals;
  timer time;
  int i;
  for(i = 0;i<10;i++) {
    cout << "hello\n";
  }

}
