#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include <stack>
#include <map>
#include <cmath>
#include <stdexcept>
#include "tree.h"
using namespace std;

// ─────────────────────────────────────────────
//  TOKEN  (control/stack element)
// ─────────────────────────────────────────────
struct Token {
    enum Type {
        NAME, INT, STR, BOOL, NIL,
        LAMBDA, GAMMA, BETA, TAU,
        YSTAR, DUMMY,
        ENV_MARKER,
        TUPLE, CLOSURE, PARTIAL,
        DELTA_REF          // ← NEW: branch reference for β, never Rule-2'd
    } type;

    string      strVal;
    int         intVal   = 0;
    bool        boolVal  = false;
    int         envId    = -1;
    int         deltaIdx = -1;
    string      param;
    vector<string> params;
    vector<Token*> tupleElems;
    string      builtinName;
    Token*      partialArg = nullptr;

    // factories
    static Token* makeInt(int v)       { auto* t=new Token; t->type=INT;  t->intVal=v;  return t; }
    static Token* makeStr(string v)    { auto* t=new Token; t->type=STR;  t->strVal=v;  return t; }
    static Token* makeBool(bool v)     { auto* t=new Token; t->type=BOOL; t->boolVal=v; return t; }
    static Token* makeNil()            { auto* t=new Token; t->type=NIL;  return t; }
    static Token* makeDummy()          { auto* t=new Token; t->type=DUMMY;return t; }
    static Token* makeName(string v)   { auto* t=new Token; t->type=NAME; t->strVal=v;  return t; }
    static Token* makeGamma()          { auto* t=new Token; t->type=GAMMA;return t; }
    static Token* makeBeta()           { auto* t=new Token; t->type=BETA; return t; }
    static Token* makeBeta(int thenIdx, int elseIdx) {
        auto* t = new Token; t->type = BETA;
        t->intVal = thenIdx;    // store then-branch index
        t->deltaIdx = elseIdx;  // store else-branch index
        return t;
    }
    static Token* makeTau(int n)       { auto* t=new Token; t->type=TAU;  t->intVal=n;  return t; }
    static Token* makeYstar()          { auto* t=new Token; t->type=YSTAR;return t; }
    static Token* makeEnvMarker(int id){ auto* t=new Token; t->type=ENV_MARKER; t->envId=id; return t; }
    static Token* makeLambda(int di, string p, int ei) {
        auto* t=new Token; t->type=LAMBDA;
        t->deltaIdx=di; t->param=p; t->envId=ei; return t;
    }
    static Token* makeLambdaN(int di, vector<string> ps, int ei) {
        auto* t=new Token; t->type=LAMBDA;
        t->deltaIdx=di; t->params=ps; t->envId=ei; return t;
    }
    // ── NEW: branch-delta reference (only for β; never pushed to stack) ──
    static Token* makeDeltaRef(int di) {
        auto* t=new Token; t->type=DELTA_REF; t->deltaIdx=di; return t;
    }
    static Token* makeTuple(vector<Token*> elems) {
        auto* t=new Token; t->type=TUPLE; t->tupleElems=elems; return t;
    }

    bool isNary() const { return type==LAMBDA && !params.empty(); }
    int  arity()  const { return isNary() ? (int)params.size() : 1; }

    string toString() const {
        switch(type){
            case INT:      return to_string(intVal);
            case STR:      return strVal;
            case BOOL:     return boolVal ? "true" : "false";
            case NIL:      return "nil";
            case DUMMY:    return "";
            case NAME:     return strVal;
            case GAMMA:    return "gamma";
            case BETA:     return "beta";
            case TAU:      return "tau_"+to_string(intVal);
            case YSTAR:    return "Y*";
            case ENV_MARKER: return "e"+to_string(envId);
            case LAMBDA:   return "lambda(d"+to_string(deltaIdx)+")";
            case CLOSURE:  return "closure";
            case DELTA_REF:return "deltaref(d"+to_string(deltaIdx)+")";
            case TUPLE: {
                string s="(";
                for(int i=0;i<(int)tupleElems.size();i++){
                    if(i) s+=", ";
                    s+=tupleElems[i]->toString();
                }
                return s+")";
            }
            case PARTIAL: return "partial("+builtinName+")";
        }
        return "?";
    }
};

// ─────────────────────────────────────────────
//  ENVIRONMENT
// ─────────────────────────────────────────────
struct Env {
    int id;
    int parent;
    map<string,Token*> bindings;
    Env(int id, int parent) : id(id), parent(parent) {}
};

struct EnvStore {
    vector<Env*> envs;

    int newEnv(int parent) {
        int id = envs.size();
        envs.push_back(new Env(id, parent));
        return id;
    }
    Env* get(int id) { return envs[id]; }

    Token* lookup(const string& name, int envId) {
        int cur = envId;
        while (cur >= 0) {
            Env* e = get(cur);
            auto it = e->bindings.find(name);
            if (it != e->bindings.end()) return it->second;
            cur = e->parent;
        }
        
        // Enhanced error message for debugging
        string envChain = "env_" + to_string(envId);
        Env* e = get(envId);
        while (e->parent >= 0) {
            envChain += " <- env_" + to_string(e->parent);
            e = get(e->parent);
        }
        
        throw runtime_error("Unbound name: " + name + " (searched in " + envChain + ")");
    }

    void bind(int envId, const string& name, Token* val) {
        get(envId)->bindings[name] = val;
    }
};

// ─────────────────────────────────────────────
//  DELTA  (a list of control tokens, pre-built)
// ─────────────────────────────────────────────
using Delta = vector<Token*>;

// ─────────────────────────────────────────────
//  FLATTEN AST  →  deltas
// ─────────────────────────────────────────────
struct DeltaBuilder {
    vector<Delta> deltas;
    int counter = 0;

    void build(Node* root) {
        deltas.resize(1);
        queue<pair<int,Node*>> wl;
        walk(root, 0, wl);
        while (!wl.empty()) {
            auto [idx, body] = wl.front(); wl.pop();
            if ((int)deltas.size() <= idx) deltas.resize(idx+1);
            walk(body, idx, wl);
        }
    }

    void walk(Node* node, int di, queue<pair<int,Node*>>& wl) {
        if (!node) return;

        const string& lbl = node->label;

        // ── lambda (unary or n-ary parameter syntax) ──────────────
        // IMPORTANT: Do NOT flatten nested lambdas like \x.\y.body
        // Each lambda node processes ONLY its immediate parameter.
        // Nested lambdas will be handled as separate lambda nodes.
        if (lbl == "lambda") {
            int myIdx = ++counter;
            if ((int)deltas.size() <= myIdx) deltas.resize(myIdx+1);

            // Process immediate parameter (could be comma-separated for n-ary)
            if (node->children.size() < 2)
                throw runtime_error("Lambda node malformed: missing parameter or body");
            
            Node* paramNode = node->children[0];
            Node* body = node->children[1];
            
            vector<string> ps;
            if (paramNode->label == ",") {
                // Explicit n-ary parameter: \(x,y,z) => body
                for (Node* c : paramNode->children) {
                    if (!c->value.empty())
                        ps.push_back(c->value);
                    else
                        ps.push_back(c->label);
                }
            } else {
                // Single parameter: \x => body
                string paramName;
                if (paramNode->label == "ID") {
                    paramName = paramNode->value;
                } else {
                    paramName = paramNode->label;
                }
                if (!paramName.empty())
                    ps.push_back(paramName);
                else
                    throw runtime_error("Lambda parameter name is empty");
            }

            if (ps.empty())
                throw runtime_error("Lambda has no parameters");

            Token* lamTok;
            if (ps.size() == 1)
                lamTok = Token::makeLambda(myIdx, ps[0], -1);
            else
                lamTok = Token::makeLambdaN(myIdx, ps, -1);

            deltas[di].push_back(lamTok);
            wl.push({myIdx, body});
            return;
        }

        // ── conditional ───────────────────────────────────────────
        //
        //  Delta layout (left = first popped from control):
        //    <cond tokens>  DELTA_REF(then)  DELTA_REF(else)  β
        //
        //  BETA handler pops two DELTA_REFs from control, then pops
        //  the boolean condition from the stack, and loads the chosen delta.
        //
        if (lbl == "->") {
            Node* cond  = node->children[0];
            Node* then_ = node->children[1];
            Node* else_ = node->children[2];

            int thenIdx = ++counter;
            int elseIdx = ++counter;
            if ((int)deltas.size() <= elseIdx) deltas.resize(elseIdx+1);

            wl.push({thenIdx, then_});
            wl.push({elseIdx, else_});

            walk(cond, di, wl);
                // ── FIX: Store branch indices inside BETA token ──
                // Don't push separate DELTA_REF tokens; they should never appear
                // on the control stream independently. BETA carries both indices.
                deltas[di].push_back(Token::makeBeta(thenIdx, elseIdx));
            return;
        }

        // ── tau ───────────────────────────────────────────────────
        if (lbl == "tau") {
            int n = node->children.size();
            for (Node* c : node->children) walk(c, di, wl);
            deltas[di].push_back(Token::makeTau(n));
            return;
        }

        // ── rec / Y* ─────────────────────────────────────────────
        if (lbl == "rec") {
            walk(node->children[0], di, wl);
            deltas[di].push_back(Token::makeGamma());
            deltas[di].push_back(Token::makeYstar());
            return;
        }

        // ── gamma / application ──────────────────────────────────
        if (lbl == "@") {
            walk(node->children[1], di, wl);
            walk(node->children[0], di, wl);
            deltas[di].push_back(Token::makeGamma());
            return;
        }

        // ── leaf nodes ───────────────────────────────────────────
        if (lbl == "INT") {
            deltas[di].push_back(Token::makeInt(stoi(node->value)));
            return;
        }
        if (lbl == "STR") {
            string s = node->value;
            if (s.size()>=2 && s.front()=='\'' && s.back()=='\'')
                s = s.substr(1, s.size()-2);
            deltas[di].push_back(Token::makeStr(s));
            return;
        }
        if (lbl == "ID") {
            deltas[di].push_back(Token::makeName(node->value));
            return;
        }
        if (lbl == "true")  { deltas[di].push_back(Token::makeBool(true));  return; }
        if (lbl == "false") { deltas[di].push_back(Token::makeBool(false)); return; }
        if (lbl == "nil")   { deltas[di].push_back(Token::makeNil());       return; }
        if (lbl == "dummy") { deltas[di].push_back(Token::makeDummy());     return; }

        // ── unary operator nodes (neg, not) ──────────────────────
        if (lbl == "neg" || lbl == "not") {
            walk(node->children[0], di, wl);
            deltas[di].push_back(Token::makeName(lbl));
            deltas[di].push_back(Token::makeGamma());
            return;
        }

        // ── binary operator nodes ────────────────────────────────
        if (lbl=="+" || lbl=="-" || lbl=="*" || lbl=="/" ||
            lbl=="**"|| lbl=="gr"|| lbl=="ge"|| lbl=="ls"||
            lbl=="le"|| lbl=="eq"|| lbl=="ne"|| lbl=="&" ||
            lbl=="or"|| lbl=="aug") {
            walk(node->children[1], di, wl);
            walk(node->children[0], di, wl);
            deltas[di].push_back(Token::makeName(lbl));
            deltas[di].push_back(Token::makeGamma());
            deltas[di].push_back(Token::makeGamma());
            return;
        }

        // ── gamma node (already an application node) ──────────────
        if (lbl == "gamma") {
            walk(node->children[1], di, wl);
            walk(node->children[0], di, wl);
            deltas[di].push_back(Token::makeGamma());
            return;
        }

        // ── fallback: push label as name, walk children ──────────
        deltas[di].push_back(Token::makeName(lbl));
        for (Node* c : node->children) walk(c, di, wl);
    }
};

// ─────────────────────────────────────────────
//  BUILT-IN FUNCTIONS
// ─────────────────────────────────────────────
static bool isBuiltin(const string& name) {
    static const vector<string> builtins = {
        "Print","print","Isinteger","Isstring","Istruth",
        "Istuple","Isfunction","Isdummy","Isnil",
        "Stem","Stern","Conc","Order","Null",
        "ItoS","not","neg",
        "+","-","*","/","**","gr","ge","ls","le","eq","ne",
        "&","or","aug"
    };
    for (auto& b : builtins) if (b==name) return true;
    return false;
}

static Token* applyBuiltin(const string& name, Token* arg) {
    if (name=="Print"||name=="print") {
        cout << arg->toString() << endl;
        return Token::makeDummy();
    }
    if (name=="Isinteger")  return Token::makeBool(arg->type==Token::INT);
    if (name=="Isstring")   return Token::makeBool(arg->type==Token::STR);
    if (name=="Istruth")    return Token::makeBool(arg->type==Token::BOOL);
    if (name=="Istuple")    return Token::makeBool(arg->type==Token::TUPLE);
    if (name=="Isfunction") return Token::makeBool(arg->type==Token::LAMBDA||arg->type==Token::PARTIAL);
    if (name=="Isdummy")    return Token::makeBool(arg->type==Token::DUMMY);
    if (name=="Isnil")      return Token::makeBool(arg->type==Token::NIL ||
                                  (arg->type==Token::TUPLE&&arg->tupleElems.empty()));
    if (name=="Null")       return Token::makeBool(arg->type==Token::NIL ||
                                  (arg->type==Token::TUPLE&&arg->tupleElems.empty()));
    if (name=="Order") {
        if (arg->type!=Token::TUPLE) throw runtime_error("Order: not a tuple");
        return Token::makeInt(arg->tupleElems.size());
    }
    if (name=="Stem") {
        if (arg->type!=Token::STR||arg->strVal.empty())
            throw runtime_error("Stem: not a non-empty string");
        return Token::makeStr(string(1,arg->strVal[0]));
    }
    if (name=="Stern") {
        if (arg->type!=Token::STR) throw runtime_error("Stern: not a string");
        return Token::makeStr(arg->strVal.size()>1 ? arg->strVal.substr(1) : "");
    }
    if (name=="ItoS") {
        if (arg->type!=Token::INT) throw runtime_error("ItoS: not an int");
        return Token::makeStr(to_string(arg->intVal));
    }
    if (name=="not") {
        if (arg->type!=Token::BOOL) throw runtime_error("not: not a bool");
        return Token::makeBool(!arg->boolVal);
    }
    if (name=="neg") {
        if (arg->type!=Token::INT) throw runtime_error("neg: not an int");
        return Token::makeInt(-arg->intVal);
    }
    if (name=="Conc"||name=="+"||name=="-"||name=="*"||name=="/"||
        name=="**"||name=="gr"||name=="ge"||name=="ls"||name=="le"||
        name=="eq"||name=="ne"||name=="&"||name=="or"||name=="aug") {
        auto* p = new Token; p->type=Token::PARTIAL;
        p->builtinName=name; p->partialArg=arg;
        return p;
    }
    throw runtime_error("Unknown builtin: "+name);
}

static Token* applyPartial(Token* partial, Token* arg2) {
    const string& name = partial->builtinName;
    Token* arg1 = partial->partialArg;

    if (name=="+") {
        if (arg1->type==Token::INT&&arg2->type==Token::INT)
            return Token::makeInt(arg1->intVal + arg2->intVal);
        if (arg1->type==Token::STR&&arg2->type==Token::STR)
            return Token::makeStr(arg1->strVal + arg2->strVal);
        throw runtime_error("+: type mismatch");
    }
    if (name=="-")  return Token::makeInt(arg1->intVal - arg2->intVal);
    if (name=="*")  return Token::makeInt(arg1->intVal * arg2->intVal);
    if (name=="/")  return Token::makeInt(arg1->intVal / arg2->intVal);
    if (name=="**") return Token::makeInt((int)pow(arg1->intVal,arg2->intVal));
    if (name=="gr") return Token::makeBool(arg1->intVal >  arg2->intVal);
    if (name=="ge") return Token::makeBool(arg1->intVal >= arg2->intVal);
    if (name=="ls") return Token::makeBool(arg1->intVal <  arg2->intVal);
    if (name=="le") return Token::makeBool(arg1->intVal <= arg2->intVal);
    if (name=="eq") {
        if (arg1->type==Token::INT &&arg2->type==Token::INT)
            return Token::makeBool(arg1->intVal==arg2->intVal);
        if (arg1->type==Token::STR &&arg2->type==Token::STR)
            return Token::makeBool(arg1->strVal==arg2->strVal);
        if (arg1->type==Token::BOOL&&arg2->type==Token::BOOL)
            return Token::makeBool(arg1->boolVal==arg2->boolVal);
        throw runtime_error("eq: type mismatch");
    }
    if (name=="ne") {
        if (arg1->type==Token::INT &&arg2->type==Token::INT)
            return Token::makeBool(arg1->intVal!=arg2->intVal);
        if (arg1->type==Token::STR &&arg2->type==Token::STR)
            return Token::makeBool(arg1->strVal!=arg2->strVal);
        if (arg1->type==Token::BOOL&&arg2->type==Token::BOOL)
            return Token::makeBool(arg1->boolVal!=arg2->boolVal);
        throw runtime_error("ne: type mismatch");
    }
    if (name=="&")   return Token::makeBool(arg1->boolVal && arg2->boolVal);
    if (name=="or")  return Token::makeBool(arg1->boolVal || arg2->boolVal);
    if (name=="aug") {
        vector<Token*> elems;
        if (arg1->type==Token::TUPLE) elems=arg1->tupleElems;
        else if (arg1->type!=Token::NIL) elems.push_back(arg1);
        if (arg2->type==Token::TUPLE)
            for(auto* e:arg2->tupleElems) elems.push_back(e);
        else if (arg2->type!=Token::NIL)
            elems.push_back(arg2);
        return Token::makeTuple(elems);
    }
    if (name=="Conc") {
        if (arg1->type!=Token::STR||arg2->type!=Token::STR)
            throw runtime_error("Conc: not strings");
        return Token::makeStr(arg1->strVal+arg2->strVal);
    }
    throw runtime_error("Unknown partial: "+name);
}

// ─────────────────────────────────────────────
//  CSE MACHINE
// ─────────────────────────────────────────────
struct CSEMachine {
    vector<Delta>&  deltas;
    EnvStore        envStore;
    vector<Token*>  control;
    vector<Token*>  stack;
    int             currentEnv;

    CSEMachine(vector<Delta>& d) : deltas(d) {
        int e0 = envStore.newEnv(-1);
        currentEnv = e0;

        stack.push_back(Token::makeEnvMarker(e0));

        // Load delta 0 in reverse (index 0 executes first)
        loadDelta(0);

        // ── FIX: push e0 env-exit marker BELOW delta0 tokens ──────
        // We want: control (top→bottom) = delta0_tokens..., e0_marker
        // After loadDelta(0) the delta is already in control (index 0 at back/top).
        // Insert the e0 marker at position 0 (bottom of control vector).
        control.insert(control.begin(), Token::makeEnvMarker(e0));
    }

    Token* ctrlPop() {
        if (control.empty()) throw runtime_error("Control underflow");
        Token* t = control.back(); control.pop_back(); return t;
    }
    Token* stkPop() {
        if (stack.empty()) throw runtime_error("Stack underflow");
        Token* t = stack.back(); stack.pop_back(); return t;
    }
    void stkPush(Token* t) { stack.push_back(t); }
    void ctrlPush(Token* t){ control.push_back(t); }

    // Load a delta onto control: push in reverse so delta[0] is on top
    void loadDelta(int idx) {
        const Delta& d = deltas[idx];
        for (int i = (int)d.size()-1; i >= 0; i--)
            control.push_back(d[i]);
    }

    void run() {
        while (!control.empty()) {
            Token* ctrl = ctrlPop();

            // ── Rule 1: Name lookup ───────────────────────────────
            if (ctrl->type == Token::NAME) {
                const string& name = ctrl->strVal;
                if (isBuiltin(name)) {
                    stkPush(ctrl);
                } else {
                    stkPush(envStore.lookup(name, currentEnv));
                }
                continue;
            }

            // ── Rule 2: Lambda → Closure ──────────────────────────
            // DELTA_REF tokens are intentionally excluded here;
            // they are only ever consumed by the BETA handler.
            if (ctrl->type == Token::LAMBDA) {
                Token* closure = new Token(*ctrl);
                closure->envId = currentEnv;
                stkPush(closure);
                continue;
            }

            // ── Literals: push directly ───────────────────────────
            if (ctrl->type == Token::INT  ||
                ctrl->type == Token::STR  ||
                ctrl->type == Token::BOOL ||
                ctrl->type == Token::NIL  ||
                ctrl->type == Token::DUMMY||
                ctrl->type == Token::TUPLE) {
                stkPush(ctrl);
                continue;
            }

            // ── Y*: push itself ───────────────────────────────────
            if (ctrl->type == Token::YSTAR) {
                stkPush(ctrl);
                continue;
            }

            // ── Rule 3/4/10/11: Gamma ─────────────────────────────
            if (ctrl->type == Token::GAMMA) {
                Token* rator = stkPop();
                Token* rand  = stkPop();
                applyRator(rator, rand);
                continue;
            }

            // ── Rule 5: Environment exit marker ───────────────────
            if (ctrl->type == Token::ENV_MARKER) {
                Token* value  = stkPop();
                Token* marker = stkPop();
                if (marker->type != Token::ENV_MARKER)
                    throw runtime_error("Rule 5: expected env marker on stack");
                // Restore the saved environment stored in marker->intVal
                // marker->envId is the newEnv id; marker->intVal holds the previous currentEnv
                currentEnv = marker->intVal;
                stkPush(value);
                continue;
            }

            // ── Rule 8: Beta (conditional) ────────────────────────
            //
            if (ctrl->type == Token::BETA) {
                    // ── Branch indices stored in BETA token itself ──
                    int thenIdx = ctrl->intVal;
                    int elseIdx = ctrl->deltaIdx;
                Token* cond = stkPop();
                if (cond->type != Token::BOOL)
                    throw runtime_error("Beta: condition is not boolean");
                    int chosenIdx = cond->boolVal ? thenIdx : elseIdx;
                loadDelta(chosenIdx);
                continue;
            }

            // ── Rule 9: Tau (tuple formation) ─────────────────────
            if (ctrl->type == Token::TAU) {
                int n = ctrl->intVal;
                vector<Token*> elems(n);
                for (int i = n-1; i >= 0; i--)
                    elems[i] = stkPop();
                stkPush(Token::makeTuple(elems));
                continue;
            }


            throw runtime_error("Unhandled control token: " + ctrl->toString());
        }
    }

    void applyRator(Token* rator, Token* rand) {

        // ── Y* application ────────────────────────────────────────
        if (rator->type == Token::YSTAR) {
            stkPush(rand);
            stkPush(rand);
            ctrlPush(Token::makeGamma());
            ctrlPush(rator);
            ctrlPush(Token::makeGamma());
            return;
        }

        // ── Rule 10: Tuple selection ──────────────────────────────
        if (rator->type == Token::TUPLE) {
            if (rand->type != Token::INT)
                throw runtime_error("Tuple index must be integer");
            int idx = rand->intVal;
            if (idx < 1 || idx > (int)rator->tupleElems.size())
                throw runtime_error("Tuple index out of range: "+to_string(idx));
            stkPush(rator->tupleElems[idx-1]);
            return;
        }

        // ── Built-in name ─────────────────────────────────────────
        if (rator->type == Token::NAME && isBuiltin(rator->strVal)) {
            stkPush(applyBuiltin(rator->strVal, rand));
            return;
        }

        // ── Partial application of built-in ──────────────────────
        if (rator->type == Token::PARTIAL) {
            stkPush(applyPartial(rator, rand));
            return;
        }

        // ── Rules 4 / 11: Lambda closure application ─────────────
        if (rator->type == Token::LAMBDA) {
            int newEnvId = envStore.newEnv(rator->envId);

            if (rator->isNary()) {
                if (rand->type != Token::TUPLE ||
                    (int)rand->tupleElems.size() != rator->arity())
                    throw runtime_error("N-ary lambda: argument count mismatch");
                for (int i = 0; i < rator->arity(); i++)
                    envStore.bind(newEnvId, rator->params[i],
                                  rand->tupleElems[i]);
            } else {
                // Bind parameter to argument in new environment
                if (rator->param.empty())
                    throw runtime_error("Lambda has empty parameter name");
                envStore.bind(newEnvId, rator->param, rand);
            }

            // Save the current environment to restore later
            int savedEnv = currentEnv;

            // ── CRITICAL: Update currentEnv BEFORE executing body ──
            // The lambda body needs to execute with currentEnv = newEnvId
            // so that parameter lookups find the bindings in the new env.
            currentEnv = newEnvId;

            Token* marker = Token::makeEnvMarker(newEnvId);
            // Store the previous environment inside the marker so Rule 5
            // can restore it when the env-exit is handled.
            marker->intVal = savedEnv;
            stkPush(marker);

            // Push env-exit marker onto control so it will be popped after
            // the delta body tokens have executed.
            ctrlPush(marker);

            loadDelta(rator->deltaIdx);
            return;
        }

        throw runtime_error("Cannot apply: " + rator->toString()
                            + " to " + rand->toString());
    }

    Token* result() {
        if (stack.empty()) throw runtime_error("Empty stack at end");
        for (int i = (int)stack.size()-1; i >= 0; i--)
            if (stack[i]->type != Token::ENV_MARKER)
                return stack[i];
        throw runtime_error("No value on stack");
    }
};

// ─────────────────────────────────────────────
//  PUBLIC ENTRY POINT
// ─────────────────────────────────────────────
Token* runCSE(Node* ast) {
    DeltaBuilder builder;
    builder.build(ast);

    CSEMachine machine(builder.deltas);
    machine.run();
    return machine.result();
}