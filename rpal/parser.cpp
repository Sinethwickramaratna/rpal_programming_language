#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include "token.h"
#include "tree.h"
using namespace std;

// Parser for the RPAL phrase structure grammar.
// Each method below mirrors one grammar level and builds the corresponding AST.
class Parser{
  public:
    explicit Parser(const vector<token> tokens) : tokens_(move(tokens)), pos_(0) {}
  
    // Parse the full program and return the root AST node.
    Node* parse(){
      Node* tree = E();
      if(!isEnd()){
        throw runtime_error("Parse error>> Unexpected token '" + peek().value + "' after end of expression.");
      }
      return tree;
    }
  
  private:
    vector<token> tokens_;
    size_t pos_=0;

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
      return checkType("ID")  || checkType("INT")  || checkType("STR") ||
              checkValue("true") || checkValue("false") ||
              checkValue("nil")  || checkValue("dummy") ||
              checkValue("(");
    }

    // E -> let D in E | fn Vb+ . E | Ew
    // This is the top-level expression rule, so it also handles the two
    // function/definition forms that rewrite into lambda and definition trees.
    Node* E(){
      if (checkValue("let")){
        consume();
        // let D in E becomes a tree with the definition on the left and body on
        // the right. The standardizer later turns this into a gamma/lambda form.
        Node* def_Node = D();

        expectValue("in");

        Node* body_Node = E();

        Node* let_Node = new Node("let");
        let_Node->addChild(def_Node);
        let_Node->addChild(body_Node);
        return let_Node;
      }

      if(checkValue("fn")){
        consume();
        
        if (!inFirstVb()){
          throw runtime_error("Parse error>> Expected identifier or '(' after 'fn' but found '" + (isEnd() ? "EOF" : peek().value) + "'.");
        }

        // fn Vb+ . E becomes a nested lambda chain, one lambda per parameter.
        vector<Node*> vbs;
        do {
          vbs.push_back(Vb());
        } while(inFirstVb());

        expectValue(".");

        Node* body_Node = E();
        Node* result = body_Node;
        for (int i = (int)vbs.size() - 1; i >= 0; i--){
          Node* lambda_Node = new Node("lambda");
          lambda_Node->addChild(vbs[i]);
          lambda_Node->addChild(result);
          result = lambda_Node;
        }

        return result;
      }

      return Ew();
    }
    
    // Ew -> T where Dr | T
    // This attaches an optional where-clause to an ordinary tuple expression.
    Node* Ew(){
      Node* t_node = T();

      if(checkValue("where")){
        consume();
        Node* dr_node = Dr();

        Node* where_node = new Node("where");
        where_node->addChild(t_node);
        where_node->addChild(dr_node);
        return where_node;
      }
      return t_node;
    }

    // T -> Ta (, Ta)+ | Ta
    // Tuples are represented with a comma node, which the standardizer later
    // turns into the canonical tuple form.
    Node* T(){
      Node* ta_node = Ta();

      if (!checkValue(",")){
        return ta_node;
      }

      Node* tau_node = new Node("tau");
      tau_node->addChild(ta_node);

      while (checkValue(",")){
        consume();
        tau_node->addChild(Ta());
      }

      return tau_node;
    }

    // Ta -> Ta aug Tc | Tc
    // aug is left-associative, so repeated aug operations build a left-leaning tree.
    Node* Ta(){
      Node* node = Tc();

      while (checkValue("aug")){
        consume();
        Node* right = Tc();

        Node* aug_node = new Node("aug");
        aug_node->addChild(node);
        aug_node->addChild(right);
        node = aug_node;
      }
      return node;
    }

    // Tc -> B -> Tc | Tc | B -> Tc | Tc
    // The conditional operator is right-associative, so the recursive call is on
    // the then/else branches rather than on the left side.
    Node* Tc(){
      Node* b_node = B();

      if (checkValue("->")){
        consume();
        Node* then_node = Tc();

        expectValue("|");

        Node* else_node = Tc();
        
        Node* cond_node = new Node("->");
        cond_node->addChild(b_node);
        cond_node->addChild(then_node);
        cond_node->addChild(else_node);

        return cond_node;
      }

      return b_node;
    }

    // B -> B or Bt | Bt
    // Boolean OR is left-associative and sits above AND in precedence.
    Node* B(){
      Node* node = Bt();

      while(checkValue("or")){
        consume();
        Node* right = Bt();

        Node* or_node = new Node("or");
        or_node->addChild(node);
        or_node->addChild(right);

        node = or_node;
      }

      return node;
    }

    // Bt -> Bt & Bs | Bs
    // Boolean AND is left-associative and binds tighter than OR.
    Node* Bt(){
      Node* node = Bs();

      while(checkValue("&")){
        consume();
        Node* right = Bs();

        Node* and_node = new Node("&");
        and_node->addChild(node);
        and_node->addChild(right);

        node = and_node;
      }
      return node;
    }

    // Bs -> not Bp | Bp
    // not is a unary boolean operator, so it consumes one operand.
    Node* Bs(){
      if(checkValue("not")){
        consume();
        Node* not_node = new Node("not");
        not_node->addChild(Bp());
        return not_node;
      }
      return Bp();
    }

    // Bp -> relational comparison | A
    // This handles the comparison operators and otherwise falls back to
    // arithmetic expressions.
    Node* Bp(){
      Node* left = A();

      string rel="";
      if (checkValue("gr")||checkValue(">")) rel="gr";
      else if (checkValue("ge")||checkValue(">=")) rel="ge";
      else if (checkValue("ls")||checkValue("<")) rel="ls";
      else if (checkValue("le")||checkValue("<=")) rel="le";
      else if (checkValue("eq")) rel="eq";
      else if (checkValue("ne")) rel="ne";

      if (!rel.empty()){
        consume();
        Node* right = A();

        Node* rel_node = new Node(rel);
        rel_node->addChild(left);
        rel_node->addChild(right);
        return rel_node;
      }
      return left;
    }

    // A -> + At | - At | A + At | A - At | At
    // Unary + is a no-op, unary - becomes a neg node, and binary +/-. are
    // parsed as left-associative arithmetic chains.
    Node* A(){
      if(checkValue("+")){
        consume();
        return At();
      }

      if(checkValue("-")){
        consume();
        Node* neg_node = new Node("neg");
        neg_node->addChild(At());
        return neg_node;
      }


      Node* node = At();
      
      while(checkValue("+")||checkValue("-")){
        string op = consume().value;
        Node* right = At();

        Node* op_node = new Node(op);
        op_node->addChild(node);
        op_node->addChild(right);
        node = op_node;
      }
      return node;
    }

    // At -> At * Af | At / Af | Af
    // Multiplication and division are left-associative.
    Node* At(){
      Node* node = Af();

      while(checkValue("*")||checkValue("/")){
        string op = consume().value;

        Node* right = Af();
        
        Node* op_node = new Node(op);
        op_node -> addChild(node);
        op_node -> addChild(right);
        node = op_node;
      }
      return node;
    }

    // Af -> Ap ** Af | Ap
    // Exponentiation is right-associative, so the recursive call stays on the
    // right-hand side.
    Node* Af(){
      Node* ap_node = Ap();
      if (checkValue("**")){
        consume();
        Node* right = Af();

        Node* power_node = new Node("**");
        power_node -> addChild(ap_node);
        power_node -> addChild(right);

        return power_node;
      }

      return ap_node;
    }

    // Ap -> Ap @ <IDENTIFIER> R | R
    // @ is used for explicit function application with an identifier selector.
    Node* Ap(){
      Node* node = R();

      while(checkValue("@")){
        consume();

        token id_tok = expectType("ID");
        Node* idNode = new Node("ID", id_tok.value);

        Node* r_node = R();

        Node* at_node = new Node("@");
        at_node->addChild(node);
        at_node->addChild(idNode);
        at_node->addChild(r_node);
        node = at_node;
      }
      return node;
    }

    // R -> R Rn | Rn
    // Adjacent rands form gamma application chains, so this method keeps adding
    // application nodes while the next token still starts a valid atomic form.
    Node* R(){
      Node* node = Rn();

      while (inFirstRn()){
        Node* right = Rn();

        Node* gamma_node = new Node("gamma");
        gamma_node->addChild(node);
        gamma_node->addChild(right);
        node = gamma_node;
      }

      return node;
    }

    // Rn -> identifiers, literals, parenthesized expressions, and dummy.
    // These are the atomic leaves of the expression grammar.
    Node* Rn(){
      if (checkType("ID")){
        token t = consume();
        return new Node("ID", t.value);
      }

      if (checkType("STR")){
        token t = consume();
        return new Node("STR", t.value);
      }

      if (checkType("INT")){
        token t = consume();
        return new Node("INT",t.value);
      }

      if (checkValue("true")){
        consume();
        return new Node("true");
      }

      if (checkValue("false")){
        consume();
        return new Node("false");
      }

      if (checkValue("nil")){
        consume();
        return new Node("nil");
      }

      if (checkValue("dummy")){
        consume();
        return new Node("dummy");
      }

      if (checkValue("(")){
        consume();
        Node* e_node = E();
        expectValue(")");
        return e_node;
      }

      throw runtime_error("Parse erro >> Unexpected token '"+(isEnd()? "EOF": peek().value)+"'.");
    }

    // D -> Da within D | Da
    // Definitions may be nested with within, so this rule recursively builds a
    // binary tree that preserves the order of the original source.
    Node* D(){
      Node* da_node = Da();

      if(checkValue("within")){
        consume();
        Node* d_node = D();

        Node* within_node = new Node("within");
        within_node->addChild(da_node);
        within_node->addChild(d_node);

        return within_node;
      }

      return da_node;
    }

    // Da -> Dr and Dr and Dr ... | Dr
    // and combines multiple simultaneous definitions into one node.
    Node* Da(){
      Node* node = Dr();
      if(!checkValue("and")) return node;

      Node* and_node = new Node("and");
      and_node->addChild(node);
      while(checkValue("and")){
        consume();
        and_node->addChild(Dr());
      }
      return and_node;
    }

    // Dr -> rec Db | Db
    // rec is a unary definition form that marks a recursive binding.
    Node* Dr(){
      if(checkValue("rec")){
        consume();
        
        Node* rec_node = new Node("rec");
        rec_node->addChild(Db());

        return rec_node;
      }
      return Db();
    }

    // Db handles the three definition shapes:
    //   1. ( D )
    //   2. V = E
    //   3. <IDENTIFIER> Vb+ = E   (function form)
    Node* Db(){
      if(checkValue("(")){
        consume();
        Node* d_node = D();
        expectValue(")");
        return d_node;
      }

      if (!checkType("ID")){
        throw runtime_error("Parse Error >> Expected identifier or '(' in Db, got '"+(isEnd()? "EOF": peek().value+"'."));
      }

      token firstId = consume();

      
      if (inFirstVb()){
        // Function definition form is rewritten into fcn_form so the
        // standardizer can later turn it into a nested lambda chain.
        Node* idNode = new Node("ID", firstId.value);

        vector<Node*> vbs;
        do{
          vbs.push_back(Vb());
        } while(inFirstVb());

        expectValue("=");

        Node* expr_node = E();

        Node* fcn_node = new Node("fcn_form");
        fcn_node->addChild(idNode);
        for (Node* vb : vbs) fcn_node->addChild(vb);
        fcn_node->addChild(expr_node);
        return fcn_node;
      }

      
      
      Node* lhs;
      if(checkValue(",")){
        // A comma-separated identifier list on the left-hand side becomes a
        // tuple pattern, which the standardizer later converts into a tuple bind.
        
        Node* comma_node = new Node(",");
        comma_node->addChild(new Node("ID", firstId.value));
        while(checkValue(",")){
          consume();
          token next = expectType("ID");
          comma_node->addChild(new Node("ID", next.value));
        }
        lhs = comma_node;
      } else {
        lhs = new Node("ID", firstId.value);
      }

      expectValue("=");
      Node* exprNode = E();

      Node* eq_node = new Node("=");
      eq_node->addChild(lhs);
      eq_node->addChild(exprNode);
      return eq_node;
    }

    // Vb -> <IDENTIFIER> | ( Vl ) | ()
    // Vb is used in function parameter positions and can be a single variable,
    // a tuple pattern, or the empty pattern.
    Node* Vb(){
      if (checkType("ID")){
        token t = consume();
        return new Node("ID", t.value);
      }

      if(checkValue("(")){
        consume();

        if(checkValue(")")){
          consume();
          return new Node("()");
        }

        Node* vl_node = Vl();
        expectValue(")");
        return vl_node;
      }

      throw runtime_error("Parse error >> Expected identifier or '(' in Vb, got '"+ (isEnd() ? "EOF" : peek().value) + "'.");
    }

    // Vl -> <IDENTIFIER> list ,
    // Vl parses a non-empty identifier list used inside tuple parameter patterns.
    Node* Vl(){
      Node* node = new Node(",");
      token first = expectType("ID");
      node->addChild(new Node("ID", first.value));

      while (checkValue(",")) {
          consume();
          token next = expectType("ID");
          node->addChild(new Node("ID", next.value));
      }

      
      if (node->children.size() == 1)
          return node->children[0];

      return node;
    }    
};

