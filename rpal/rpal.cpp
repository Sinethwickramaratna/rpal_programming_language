#include <iostream>
#include <algorithm>
#include <string>
#include <vector>
#include <fstream>
#include <utility>
#include "lexical_analyser.cpp"
#include "parser.cpp"
#include "standerdize.cpp"
#include "cse_machine.cpp"
using namespace std;

void printTree(Node* node,string dotes=""){
  if (node->value != ""){
    if (node->label.length()==0){
      cout<<dotes +"<" + node->value + ">"<<endl;
    }else{cout<<dotes +"<" + node->label + ":" + node->value+">"<<endl;}
  }else{
    cout<<dotes + node->label<<endl;
  }
  for (Node* child: node->children){
    printTree(child, dotes + ".");
  }
}

Node* copyTree(Node* node) {
    if (!node) return nullptr;
    Node* copy = new Node(node->label, node->value);
    for (Node* child : node->children)
        copy->addChild(copyTree(child));
    return copy;
}

int main(int argc, char* argv[]){
  if (argc < 2){
    cerr<< "Error >> Missing filename. \n";
    cerr << "Usage: " << argv[0] << " <source_file>" << endl;
    return 1;
  }


  string filename = argc==3? argv[2]:argv[1];

  ifstream inputFile(filename);
  if (!inputFile.is_open()){
    cout<<"Error: Could not open file "<<filename<<endl;
    return 1;
  }
  
  string line;
  string allInput = "";
  while (getline(inputFile, line)){
    allInput += line + "\n";
  }
  inputFile.close();

  auto result = tokenize(allInput);
  bool error = result.first;
  auto tokens = result.second;

  if(!error){ 
    Parser p(tokens);
    try {
      Node* ast = p.parse();
      
      if (argc == 3 && string(argv[1]) == "-ast") {
          printTree(ast);
      }

      Node* st = standardize(copyTree(ast));
      
      cout << "Parsing successful!" << endl;
      
      if (argc == 3 && string(argv[1]) == "-st") {
          printTree(st);
      }

      Token* result = runCSE(st);
      cout << result->toString() << endl;
      delete ast;
      delete st;

    } catch (const runtime_error& e) {
        cerr << "Runtime error: " << e.what() << endl;
        return 1;
    } catch (const exception& e) {
        cerr << "Exception: " << e.what() << endl;
        return 1;
    } catch (...) {
        cerr << "Unknown error occurred during parsing or standardization." << endl;
        return 1;
    }
  }
 
  return 0;
}
