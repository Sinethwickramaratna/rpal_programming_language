#include <iostream>
#include <algorithm>
#include <string>
#include <vector>
#include <fstream>
#include <utility>
#include "token.h"
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

pair<bool, token> check_identifier(string input){
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
      return {false, token("Error", string("Error >> Invalid Character. Identifier can not have character '") + ch + "'.")};
    }

    state = states[state][col];

    if (state==-1){
      return {false, token("Error", string("Error >> Invalid Identifier. Identifier can not start with '") + ch + "'.")};
    }
    i+=1;
  }

  if (state == FINAL_STATE){
    return {true, token("ID", input)};
  }else{
    return {false, token("Error", "Error >> Invalid Identifier.")};
  }
}

pair <bool, token> check_integer(string input){
  if (input.empty()){
    return {false, token("Error", "Error >> Empty input for integer.")};
  }

  for (char ch : input){
    if (!isDigit(ch)){
      return {false, token("Error", string("Error >> Invalid Character. Integer can not have character '") + ch + "'.")};
    }
  }

  return {true, token("INT", input)};
}

pair <bool, token> check_operator(string input){
  if (input.empty()){
    return {false, token("Error", "Error >> Empty input for operator.")};
  }

  for (char ch: input){
    if (!isOperator(ch)){
      return {false, token("Error", string("Error >> Invalid Character. Operator can not have character '") + ch + "'.")};
    }
  }
  return {true, token("OP", input)};
}

pair<bool, token> check_string(string input) {
    // Columns: 0=quote("), 1=backslash(\), 2=valid_escape_char(t,n,\,"), 3=regular_char, 4=other
    int states[4][5] = {
        { 1, -1, -1, -1, -1},  // State 0: start, only opening quote valid
        { 3,  2, -1,  1, -1},  // State 1: inside string, await chars or closing quote
        {-1, -1,  1, -1, -1},  // State 2: after backslash, only valid escape chars
        {-1, -1, -1, -1, -1}   // State 3: final/accepting state (closed quote)
    };

    int FINAL_STATE = 3;
    int state = 0;
    int length = input.length();
    int i = 0;

    while (i < length) {
        char ch = input[i];
        int col = -1;

        if (state == 0) {
            if (ch == '\"'|| ch == '\'')         col = 0;  // opening quote
            // anything else is invalid at start

        } else if (state == 1) {
            if      (ch == '\"')    col = 0;
            else if (ch == '\'')    col = 0;
            else if (ch == '\\')    col = 1;
            else if (ch == '\n' || ch == '\r')  col = -1;  // ← explicit newline rejection
            else if (ch == '('  || ch == ')'  || ch == ';' ||
                    ch == ','  || ch == ' '  ||
                    isLetter(ch)  || isDigit(ch) || isOperator(ch))
                                    col = 3;
        } else if (state == 2) {
            // Valid escape characters after a backslash
            if (ch == 't' || ch == 'n' || ch == '\\' || ch == '\"')
                                    col = 2;  // valid escape → back to state 1
            // anything else (e.g. \x, \() is invalid
        }

        if (col == -1) {
            return {false, token("Error",
                string("Error >> Invalid String. Invalid character '") + ch + "'.")};
        }

        state = states[state][col];

        if (state == -1) {
            return {false, token("Error",
                string("Error >> Invalid String. Invalid character '") + ch + "'.")};
        }

        i++;
    }

    if (state == FINAL_STATE) {
        return {true, token("STR", input)};
    } else {
        return {false, token("Error", "Error >> Invalid String.")};
    }
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

pair <bool, token> check_punctuation(string input){
  if (input.empty()){
    return {false, token("Error", "Error>> Empty input for punctuation.")};
  }
  
  char ch = input[0];
  if (ch == '(' || ch == ')' || ch == ';' || ch == ','){
    return {true, token("PUNCT", input)};
  }
  return {false, token("Error", "Error>> Invalid punctuation.")};
}

pair <bool, token> check_keyword(string input){
  vector<string> keywords = {
    "let", "where", "true", "false", "not", "fn", "ls", "gr", "ge", 
    "aug", "le", "nil", "dummy", "or", "in", "eq", "ne", "and", 
    "rec", "within"
  };
  
  string lower_input = input;
  transform(lower_input.begin(), lower_input.end(), lower_input.begin(), ::tolower);
  
  for (const string& keyword : keywords){
    if (lower_input == keyword){
      return {true, token("KEYWORD", input)};
    }
  }
  return {false, token("Error", "Error>> Not a keyword.")};
}

pair <bool, vector<token>> tokenize(string input){
  vector<token> tokens;
  int i = 0;
  int length = input.length();
  int line = 1;
  bool error = false;

  auto append = [&](char c, string &tok){
    tok.push_back(c);
    if (c == '\n') line++;
  };

  while (i < length){
    string tok = "";
    int token_start_line = line;
    char ch = input[i];

    if (ch == ' ' || ch == '\t' || ch == '\n' || ch == '\r'){
      append(ch, tok);
      i++;
      while (i < length && (input[i] == ' ' || input[i] == '\t' || input[i] == '\n' || input[i] == '\r')){
        append(input[i], tok);
        i++;
      }
      check_whitespace(tok);
      continue;
    }

    if (ch == '/' && i + 1 < length && input[i + 1] == '/'){
      append(ch, tok);
      i++;
      while (i < length && input[i] != '\n' && input[i] != '\r'){
        append(input[i], tok);
        i++;
      }
      if (i < length){
        append(input[i], tok);
        i++;
      }
      check_comment(tok);
      continue;
    }

    // 1) Keywords / Identifiers
    if (isLetter(ch)){
      append(ch, tok);
      i++;
      while (i < length && (isLetter(input[i]) || isDigit(input[i]) || input[i] == '_')){
        append(input[i], tok);
        i++;
      }
      pair<bool, token> keyword_result = check_keyword(tok);
      if (keyword_result.first){
        tokens.push_back(keyword_result.second);
      }else{
        pair<bool, token> identifier_result = check_identifier(tok);
        if (identifier_result.first){
          tokens.push_back(identifier_result.second);
        } else {
          cerr << identifier_result.second.value << " at line " << token_start_line << endl;
          error = true;
          return make_pair(error, tokens);
        }
      }
      continue;
    }

    // 2) Integers (consume possible identifier-like tail to report single error)
    if (isDigit(ch)){
      append(ch, tok);
      i++;

      if (i < length && (input[i] == '_' || isLetter(input[i]))){
        while (i < length && (isLetter(input[i]) || isDigit(input[i]) || input[i] == '_')){
          append(input[i], tok);
          i++;
        }

        // Mixed token starting with digit (e.g. "1_x") -> produce a clearer error and return an INVALID token
        cerr << "Error >> Invalid token. Identifier cannot start with a digit: '" << tok << "' at line " << token_start_line << endl;
        error = true;
        return make_pair(error, tokens);
      }

      while (i < length && isDigit(input[i])){
        append(input[i], tok);
        i++;
      }
      pair<bool, token> integer_result = check_integer(tok);
      if (integer_result.first){
        tokens.push_back(integer_result.second);
      } else {
        cerr << integer_result.second.value << " at line " << token_start_line << endl;
        error = true;
        return make_pair(error, tokens);
      }
      continue;
    }

    // 3) Operators (skip quote characters so strings are still recognized)
    if (isOperator(ch) && ch != '"' && ch != '\''){
      append(ch, tok);
      i++;
      while (i < length && isOperator(input[i]) 
            && input[i] != '/' 
            && input[i] != '\''
            && input[i] != '"'){   // ← add this guard
          append(input[i], tok);
          i++;
      }
      pair<bool, token> operator_result = check_operator(tok);
      if (operator_result.first){
          tokens.push_back(operator_result.second);
      } else {
          cerr << operator_result.second.value << " at line " << token_start_line << endl;
          error = true;
          return make_pair(error, tokens);
      }
      continue;
    }

    // 4) Strings
    if (ch == '"' || ch == '\''){
      char delimiter = ch;
      append(ch, tok);
      i++;
      while (i < length){
          if (input[i] == '\n' || input[i] == '\r'){
              // Unclosed string literal
              cerr << "Error >> Unclosed string literal at line " << token_start_line << endl;
              error = true;
              return make_pair(error, tokens);
          }
          if (input[i] == '\\' && i + 1 < length){
              append(input[i], tok);
              i++;
              append(input[i], tok);
              i++;
          } else if (input[i] == delimiter){
              append(input[i], tok);
              i++;
              break;
          } else {
              append(input[i], tok);
              i++;
          }
      }
      pair<bool, token> string_result = check_string(tok);
      if (string_result.first){
          tokens.push_back(string_result.second);
      } else {
          cerr << string_result.second.value << " at line " << token_start_line << endl;
          error = true;
          return make_pair(error, tokens);
      }
      continue;
    }

    // 5) Punctuation
    if (ch == '(' || ch == ')' || ch == ';' || ch == ','){
      append(ch, tok);
      i++;
      pair<bool, token> punct = check_punctuation(tok);
      if (punct.first){
        tokens.push_back(punct.second);
      } else {
        cerr << punct.second.value << " at line " << token_start_line << endl;
        error = true;
        return make_pair(error, tokens);
      }
      continue;
    }

    // Unknown single character -> report and advance
    cerr << "Error >> Unknown character '" << ch << "' at line " << token_start_line << endl;
    error = true;
  }

  return make_pair(error, tokens);
}