#include<iostream>
#include<chrono>
#include<thread>
#include<future>
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

int sum(int a,int b) {
    //timer time;
    int c = a+b;
    return c;
}
int diff(int a, int b) {
    //timer time;
    int c = b-a;
    return c;
}
int main() {
    timer time;
    int a=1,b=2,c,d;
    //std::future<int> c = std::async(std::launch::async,sum,a,b);
c = sum(a,b);
d = diff(a,b);
}