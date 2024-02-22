#include<iostream>
#include<mutex>
#include<thread>
#include<condition_variable>
using namespace std;
condition_variable cv;
mutex m;
long amount  = 0;
void addmoney(int money) {
lock_guard<mutex> lg(m);
amount += money;
cout << "current amount- " << amount << endl;
cv.notify_one();
}
void withdraw(int money) {
    unique_lock<mutex>ul(m);
    cv.wait(ul,[] {
        return (amount!=0) ? true :false;
    });
    if(amount >= money) {
        amount -= money;
        cout <<"current amount" << amount << endl;
    } else {
        cout << "amount cant deduct" << endl;
    }
}
int main() {
    thread t1(withdraw,500);
    thread t2(addmoney,50);
    
    t1.join();
    t2.join();
    return 0;
    
}
