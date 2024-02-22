

// C++ Program to cast
// class object to string
// object
#include <iostream>
#include <string>
using namespace std;
 
// new class
class integer {
    int x;
 
public:
    // constructor
    integer(int x_in = 0)
        : x{ x_in }
    {
        cout << "Constructor Called" << endl;
    }
 
    // user defined conversion operator to string type
    operator string()
    {
        cout << "Conversion Operator Called" << endl;
        return to_string(x);
    }
};
 
// Driver code
int main()
{
    integer obj(3);
    string str = obj;
    obj = 20;
 
    // using static_cast for typecasting
    string str2 = static_cast<string>(obj);
    obj = static_cast<integer>(30);
 
    return 0;
}