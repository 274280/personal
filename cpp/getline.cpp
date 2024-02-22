#include<iostream>
using namespace std;
#define len1 4
int main() {
	char s1[len1],s2[len1],s3[len1];
	cout << "enter name : " << endl;
	cin.getline(s1,len1);
	cout << " enter city : "  << endl;
	cin.getline(s2,len1);
	cout << "enter profession " << endl;
	cin.getline(s3,len1);

	cout << "details are : " << endl;

	cout << "Name : " << s1 << endl;
	cout << "city : " <<s2  << endl;
	cout << "profession : " << s3 << endl;


}
