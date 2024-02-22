#include<iostream>
#include<thread>
#include<mutex>
#include<condition_variable>
#include<future>


using namespace std;


void sumofnum(std::promise<int> &&promise_obj,int a, int b) {
	int sum;
	for(int i=a;i<=b;i++) {
		sum = sum+i;
	}
	promise_obj.set_value(sum);
}



int main(int argc,char **argv) {
	std::promise<int> promise_obj;
	std::future<int> future_obj = promise_obj.get_future();
	int a = stoi(argv[1]);
	int b = stoi(argv[2]);
	
	std::thread t1(sumofnum,std::move(promise_obj),a,b);
	cout << "thread is created " <<endl;
	cout << "waiting fort result" << endl;
	
	cout << "sum is : " << future_obj.get() << endl;

	t1.join();
}
