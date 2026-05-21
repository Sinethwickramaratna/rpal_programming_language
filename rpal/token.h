#ifndef TOKEN_CPP
#define TOKEN_CPP

#include <iostream>
#include <string>
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

#endif