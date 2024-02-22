#include <iostream>
#include  <memory> 
using namespace std;

template<Typename T>
class base {
    public:
    T a,b;
    base() {
         cout << "base  constructor" << endl;
    }
};
class derived : public base<T> {
    public:
    int c,d;
    derived() {
        cout << "derived constructor" << endl;
    }
};
int main()
{   
    //derived d1;
    std::shared_ptr<derived> ptr1 = std::make_shared<derived>();
    std::shared_ptr<base<int>> ptr = std::make_shared<derived<int>>;
    std::unique_ptr<int> u1(new int(20));
//std::unique_ptr<int> u2(u1);   //
    std::shared_ptr<int> p1 = std::make_shared<int>();
    *p1 = 78;
    std::cout << "p1 = " << *p1 << std::endl;
    std::cout << "p1 Reference count = " << p1.use_count() << std::endl;
    std::shared_ptr<int> p2(p1);
    std::cout << "p2 Reference count = " << p2.use_count() << std::endl;
    std::cout << "p1 Reference count = " << p1.use_count() << std::endl;
        if (p1 == p2)
    {
        std::cout << "p1 and p2 are pointing to same pointer\n";
    }
    std::cout<<"Reset p1 "<<std::endl;
    p1.reset();
    std::cout << "p1 Reference Count = " << p1.use_count() << std::endl;
    p1.reset(new int(11));
    std::cout << "p1  Reference Count = " << p1.use_count() << std::endl;
    std::cout << "p2 Reference count = " << p2.use_count() << std::endl;
    p1 = nullptr;
    std::cout << "p1  Reference Count = " << p1.use_count() << std::endl;
    if (!p1)

    {
        std::cout << "p1 is NULL" << std::endl;
    }
    return 0;
}