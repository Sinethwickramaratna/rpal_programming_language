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

// Trim whitespace from the final printed result.
// The evaluator sometimes returns strings with incidental leading or trailing
// spaces, so this keeps the final console output clean and predictable.
static inline string trim(const string &s) {
  size_t start = 0;
  while (start < s.size() && isspace((unsigned char)s[start])) ++start;
  if (start == s.size()) return string();
  size_t end = s.size() - 1;
  while (end > start && isspace((unsigned char)s[end])) --end;
  return s.substr(start, end - start + 1);
}

// Print the tree in the dotted format used by the project.
// Each indentation level is represented by an extra dot so the structure of
// the AST is easy to read in plain text output.
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

// Make a deep copy so standardization does not mutate the parser tree.
// The parser tree is preserved so the raw AST can still be printed if needed.
Node* copyTree(Node* node) {
    if (!node) return nullptr;
    Node* copy = new Node(node->label, node->value);
    for (Node* child : node->children)
        copy->addChild(copyTree(child));
    return copy;
}

// Load the source file, build the tree, and run the evaluator.
// The executable supports optional flags for printing the AST or the
// standardized tree, but the default path is the full compile-evaluate flow.
int main(int argc, char* argv[]){
  if (argc < 2){
    cerr<< "Error >> Missing filename. \n";
    cerr << "Usage: " << argv[0] << " <source_file>" << endl;
    return 1;
  }


  // The second command-line argument is the source file name when a flag is
  // present; otherwise the first argument is treated as the file name.
  string filename = argc==3? argv[2]:argv[1];

  // Read the whole source file into one string so the lexer sees the complete
  // program rather than line-by-line fragments.
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

  // Convert raw source text into the token stream expected by the parser.
  auto result = tokenize(allInput);
  bool error = result.first;
  auto tokens = result.second;

  if(!error){ 
    // Parse the token stream into an AST, then optionally display it.
    Parser p(tokens);
    try {
      Node* ast = p.parse();
      
      if (argc == 3 && string(argv[1]) == "-ast") {
          // Useful for checking the parser output before any rewriting happens.
          printTree(ast);
      }

      // Standardization rewrites the AST into the canonical form expected by
      // the CSE machine. copyTree() keeps the original parse tree untouched.
      Node* st = standardize(copyTree(ast));
      
      
      if (argc == 3 && string(argv[1]) == "-st") {
          // Useful for inspecting the rewritten tree before evaluation.
          printTree(st);
      }

      // Run the CSE machine on the standardized tree and print the final value.
      Token* result = runCSE(st);
      string out = trim(result->toString());
      if (!out.empty()) cout << out;
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

