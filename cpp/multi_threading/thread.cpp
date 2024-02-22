#include<iostream>
#include<thread>

using namespace std;

class threading {
	public:

	threading() {
		cout << "threading constructor" << endl;
	}

	void fun1() {
		cout << "first function calling" << endl;
	}
	void fun2() {
		cout << "second function calling " << endl;
	}
};
int main() {
	
	threading t1;
	std::thread f1(&threading::fun1,threading());
	std::thread f2(&threading::fun2,threading());

	cout << "tthreaaaaad in main function " << endl;

	f1.join();
	//f2.join();

	cout << "end of main thread " << endl;

}
