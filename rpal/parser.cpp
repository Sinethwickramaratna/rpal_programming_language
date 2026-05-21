#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include "token.cpp"
using namespace std;

struct ASTNode{
  string label;
  string value;
  vector<ASTNode*> children;

  ASTNode(const string& lbl, const string& val = "") : label(lbl), value(val) {}
  ~ASTNode() {
    for (ASTNode* child : children) {
      delete child;
    }
  }

  void addChild(ASTNode* child) {
    children.push_back(child);
  }
};

class Parser{
  public:
    explicit Parser(const vector<token> tokens) : tokens_(move(tokens)), pos_(0) {}
  
    ASTNode* parse(){
      ASTNode* tree = E();
      if(!isEnd()){
        throw runtime_error("Parse error>> Unexpected token '" + peek().value + "' after end of expression.");
      }
      return tree;
    }
  
  private:
    vector<token> tokens_;
    size_t pos_;

    token& peek(){
      return tokens_[pos_];
    }

    token consume(){
      return tokens_[pos_++];
    }

    bool isEnd(){
      return pos_ >= tokens_.size();
    }

    bool checkType(const string& type){
      return !isEnd() && peek().type == type;
    }

    bool checkValue(const string& value){
      return !isEnd() && peek().value == value;
    }

    token expectValue(const string& value){
      if (!checkValue(value)){
        throw runtime_error("Parse error>> Expected '" + value + "' but found '" + (isEnd() ? "EOF" : peek().value) + "'.");
      }
      return consume();
    }

    token expectType(const string& type){
      if (!checkType(type)){
        throw runtime_error("Parse error>> Expected token of type '" + type + "' but found '" + (isEnd() ? "EOF" : peek().type) + "'.");
      }
      return consume();
    }

    bool inFirstVb(){
      return checkType("ID") || checkValue("(");
    }

    bool inFirstRn(){
      return checkType("ID")  || checkType("INT")  || checkType("STRING") ||
              checkValue("true") || checkValue("false") ||
              checkValue("nil")  || checkValue("dummy") ||
              checkValue("(");
    }

    ASTNode* E(){
      if (checkValue("let")){
        consume();
        ASTNode* def_Node = D();

        expectValue("in");

        ASTNode* body_Node = E();

        ASTNode* let_Node = new ASTNode("let");
        let_Node->addChild(def_Node);
        let_Node->addChild(body_Node);
        return let_Node;
      }

      if(checkValue("fn")){
        consume();
        
        if (!inFirstVb()){
          throw runtime_error("Parse error>> Expected identifier or '(' after 'fn' but found '" + (isEnd() ? "EOF" : peek().value) + "'.");
        }

        vector<ASTNode*> vbs;
        do {
          vbs.push_back(Vb());
        } while(inFirstVb());

        expectValue(".");

        ASTNode* body_Node = E();
        ASTNode* result = body_Node;
        for (int i = (int)vbs.size() - 1; i >= 0; i--){
          ASTNode* lambda_Node = new ASTNode("lambda");
          lambda_Node->addChild(vbs[i]);
          lambda_Node->addChild(result);
          result = lambda_Node;
        }

        return result;
      }

      return Ew();
    }
    
    ASTNode* Ew(){
      ASTNode* t_node = T();

      if(checkValue("where")){
        consume();
        ASTNode* dr_node = Dr();

        ASTNode* where_node = new ASTNode("where");
        where_node->addChild(t_node);
        where_node->addChild(dr_node);
        return where_node;
      }
      return t_node;
    }

    ASTNode* T(){
      ASTNode* ta_node = Ta();

      if (!checkValue(",")){
        return ta_node;
      }

      ASTNode* tau_node = new ASTNode("tau");
      tau_node->addChild(ta_node);

      while (checkValue(",")){
        consume();
        tau_node->addChild(Ta());
      }

      return tau_node;
    }

    ASTNode* Ta(){
      ASTNode* node = Tc();

      while (checkValue("aug")){
        consume();
        ASTNode* right = Tc();

        ASTNode* aug_node = new ASTNode("aug");
        aug_node->addChild(node);
        aug_node->addChild(right);
        node = aug_node;
      }
      return node;
    }

    ASTNode* Tc(){
      ASTNode* b_node = B();

      if (checkValue("->")){
        consume();
        ASTNode* then_node = Tc();

        expectValue("|");

        ASTNode* else_node = Tc();
        
        ASTNode* cond_node = new ASTNode("->");
        cond_node->addChild(b_node);
        cond_node->addChild(then_node);
        cond_node->addChild(else_node);

        return cond_node;
      }

      return b_node;
    }

    ASTNode* B(){
      ASTNode* node = Bt();

      while(checkValue("or")){
        consume();
        ASTNode* right = Bt();

        ASTNode* or_node = new ASTNode("or");
        or_node->addChild(node);
        or_node->addChild(right);

        node = or_node;
      }

      return node;
    }

    ASTNode* Bt(){
      ASTNode* node = Bs();

      while(checkValue("and")){
        consume();
        ASTNode* right = Bs();

        ASTNode* and_node = new ASTNode("and");
        and_node->addChild(node);
        and_node->addChild(right);

        node = and_node;
      }
      return node;
    }

    ASTNode* Bs(){
      if(checkValue("not")){
        consume();
        ASTNode* not_node = new ASTNode("not");
        not_node->addChild(Bp());
        return not_node;
      }
      return Bp();
    }

    ASTNode* Bp(){
      ASTNode* left = A();

      string rel="";
      if (checkValue("gr")||checkValue(">")) rel="gr";
      else if (checkValue("ge")||checkValue(">=")) rel="ge";
      else if (checkValue("ls")||checkValue("<")) rel="ls";
      else if (checkValue("le")||checkValue("<=")) rel="le";
      else if (checkValue("eq")) rel="eq";
      else if (checkValue("ne")) rel="ne";

      if (!rel.empty()){
        consume();
        ASTNode* right = A();

        ASTNode* rel_node = new ASTNode(rel);
        rel_node->addChild(left);
        rel_node->addChild(right);
        return rel_node;
      }
      return left;
    }

    ASTNode* A(){
      if(checkValue("+")){
        consume();
        return At();
      }

      if(checkValue("-")){
        consume();
        ASTNode* neg_node = new ASTNode("neg");
        neg_node->addChild(At());
        return neg_node;
      }


      ASTNode* node = At();
      
      while(checkValue("+")||checkValue("-")){
        string op = consume().value;
        ASTNode* right = At();

        ASTNode* op_node = new ASTNode(op);
        op_node->addChild(node);
        op_node->addChild(right);
        node = op_node;
      }
      return node;
    }

    ASTNode* At(){
      ASTNode* node = Af();

      while(checkValue("*")||checkValue("/")){
        string op = consume().value;

        ASTNode* right = Af();
        
        ASTNode* op_node = new ASTNode(op);
        op_node -> addChild(node);
        op_node -> addChild(right);
        node = op_node;
      }
      return node;
    }

    ASTNode* Af(){
      ASTNode* ap_node = Ap();
      if (checkValue("**")){
        consume();
        ASTNode* right = Af();

        ASTNode* power_node = new ASTNode("**");
        power_node -> addChild(ap_node);
        power_node -> addChild(right);

        return power_node;
      }

      return ap_node;
    }

    ASTNode* Ap(){
      ASTNode* node = R();

      while(checkValue("@")){
        consume();

        token id_tok = expectType("ID");
        ASTNode* idNode = new ASTNode("ID", id_tok.value);

        ASTNode* r_node = R();

        ASTNode* at_node = new ASTNode("@");
        at_node->addChild(node);
        at_node->addChild(idNode);
        at_node->addChild(r_node);
        node = at_node;
      }
      return node;
    }

    ASTNode* R(){
      ASTNode* node = Rn();

      while (inFirstRn()){
        ASTNode* right = Rn();

        ASTNode* gamma_node = new ASTNode("gamma");
        gamma_node->addChild(node);
        gamma_node->addChild(right);
        node = gamma_node;
      }

      return node;
    }

    ASTNode* Rn(){
      if (checkType("ID")){
        token t = consume();
        return new ASTNode("ID", t.value);
      }

      if (checkType("STR")){
        token t = consume();
        return new ASTNode("STR", t.value);
      }

      if (checkType("INT")){
        token t = consume();
        return new ASTNode("INT",t.value);
      }

      if (checkValue("true")){
        consume();
        return new ASTNode("true");
      }

      if (checkValue("false")){
        consume();
        return new ASTNode("false");
      }

      if (checkValue("nil")){
        consume();
        return new ASTNode("nill");
      }

      if (checkValue("dummy")){
        consume();
        return new ASTNode("dummy");
      }

      if (checkValue("(")){
        consume();
        ASTNode* e_node = E();
        expectValue(")");
        return e_node;
      }

      throw runtime_error("Parse erro >> Unexpected token '"+(isEnd()? "EOF": peek().value)+"'.");
    }

    ASTNode* D(){
      ASTNode* da_node = Da();

      if(checkValue("within")){
        consume();
        ASTNode* d_node = D();

        ASTNode* within_node = new ASTNode("within");
        within_node->addChild(da_node);
        within_node->addChild(d_node);

        return within_node;
      }

      return da_node;
    }

    ASTNode* Da(){
      ASTNode* node = Dr();
      if(!checkValue("and")) return node;

      ASTNode* and_node = new ASTNode("and");
      while(checkValue("and")){
        consume();
        and_node->addChild(Dr());
      }
      return and_node;
    }

    ASTNode* Dr(){
      if(checkValue("rec")){
        consume();
        
        ASTNode* rec_node = new ASTNode("rec");
        rec_node->addChild(Db());

        return rec_node;
      }
      return Db();
    }

    ASTNode* Db(){
      if(checkValue("(")){
        consume();
        ASTNode* d_node = D();
        expectValue(")");
        return d_node;
      }

      if (!checkType("ID")){
        throw runtime_error("Parse Error >> Expected identifier or '(' in Db, got'"+(isEnd()? "EOF": peek().value+"'."));
      }

      token firstId = consume();

      if (inFirstVb()){
        ASTNode* idNode = new ASTNode("ID",firstId.value);

        vector<ASTNode*> vbs;
        do{
          vbs.push_back(Vb());
        }while (inFirstVb());

        expectValue("=");

        ASTNode* expr_node = E();

        ASTNode* fcn_node = new ASTNode("fcn_form");
        fcn_node->addChild(idNode);
        for (ASTNode* vb: vbs) fcn_node->addChild(vb);
        fcn_node->addChild(expr_node);
        return fcn_node;
      }


      expectValue("=");
      ASTNode* exprNode = E();

      ASTNode* lhs = Vl();
      
      ASTNode* eq_node = new ASTNode("=");
      eq_node->addChild(lhs);
      eq_node->addChild(exprNode);
      return eq_node;
    }

    ASTNode* Vb(){
      if (checkType("ID")){
        token t = consume();
        return new ASTNode("ID", t.value);
      }

      if(checkValue("(")){
        consume();

        if(checkValue(")")){
          consume();
          return new ASTNode("()");
        }

        ASTNode* vl_node = Vl();
        expectValue(")");
        return vl_node;
      }

      throw runtime_error("Parse error >> Expected identifier or '(' in Vb, got '"+ (isEnd() ? "EOF" : peek().value) + "'.");
    }

    ASTNode* Vl(){
      ASTNode* node = new ASTNode(",");
      token first = expectType("ID");
      node->addChild(new ASTNode("<ID>", first.value));

      while (checkValue(",")) {
          consume();
          token next = expectType("ID");
          node->addChild(new ASTNode("<ID>", next.value));
      }

      // Single variable — no need for the ',' wrapper
      if (node->children.size() == 1)
          return node->children[0];

      return node;
    }    
};