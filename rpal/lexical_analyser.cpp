#include <iostream>
#include <algorithm>
#include <string>
#include <vector>
#include <fstream>
#include <utility>
using namespace std;

bool isDigit(char ch){
  return (ch >='0'&& ch<='9');
}

bool isLetter(char ch){
  return (ch >='A'&& ch<='Z') || (ch >='a'&& ch<='z');
}

bool isOperator(char ch){
  vector<char> operator_symbols = {
    '+', '-', '*', '<', '>', '&', '.', 
    '@', '/', ':', '=', '~', '|', '$', 
    '!', '#', '%', '^', '_', '[', ']', 
    '{', '}', '"', '\'', '?'
  };
  return find(operator_symbols.begin(), operator_symbols.end(), ch) != operator_symbols.end();
}

pair<bool, string> check_identifier(string input){
  int states[2][3] = {{1, -1, -1}, {1, 1, 1}};
  int i=0;

  int FINAL_STATE = 1;

  int length = input.length();
  int col = 0;
  int state = 0;

  while (i<length){
    char ch = input[i];

    if (isLetter(ch)){
      col = 0;
    }else if (isDigit(ch)){
      col = 1;
    }else if (ch =='_'){
      col = 2;
    }else{
      return {false, "Error >> Invalid Character. Identifier can not have character '" + string(1, ch) + "'."};
    }

    state = states[state][col];

    if (state==-1){
      return {false, string("Error >> Invalid Identifier. Identifier can not start with '") + ch + "'."};
    }
    i+=1;
  }

  if (state == FINAL_STATE){
    return {true, string("<ID: ") + input + ">"};
  }else{
    return {false, "Error >> Invalid Identifier."};
  }
}

pair <bool, string> check_integer(string input){
  if (input.empty()){
    return {false, "Error >> Empty input for integer."};
  }

  for (char ch : input){
    if (!isDigit(ch)){
      return {false, string("Error >> Invalid Character. Integer can not have character '") + ch + "'."};
    }
  }

  return {true, string("<INT: ") + input + ">"};
}

pair <bool, string> check_operator(string input){
  if (input.empty()){
    return {false, "Error >> Empty input for operator."};
  }

  for (char ch: input){
    if (!isOperator(ch)){
      return {false, string("Error >> Invalid Character. Operator can not have character '")+ ch + "'."};
    }
  }
  return {true,input};
}

pair <bool, string> check_string(string input){
  // State table:
  // state 0: expect opening quote
  // state 1: inside string
  // state 2: after backslash (escape)
  // state 3: final (after closing quote)
  int states[4][5] = {
    {1, -1, -1, -1, -1},
    {-1, 2, -1, 1, 3},
    {-1, -1, 1, -1, -1},
    {-1, -1, -1, -1, -1}
  };

  int state = 0;
  int i = 0;
  int length = input.length();
  int FINAL_STATE = 3;

  while (i < length){
    char ch = input[i];
    int col = -1;

    if (state == 0){
      if (ch == '"') col = 0;
    } else if (state == 1){
      if (ch == '\\'){
        col = 1; // escape
      } else if (ch == '"'){
        col = 4; // closing quote
      } else if (ch == '(' || ch == ')' || ch == ';' || ch == ',' || ch == ' ' ||
                 isLetter(ch) || isDigit(ch) || (isOperator(ch) && ch != '"' && ch != '\'')){
        col = 3; // regular content
      }
    } else if (state == 2){
      if (ch == 't' || ch == 'n' || ch == '\\' || ch == '\''){
        col = 2; // valid escape
      }
    }

    if (col == -1){
      return {false, string("Error >> Invalid String. Invalid character '") + ch + "'."};
    }

    state = states[state][col];
    if (state == -1){
      return {false, string("Error >> Invalid String. Invalid character '") + ch + "'."};
    }

    i++;
  }

  if (state == FINAL_STATE){
    return {true, string("<STRING: ") + input + ">"};
  }

  return {false, string("Error >> Invalid String.")};
}

bool check_whitespace(string input){
  int states[2][1] = {{1},{1}};
  int i = 0;
  int FINAL_STATE = 1;
  int state = 0;
  int length = input.length();

  if (length == 0){
    return false;
  }

  int col = 0;
  while (i < length){
    char ch = input[i];

    if(ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r'){
      col = 0;
    }else{
      return false;
    }

    state = states[state][col];

    if (state == -1){
      return false;
    }
    i++;
  }

  return state == FINAL_STATE;
}

bool check_comment(string input){
  int states[4][3] = {
    {1, -1, -1},
    {-1, 2, -1},
    {2, 2, 3},
    {-1, -1, -1}
  };
  
  int i = 0;
  int FINAL_STATE = 3;
  int state = 0;
  int length = input.length();

  while (i < length){
    char ch = input[i];
    int col = -1;

    if (state == 0 && ch == '/'){
      col = 0;
    }
    else if (state == 1 && ch == '/'){
      col = 1;
    }
    else if (state == 2){
      if (ch == '\n' || ch == '\r'){
        col = 2;
      }
      else if (ch == '\'' || ch == '(' || ch == ')' || ch == ';' || ch == ',' || 
               ch == '\\' || ch == ' ' || ch == '\t' || 
               isLetter(ch) || isDigit(ch) || isOperator(ch)){
        col = 1;
      }
    }

    if (col == -1){
      return false;
    }

    state = states[state][col];

    if (state == -1){
      return false;
    }
    i++;
  }

  return state == FINAL_STATE;
}

pair <bool, string> check_punctuation(string input){
  if (input.empty()){
    return {false, "Error>> Empty input for punctuation."};
  }
  
  char ch = input[0];
  if (ch == '(' || ch == ')' || ch == ';' || ch == ','){
    return {true, input};
  }
  return {false, "Error>> Invalid punctuation."};
}

pair <bool, string> check_keyword(string input){
  vector<string> keywords = {
    "let", "where", "true", "false", "not", "fn", "ls", "gr", "ge", 
    "aug", "le", "nil", "dummy", "or", "in", "eq", "ne", "and", 
    "rec", "within"
  };
  
  string lower_input = input;
  transform(lower_input.begin(), lower_input.end(), lower_input.begin(), ::tolower);
  
  for (const string& keyword : keywords){
    if (lower_input == keyword){
      return {true, input};
    }
  }
  return {false, "Error>> Not a keyword."};
}

pair <bool, vector<string>> tokenize(string input){
  vector<string> tokens;
  int i = 0;
  int length = input.length();
  int line = 1;
  bool error = false;

  auto append = [&](char c, string &tok){
    tok.push_back(c);
    if (c == '\n') line++;
  };

  while (i < length){
    string token = "";
    int token_start_line = line;
    char ch = input[i];

    if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r'){
      append(ch, token);
      i++;
      while (i < length && (input[i] == ' ' || input[i] == '\t' || input[i] == '\n' || input[i] == '\r')){
        append(input[i], token);
        i++;
      }
      check_whitespace(token);
      continue;
    }

    if (ch == '/' && i + 1 < length && input[i + 1] == '/'){
      append(ch, token);
      i++;
      while (i < length && input[i] != '\n' && input[i] != '\r'){
        append(input[i], token);
        i++;
      }
      if (i < length){
        append(input[i], token);
        i++;
      }
      check_comment(token);
      continue;
    }

    // 1) Keywords / Identifiers
    if (isLetter(ch)){
      append(ch, token);
      i++;
      while (i < length && (isLetter(input[i]) || isDigit(input[i]) || input[i] == '_')){
        append(input[i], token);
        i++;
      }
      pair<bool, string> keyword_result = check_keyword(token);
      if (keyword_result.first){
        tokens.push_back(keyword_result.second);
      }else{
        pair<bool, string> identifier_result = check_identifier(token);
        if (identifier_result.first){
          tokens.push_back(identifier_result.second);
        } else {
          cerr << identifier_result.second << " at line " << token_start_line << endl;
          error = true;
          continue;
        }
      }
      continue;
    }

    // 2) Integers (consume possible identifier-like tail to report single error)
    if (isDigit(ch)){
      append(ch, token);
      i++;

      if (i < length && (input[i] == '_' || isLetter(input[i]))){
        while (i < length && (isLetter(input[i]) || isDigit(input[i]) || input[i] == '_')){
          append(input[i], token);
          i++;
        }

        // Mixed token starting with digit (e.g. "1_x") -> produce a clearer error and return an INVALID token
        cerr << "Error >> Invalid token. Identifier cannot start with a digit: '" << token << "' at line " << token_start_line << endl;
        error = true;
        continue;
      }

      while (i < length && isDigit(input[i])){
        append(input[i], token);
        i++;
      }
      pair<bool, string> integer_result = check_integer(token);
      if (integer_result.first){
        tokens.push_back(integer_result.second);
      } else {
        cerr << integer_result.second << " at line " << token_start_line << endl;
        error = true;
      }
      continue;
    }

    // 3) Operators (skip quote characters so strings are still recognized)
    if (isOperator(ch) && ch != '"' && ch != '\''){
      append(ch, token);
      i++;
      while (i < length && isOperator(input[i]) && input[i] != '/' && input[i] != '\''){
        append(input[i], token);
        i++;
      }
      pair<bool, string> operator_result = check_operator(token);
      if (operator_result.first){
        tokens.push_back(operator_result.second);
      } else {
        cerr << operator_result.second << " at line " << token_start_line << endl;
        error = true;
        continue;
      }
      continue;
    }

    // 4) Strings
    if (ch == '"'){
      append(ch, token);
      i++;
      while (i < length){
        if (input[i] == '\\' && i + 1 < length){
          append(input[i], token);
          i++;
          append(input[i], token);
          i++;
        } else if (input[i] == '"'){
          append(input[i], token);
          i++;
          break;
        } else {
          append(input[i], token);
          i++;
        }
      }
      pair<bool, string> string_result = check_string(token);
      if (string_result.first){
        tokens.push_back(string_result.second);
      } else {
        cerr << string_result.second << " at line " << token_start_line << endl;
        error = true;
        continue;
      }
      continue;
    }

    // 5) Punctuation
    if (ch == '(' || ch == ')' || ch == ';' || ch == ','){
      append(ch, token);
      i++;
      pair<bool, string> punct = check_punctuation(token);
      if (punct.first){
        tokens.push_back(punct.second);
      } else {
        cerr << punct.second << " at line " << token_start_line << endl;
        error = true;
        continue;
      }
      continue;
    }

    // Unknown single character -> report and advance
    cerr << "Error >> Unknown character '" << ch << "' at line " << token_start_line << endl;
    error = true;
  }

  return make_pair(error, tokens);
}

int main(int argc, char* argv[]){
  if (argc < 2){
    cerr<< "Error >> Missing filename. \n";
    cerr << "Usage: " << argv[0] << " <source_file>" << endl;
    return 1;
  }

  string filename = argv[1];

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

  pair<bool, vector<string>> result = tokenize(allInput);
  bool error = result.first;
  vector<string> tokens = result.second;

  if(!error){ 
    for (const string& token : tokens){
      cout<<token<<endl;
    }
  }
 
  return 0;
}
