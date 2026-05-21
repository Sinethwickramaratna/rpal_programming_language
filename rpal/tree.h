#ifndef AST_CPP
#define AST_CPP

#include<iostream>
#include<string>
#include<vector>
using namespace std;

struct Node{
  string label;
  string value;
  vector<Node*> children;

  Node(const string& lbl, const string& val = "") : label(lbl), value(val) {}
  ~Node() {
    for (Node* child : children) {
      delete child;
    }
  }

  void addChild(Node* child) {
    children.push_back(child);
  }
};

#endif