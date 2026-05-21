#include <iostream>
using namespace std;

class token{
  public:
    string type;
    string value;

    token(string t, string v){
      type = t;
      value = v;
    }
};