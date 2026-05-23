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

// Decode escape sequences before a string literal is stored or printed.
// RPAL string literals may arrive with escape markers such as \n or \t still
// embedded in the raw AST text. The evaluator stores the decoded form so the
// same value is used consistently by builtins, tuple printing, and diagnostics.
static string decodeEscapes(const string &s)
{
    string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size(); ++i)
    {
        char c = s[i];
        if (c == '\\' && i + 1 < s.size())
        {
            char n = s[i + 1];
            switch (n)
            {
            case 'n':
                out.push_back('\n');
                break;
            case 't':
                out.push_back('\t');
                break;
            case 'r':
                out.push_back('\r');
                break;
            case '\\':
                out.push_back('\\');
                break;
            case '"':
                out.push_back('"');
                break;
            case '\'':
                out.push_back('\'');
                break;
            default:

                out.push_back(n);
                break;
            }
            ++i;
        }
        else
        {
            out.push_back(c);
        }
    }
    return out;
}
// Token is the shared runtime representation used by the CSE machine.
// The same structure represents both data values and control markers, which
// keeps the control, stack, and environment logic uniform. Literal tokens hold
// direct values, while control tokens such as gamma, beta, tau, Y*, and env
// markers describe the next machine step.
struct Token
{
    enum Type
    {
        NAME,
        INT,
        STR,
        BOOL,
        NIL,
        LAMBDA,
        GAMMA,
        BETA,
        TAU,
        YSTAR,
        DUMMY,
        ENV_MARKER,
        ETA,
        TUPLE,
        CLOSURE,
        PARTIAL,
        DELTA_REF
    } type;

    string strVal;
    int intVal = 0;
    bool boolVal = false;
    int envId = -1;
    int deltaIdx = -1;
    string param;
    vector<string> params;
    vector<Token *> tupleElems;
    string builtinName;
    Token *partialArg = nullptr;
    Token *etaClosure = nullptr;

    // Factory helpers keep token construction centralized and consistent.
    // This makes the machine code easier to read because every token type is
    // created in one obvious way instead of scattering raw struct initialization.
    static Token *makeInt(int v)
    {
        auto *t = new Token;
        t->type = INT;
        t->intVal = v;
        return t;
    }
    static Token *makeStr(string v)
    {
        auto *t = new Token;
        t->type = STR;
        t->strVal = v;
        return t;
    }
    static Token *makeBool(bool v)
    {
        auto *t = new Token;
        t->type = BOOL;
        t->boolVal = v;
        return t;
    }
    static Token *makeNil()
    {
        auto *t = new Token;
        t->type = NIL;
        return t;
    }
    static Token *makeDummy()
    {
        auto *t = new Token;
        t->type = DUMMY;
        return t;
    }
    static Token *makeName(string v)
    {
        auto *t = new Token;
        t->type = NAME;
        t->strVal = v;
        return t;
    }
    static Token *makeGamma()
    {
        auto *t = new Token;
        t->type = GAMMA;
        return t;
    }
    static Token *makeBeta()
    {
        auto *t = new Token;
        t->type = BETA;
        return t;
    }
    static Token *makeBeta(int thenIdx, int elseIdx)
    {
        auto *t = new Token;
        t->type = BETA;
        t->intVal = thenIdx;
        t->deltaIdx = elseIdx;
        return t;
    }
    static Token *makeTau(int n)
    {
        auto *t = new Token;
        t->type = TAU;
        t->intVal = n;
        return t;
    }
    static Token *makeYstar()
    {
        auto *t = new Token;
        t->type = YSTAR;
        return t;
    }
    static Token *makeEnvMarker(int id)
    {
        auto *t = new Token;
        t->type = ENV_MARKER;
        t->envId = id;
        return t;
    }
    static Token *makeLambda(int di, string p, int ei)
    {
        auto *t = new Token;
        t->type = LAMBDA;
        t->deltaIdx = di;
        t->param = p;
        t->envId = ei;
        return t;
    }
    static Token *makeLambdaN(int di, vector<string> ps, int ei)
    {
        auto *t = new Token;
        t->type = LAMBDA;
        t->deltaIdx = di;
        t->params = ps;
        t->envId = ei;
        return t;
    }

    static Token *makeDeltaRef(int di)
    {
        auto *t = new Token;
        t->type = DELTA_REF;
        t->deltaIdx = di;
        return t;
    }
    static Token *makeTuple(vector<Token *> elems)
    {
        auto *t = new Token;
        t->type = TUPLE;
        t->tupleElems = elems;
        return t;
    }

    static Token *makeEta(Token *closure)
    {
        auto *t = new Token;
        t->type = ETA;
        t->etaClosure = closure;
        return t;
    }

    bool isNary() const { return type == LAMBDA && !params.empty(); }
    int arity() const { return isNary() ? (int)params.size() : 1; }

    // Human-readable form used for diagnostics and builtin printing.
    // This is intentionally compact, because the machine prints values often
    // when debugging control/stack behavior or when the Print builtin is used.
    string toString() const
    {
        switch (type)
        {
        case INT:
            return to_string(intVal);
        case STR:
            return strVal;
        case BOOL:
            return boolVal ? "true" : "false";
        case NIL:
            return "nil";
        case DUMMY:
            return "";
        case NAME:
            return strVal;
        case GAMMA:
            return "gamma";
        case BETA:
            return "beta";
        case TAU:
            return "tau_" + to_string(intVal);
        case YSTAR:
            return "Y*";
        case ETA:
            return "eta";
        case ENV_MARKER:
            return "e" + to_string(envId);
        case LAMBDA: {
            // For lambdas captured as closures, print a more descriptive form
            // that shows the parameter name(s) and the environment id. This
            // makes output like `[lambda closure: x: 2]` instead of the
            // terse `lambda(d1)` which is less helpful for users.
            string names;
            if (isNary())
            {
                for (size_t i = 0; i < params.size(); ++i)
                {
                    if (i)
                        names += ",";
                    names += params[i];
                }
            }
            else
            {
                names = param;
            }
            if (deltaIdx >= 0)
                return string("[lambda closure: ") + names + ": " + to_string(deltaIdx + 1) + "]";
            return "lambda(d" + to_string(deltaIdx) + ")";
        }
        case CLOSURE:
            return "closure";
        case DELTA_REF:
            return "deltaref(d" + to_string(deltaIdx) + ")";
        case TUPLE:
        {
            string s = "(";
            for (int i = 0; i < (int)tupleElems.size(); i++)
            {
                if (i)
                    s += ", ";
                s += tupleElems[i]->toString();
            }
            return s + ")";
        }
        case PARTIAL:
            return "partial(" + builtinName + ")";
        }
        return "?";
    }
};

// Environment frame with parent link and bindings for lexical lookup.
// Each environment corresponds to one lexical scope in the RPAL program.
struct Env
{
    int id;
    int parent;
    map<string, Token *> bindings;
    Env(int id, int parent) : id(id), parent(parent) {}
};

// Owns the chain of environments created while the CSE machine runs.
// lookup() walks parents to implement lexical scoping, and bind() stores the
// values created when lambdas are applied.
struct EnvStore
{
    vector<Env *> envs;

    int newEnv(int parent)
    {
        int id = envs.size();
        envs.push_back(new Env(id, parent));
        return id;
    }
    Env *get(int id) { return envs[id]; }

    // CSE Rule 1: resolve names by searching the current environment chain.
    // This is the lexical lookup step of the machine: if the name is not found
    // in the current frame, the search continues in the parent, then the parent
    // of that parent, and so on until the root environment is reached.
    Token *lookup(const string &name, int envId)
    {
        int cur = envId;
        while (cur >= 0)
        {
            Env *e = get(cur);
            auto it = e->bindings.find(name);
            if (it != e->bindings.end())
                return it->second;
            cur = e->parent;
        }

        string envChain = "env_" + to_string(envId);
        Env *e = get(envId);
        while (e->parent >= 0)
        {
            envChain += " <- env_" + to_string(e->parent);
            e = get(e->parent);
        }

        throw runtime_error("Unbound name: " + name + " (searched in " + envChain + ")");
    }

    void bind(int envId, const string &name, Token *val)
    {
        get(envId)->bindings[name] = val;
    }
};

using Delta = vector<Token *>;

// Convert the AST into the delta/control representation consumed by the machine.
// The builder turns the parsed tree into executable control tokens and stores
// deferred lambda/branch bodies in separate deltas. That lets the evaluator
// jump into a branch or closure body only when the machine rules demand it.
struct DeltaBuilder
{
    vector<Delta> deltas;
    int counter = 0;

    // Build delta 0, then expand any deferred lambda/branch bodies.
    // The queue acts as a worklist so that nested lambdas and conditionals are
    // assigned stable delta indices before their bodies are fully emitted.
    void build(Node *root)
    {
        deltas.resize(1);
        queue<pair<int, Node *>> wl;
        walk(root, 0, wl);
        while (!wl.empty())
        {
            auto [idx, body] = wl.front();
            wl.pop();
            if ((int)deltas.size() <= idx)
                deltas.resize(idx + 1);
            walk(body, idx, wl);
        }
    }

    // Walk an AST subtree and emit the corresponding control tokens.
    // The order matters: many nodes are emitted in reverse evaluation order so
    // that the control list, when popped from the back, reproduces RPAL's
    // left-to-right execution semantics.
    void walk(Node *node, int di, queue<pair<int, Node *>> &wl)
    {
        if (!node)
            return;

        const string &lbl = node->label;

        if (lbl.empty() && node->value == "Y*")
        {
            deltas[di].push_back(Token::makeYstar());
            return;
        }

        if (lbl == "lambda")
        {
            // CSE Rule 2: lambda nodes become closures referencing a delta body.
            // The parameter list is collected here, but the body is emitted into
            // a separate delta so the machine can evaluate it later under the
            // environment that exists at application time.
            int myIdx = ++counter;
            if ((int)deltas.size() <= myIdx)
                deltas.resize(myIdx + 1);

            if (node->children.size() < 2)
                throw runtime_error("Lambda node malformed: missing parameter or body");

            Node *paramNode = node->children[0];
            Node *body = node->children[1];

            vector<string> ps;
            if (paramNode->label == ",")
            {

                for (Node *c : paramNode->children)
                {
                    if (!c->value.empty())
                        ps.push_back(c->value);
                    else
                        ps.push_back(c->label);
                }
            }
            else
            {

                string paramName;
                if (paramNode->label == "ID")
                {
                    paramName = paramNode->value;
                }
                else
                {
                    paramName = paramNode->label;
                }
                if (!paramName.empty())
                    ps.push_back(paramName);
                else
                    throw runtime_error("Lambda parameter name is empty");
            }

            if (ps.empty())
                throw runtime_error("Lambda has no parameters");

            Token *lamTok;
            if (ps.size() == 1)
                lamTok = Token::makeLambda(myIdx, ps[0], -1);
            else
                lamTok = Token::makeLambdaN(myIdx, ps, -1);

            deltas[di].push_back(lamTok);
            wl.push({myIdx, body});
            return;
        }

        if (lbl == "->")
        {
            // CSE Rule 8: conditionals split into separate then/else deltas.
            // The condition itself is emitted into the current control stream,
            // and the beta token remembers where the machine should jump next.
            Node *cond = node->children[0];
            Node *then_ = node->children[1];
            Node *else_ = node->children[2];

            int thenIdx = ++counter;
            int elseIdx = ++counter;
            if ((int)deltas.size() <= elseIdx)
                deltas.resize(elseIdx + 1);

            wl.push({thenIdx, then_});
            wl.push({elseIdx, else_});

            walk(cond, di, wl);

            deltas[di].push_back(Token::makeBeta(thenIdx, elseIdx));
            return;
        }

        if (lbl == "tau")
        {
            // CSE Rule 9: tuple formation evaluates each element before the tuple
            // token is emitted. The machine later collects those values into one
            // tuple value on the stack.
            int n = node->children.size();
            for (Node *c : node->children)
                walk(c, di, wl);
            deltas[di].push_back(Token::makeTau(n));
            return;
        }

        if (lbl == "rec")
        {
            // Recursive bindings are compiled through Y* and gamma application.
            // The recursive name is not resolved immediately; instead the machine
            // builds a recursive closure using the Y-combinator style mechanism.
            walk(node->children[0], di, wl);

            deltas[di].push_back(Token::makeGamma());
            deltas[di].push_back(Token::makeYstar());
            return;
        }

        if (lbl == "@")
        {
            // Function application in the AST becomes argument/function order for
            // gamma. The child order is reversed here because the control stack is
            // popped from the back during execution.
            walk(node->children[1], di, wl);
            walk(node->children[0], di, wl);
            deltas[di].push_back(Token::makeGamma());
            return;
        }

        if (lbl == "INT")
        {
            deltas[di].push_back(Token::makeInt(stoi(node->value)));
            return;
        }
        if (lbl == "STR")
        {
            string s = node->value;
            if (s.size() >= 2 && s.front() == '\'' && s.back() == '\'')
                s = s.substr(1, s.size() - 2);
            else if (s.size() >= 2 && s.front() == '\"' && s.back() == '\"')
                s = s.substr(1, s.size() - 2);
            s = decodeEscapes(s);
            deltas[di].push_back(Token::makeStr(s));
            return;
        }
        if (lbl == "ID")
        {
            deltas[di].push_back(Token::makeName(node->value));
            return;
        }
        if (lbl == "true")
        {
            deltas[di].push_back(Token::makeBool(true));
            return;
        }
        if (lbl == "false")
        {
            deltas[di].push_back(Token::makeBool(false));
            return;
        }
        if (lbl == "nil" || lbl == "nill")
        {
            deltas[di].push_back(Token::makeNil());
            return;
        }
        if (lbl == "dummy")
        {
            deltas[di].push_back(Token::makeDummy());
            return;
        }

        if (lbl == "neg" || lbl == "not")
        {
            // Unary operators are emitted as builtin names followed by gamma.
            // That keeps them in the same application pipeline as ordinary
            // functions even though they are implemented as builtins.
            walk(node->children[0], di, wl);
            deltas[di].push_back(Token::makeName(lbl));
            deltas[di].push_back(Token::makeGamma());
            return;
        }

        if (lbl == "+" || lbl == "-" || lbl == "*" || lbl == "/" ||
            lbl == "**" || lbl == "gr" || lbl == "ge" || lbl == "ls" ||
            lbl == "le" || lbl == "eq" || lbl == "ne" || lbl == "&" ||
            lbl == "or" || lbl == "aug")
        {
            // Binary operators become a builtin name plus two gamma applications.
            // The first gamma turns the operator into a partial application; the
            // second gamma completes the operation after both operands are ready.
            walk(node->children[1], di, wl);
            walk(node->children[0], di, wl);
            deltas[di].push_back(Token::makeName(lbl));
            deltas[di].push_back(Token::makeGamma());
            deltas[di].push_back(Token::makeGamma());
            return;
        }

        if (lbl == "gamma")
        {
            // Gamma nodes preserve the evaluator's function-argument order.
            // The right child is emitted first so that, after reverse pushing,
            // the machine sees the operator/function above the argument value.
            walk(node->children[1], di, wl);
            walk(node->children[0], di, wl);
            deltas[di].push_back(Token::makeGamma());
            return;
        }

        deltas[di].push_back(Token::makeName(lbl));
        for (Node *c : node->children)
            walk(c, di, wl);
    }
};

// Recognize builtin names so the evaluator can dispatch without environment lookup.
// Builtins behave like pre-defined runtime functions rather than lexical names.
static bool isBuiltin(const string &name)
{
    static const vector<string> builtins = {
        "Print", "print", "Isinteger", "Isstring", "Istruth",
        "Istuple", "Isfunction", "Isdummy", "Isnil",
        "Stem", "Stern", "Conc", "Order", "Null",
        "ItoS", "not", "neg",
        "+", "-", "*", "/", "**", "gr", "ge", "ls", "le", "eq", "ne",
        "&", "or", "aug"};
    for (auto &b : builtins)
        if (b == name)
            return true;
    return false;
}

// CSE Rule 7: unary builtins are applied directly here.
// These are builtins that need only one operand, such as Print, not, neg, and
// the classification predicates. Their result can be produced immediately.
static Token *applyBuiltin(const string &name, Token *arg)
{
    if (name == "Print" || name == "print")
    {
        if (arg->type != Token::DUMMY)
            cout << arg->toString();
        return Token::makeDummy();
    }
    if (name == "Isinteger")
        return Token::makeBool(arg->type == Token::INT);
    if (name == "Isstring")
        return Token::makeBool(arg->type == Token::STR);
    if (name == "Istruth")
        return Token::makeBool(arg->type == Token::BOOL);
    if (name == "Istuple")
    {
        // Treat `nil` as an empty tuple for compatibility with programs
        // that expect empty tuples to behave like tuples of order 0.
        return Token::makeBool(arg->type == Token::TUPLE || arg->type == Token::NIL);
    }
    if (name == "Isfunction")
        return Token::makeBool(arg->type == Token::LAMBDA || arg->type == Token::PARTIAL);
    if (name == "Isdummy")
        return Token::makeBool(arg->type == Token::DUMMY);
    if (name == "Isnil")
        return Token::makeBool(arg->type == Token::NIL ||
                               (arg->type == Token::TUPLE && arg->tupleElems.empty()));
    if (name == "Null")
        return Token::makeBool(arg->type == Token::NIL ||
                               (arg->type == Token::TUPLE && arg->tupleElems.empty()));
    if (name == "Order")
    {
        if (arg->type == Token::NIL)
            return Token::makeInt(0);
        if (arg->type != Token::TUPLE)
            throw runtime_error("Order: not a tuple");
        return Token::makeInt(arg->tupleElems.size());
    }
    if (name == "Stem")
    {
        if (arg->type != Token::STR || arg->strVal.empty())
            throw runtime_error("Stem: not a non-empty string");
        return Token::makeStr(string(1, arg->strVal[0]));
    }
    if (name == "Stern")
    {
        if (arg->type != Token::STR)
            throw runtime_error("Stern: not a string");
        return Token::makeStr(arg->strVal.size() > 1 ? arg->strVal.substr(1) : "");
    }
    if (name == "ItoS")
    {
        if (arg->type != Token::INT)
            throw runtime_error("ItoS: not an int");
        return Token::makeStr(to_string(arg->intVal));
    }
    if (name == "not")
    {
        if (arg->type != Token::BOOL)
            throw runtime_error("not: not a bool");
        return Token::makeBool(!arg->boolVal);
    }
    if (name == "neg")
    {
        if (arg->type != Token::INT)
            throw runtime_error("neg: not an int");
        return Token::makeInt(-arg->intVal);
    }
    if (name == "Conc" || name == "+" || name == "-" || name == "*" || name == "/" ||
        name == "**" || name == "gr" || name == "ge" || name == "ls" || name == "le" ||
        name == "eq" || name == "ne" || name == "&" || name == "or" || name == "aug")
    {
        auto *p = new Token;
        p->type = Token::PARTIAL;
        p->builtinName = name;
        p->partialArg = arg;
        return p;
    }
    throw runtime_error("Unknown builtin: " + name);
}

// CSE Rule 6: binary builtins are evaluated in two stages via a partial token.
// The first operand is stored inside a PARTIAL token, and the second operand is
// combined later once the machine sees the second gamma application.
static Token *applyPartial(Token *partial, Token *arg2)
{
    const string &name = partial->builtinName;
    Token *arg1 = partial->partialArg;

    if (name == "+")
    {
        if (arg1->type == Token::INT && arg2->type == Token::INT)
            return Token::makeInt(arg1->intVal + arg2->intVal);
        if (arg1->type == Token::STR && arg2->type == Token::STR)
            return Token::makeStr(arg1->strVal + arg2->strVal);
        throw runtime_error("+: type mismatch");
    }
    if (name == "-")
        return Token::makeInt(arg1->intVal - arg2->intVal);
    if (name == "*")
        return Token::makeInt(arg1->intVal * arg2->intVal);
    if (name == "/")
        return Token::makeInt(arg1->intVal / arg2->intVal);
    if (name == "**")
        return Token::makeInt((int)pow(arg1->intVal, arg2->intVal));
    if (name == "gr")
        return Token::makeBool(arg1->intVal > arg2->intVal);
    if (name == "ge")
        return Token::makeBool(arg1->intVal >= arg2->intVal);
    if (name == "ls")
        return Token::makeBool(arg1->intVal < arg2->intVal);
    if (name == "le")
        return Token::makeBool(arg1->intVal <= arg2->intVal);
    if (name == "eq")
    {
        if (arg1->type == Token::INT && arg2->type == Token::INT)
            return Token::makeBool(arg1->intVal == arg2->intVal);
        if (arg1->type == Token::STR && arg2->type == Token::STR)
            return Token::makeBool(arg1->strVal == arg2->strVal);
        if (arg1->type == Token::BOOL && arg2->type == Token::BOOL)
            return Token::makeBool(arg1->boolVal == arg2->boolVal);
        throw runtime_error("eq: type mismatch");
    }
    if (name == "ne")
    {
        if (arg1->type == Token::INT && arg2->type == Token::INT)
            return Token::makeBool(arg1->intVal != arg2->intVal);
        if (arg1->type == Token::STR && arg2->type == Token::STR)
            return Token::makeBool(arg1->strVal != arg2->strVal);
        if (arg1->type == Token::BOOL && arg2->type == Token::BOOL)
            return Token::makeBool(arg1->boolVal != arg2->boolVal);
        throw runtime_error("ne: type mismatch");
    }
    if (name == "&")
        return Token::makeBool(arg1->boolVal && arg2->boolVal);
    if (name == "or")
        return Token::makeBool(arg1->boolVal || arg2->boolVal);
    if (name == "aug")
    {
        vector<Token *> elems;
        if (arg1->type == Token::TUPLE)
            elems = arg1->tupleElems;
        else if (arg1->type != Token::NIL)
            elems.push_back(arg1);
        if (arg2->type == Token::TUPLE)
            for (auto *e : arg2->tupleElems)
                elems.push_back(e);
        else if (arg2->type != Token::NIL)
            elems.push_back(arg2);
        return Token::makeTuple(elems);
    }
    if (name == "Conc")
    {
        if (arg1->type != Token::STR || arg2->type != Token::STR)
            throw runtime_error("Conc: not strings");
        return Token::makeStr(arg1->strVal + arg2->strVal);
    }
    throw runtime_error("Unknown partial: " + name);
}

// Drive the control, stack, and environment evaluation loop.
// This is the core CSE machine implementation. The control list holds the
// next tokens to process, the stack holds intermediate values, and the current
// environment tracks lexical scope for names and closures.
struct CSEMachine
{
    vector<Delta> &deltas;
    EnvStore envStore;
    vector<Token *> control;
    vector<Token *> stack;
    int currentEnv;

    // Initialize the machine with the root environment and delta 0.
    // e0 is the root environment from which all other scopes descend.
    CSEMachine(vector<Delta> &d) : deltas(d)
    {
        int e0 = envStore.newEnv(-1);
        currentEnv = e0;

        // The initial stack marker is used by Rule 5 when returning from a call.
        // A matching marker is also inserted into control so the first execution
        // frame has the same structure as any later function call frame.
        stack.push_back(Token::makeEnvMarker(e0));

        loadDelta(0);

        // Enter the initial environment before executing the first delta.
        // This makes the first evaluation step behave like a top-level call.
        control.insert(control.begin(), Token::makeEnvMarker(e0));
    }

    Token *ctrlPop()
    {
        if (control.empty())
            throw runtime_error("Control underflow");
        Token *t = control.back();
        control.pop_back();
        return t;
    }
    Token *stkPop()
    {
        if (stack.empty())
            throw runtime_error("Stack underflow");
        Token *t = stack.back();
        stack.pop_back();
        return t;
    }
    void stkPush(Token *t) { stack.push_back(t); }
    void ctrlPush(Token *t) { control.push_back(t); }

    // Push a delta body onto the control list in reverse order.
    // The machine pops from the back, so reversing here preserves execution
    // order without needing an explicit stack during every application.
    void loadDelta(int idx)
    {
        const Delta &d = deltas[idx];
        for (int i = (int)d.size() - 1; i >= 0; i--)
            control.push_back(d[i]);
    }

    // Keep stepping until the control list is empty.
    // Each branch below corresponds to one family of CSE machine rules.
    void run()
    {
        while (!control.empty())
        {
            Token *ctrl = ctrlPop();

            if (ctrl->type == Token::NAME)
            {
                // CSE Rule 1: names are resolved in the current environment chain.
                // Builtins are treated separately because they are not stored in
                // the lexical environment like ordinary program variables.
                const string &name = ctrl->strVal;
                if (name.empty())
                {
                    cerr << "[ErrorDiag] Empty NAME token encountered. currentEnv=" << currentEnv << "\n";
                    cerr << "[ErrorDiag] Control (top->bottom):\n";
                    for (int i = (int)control.size() - 1; i >= 0; --i)
                        cerr << "  " << control[i]->toString() << "\n";
                    cerr << "[ErrorDiag] Stack (top->bottom):\n";
                    for (int i = (int)stack.size() - 1; i >= 0; --i)
                        cerr << "  " << stack[i]->toString() << "\n";
                    throw runtime_error("Empty NAME token encountered");
                }
                if (isBuiltin(name))
                {
                    stkPush(ctrl);
                }
                else
                {
                    stkPush(envStore.lookup(name, currentEnv));
                }
                continue;
            }

            if (ctrl->type == Token::LAMBDA)
            {
                // CSE Rule 2: lambda control becomes a closure with the current env.
                // Capturing currentEnv here is what gives the machine lexical
                // scoping instead of dynamic scoping.
                Token *closure = new Token(*ctrl);
                closure->envId = currentEnv;
                stkPush(closure);
                continue;
            }

            if (ctrl->type == Token::INT ||
                ctrl->type == Token::STR ||
                ctrl->type == Token::BOOL ||
                ctrl->type == Token::NIL ||
                ctrl->type == Token::DUMMY ||
                ctrl->type == Token::TUPLE)
            {
                stkPush(ctrl);
                continue;
            }

            if (ctrl->type == Token::YSTAR)
            {
                // Y* is treated as a control token so recursive functions can be built.
                // The actual recursive expansion is deferred until application time.
                stkPush(ctrl);
                continue;
            }

            if (ctrl->type == Token::GAMMA)
            {
                // CSE Rule 3: gamma triggers application of a rator/rand pair.
                // The machine pops the function/operator first, then the operand,
                // and delegates the actual semantics to applyRator().
                Token *rator = stkPop();
                Token *rand = stkPop();
                applyRator(rator, rand);
                continue;
            }

            if (ctrl->type == Token::ENV_MARKER)
            {
                // CSE Rule 5: restore the caller environment after a function body.
                // The marker records which environment should become current again
                // once the callee has finished and left its result on the stack.
                Token *value = stkPop();
                Token *marker = stkPop();
                if (marker->type != Token::ENV_MARKER)
                    throw runtime_error("Rule 5: expected env marker on stack");

                currentEnv = marker->intVal;
                stkPush(value);
                continue;
            }

            if (ctrl->type == Token::BETA)
            {
                // CSE Rule 8: choose the then or else delta based on the boolean cond.
                // beta is the machine's conditional jump instruction.

                int thenIdx = ctrl->intVal;
                int elseIdx = ctrl->deltaIdx;
                Token *cond = stkPop();
                if (cond->type != Token::BOOL)
                {
                    throw runtime_error("Beta: condition is not boolean");
                }
                int chosenIdx = cond->boolVal ? thenIdx : elseIdx;
                loadDelta(chosenIdx);
                continue;
            }

            if (ctrl->type == Token::TAU)
            {
                // CSE Rule 9: collect n values from the stack into a tuple.
                // The values are popped in reverse and reassembled in source order.
                int n = ctrl->intVal;
                vector<Token *> elems(n);
                for (int i = n - 1; i >= 0; i--)
                    elems[i] = stkPop();
                stkPush(Token::makeTuple(elems));
                continue;
            }

            throw runtime_error("Unhandled control token: " + ctrl->toString());
        }
    }

    // Apply one operator/value pair according to the machine rules.
    // This function centralizes the behavior for lambda application, tuple
    // selection, recursion, and builtin dispatch.
    void applyRator(Token *rator, Token *rand)
    {

        if (rator->type == Token::YSTAR)
        {
            // Y* turns a lambda into an eta-recursive closure.
            // This is how rec declarations are turned into self-referential
            // functions without the evaluator needing direct name substitution.

            if (rand->type != Token::LAMBDA)
                throw runtime_error("Y*: operand is not a function");
            stkPush(Token::makeEta(rand));
            return;
        }

        if (rator->type == Token::ETA)
        {
            // Eta expands recursive application back into a normal gamma sequence.
            // The closure is re-applied to its argument, then gamma is scheduled
            // twice so the recursive function body is evaluated in the usual way.
            if (!rator->etaClosure || rator->etaClosure->type != Token::LAMBDA)
                throw runtime_error("eta: missing underlying lambda closure");

            Token *arg = rand;
            Token *lam = rator->etaClosure;

            stkPush(arg);
            stkPush(rator);
            stkPush(lam);

            ctrlPush(Token::makeGamma());
            ctrlPush(Token::makeGamma());
            return;
        }

        if (rator->type == Token::TUPLE)
        {
            // CSE Rule 10: tuple selection by 1-based index.
            // The index is checked carefully because RPAL tuples are 1-based,
            // not 0-based, and an invalid index should fail loudly.
            if (rand->type != Token::INT)
                throw runtime_error("Tuple index must be integer");
            int idx = rand->intVal;
            if (idx < 1 || idx > (int)rator->tupleElems.size())
                throw runtime_error("Tuple index out of range: " + to_string(idx));
            stkPush(rator->tupleElems[idx - 1]);
            return;
        }

        if (rator->type == Token::NAME && isBuiltin(rator->strVal))
        {
            // Unary builtins are executed immediately once their argument is known.
            // This is the fast path for builtins such as Print, not, neg, and the
            // type-checking predicates.
            stkPush(applyBuiltin(rator->strVal, rand));
            return;
        }

        if (rator->type == Token::PARTIAL)
        {
            // Binary builtins wait for their second operand before producing a result.
            // The PARTIAL token stores the first operand and the operator name so
            // the second application can finish the computation.
            stkPush(applyPartial(rator, rand));
            return;
        }

        if (rator->type == Token::LAMBDA)
        {
            // CSE Rule 4 and Rule 11: create a new environment and bind the argument(s).
            // A fresh environment is created for each call so parameters do not
            // leak into surrounding scopes and recursive calls remain isolated.
            int newEnvId = envStore.newEnv(rator->envId);

            if (rator->isNary())
            {
                // N-ary lambdas bind each formal parameter to the matching tuple element.
                // This corresponds to Rule 11, where one tuple argument supplies the
                // values for a whole parameter list.
                if (rand->type != Token::TUPLE ||
                    (int)rand->tupleElems.size() != rator->arity())
                    throw runtime_error("N-ary lambda: argument count mismatch");
                for (int i = 0; i < rator->arity(); i++)
                    envStore.bind(newEnvId, rator->params[i],
                                  rand->tupleElems[i]);
            }
            else
            {

                if (rator->param.empty())
                    throw runtime_error("Lambda has empty parameter name");
                // Single-argument lambdas bind the incoming value directly to the
                // captured parameter name in the new environment.
                envStore.bind(newEnvId, rator->param, rand);
            }

            int savedEnv = currentEnv;

            currentEnv = newEnvId;

            Token *marker = Token::makeEnvMarker(newEnvId);

            marker->intVal = savedEnv;
            stkPush(marker);

            int bodySize = deltas[rator->deltaIdx].size();
            loadDelta(rator->deltaIdx);
            if (bodySize > 0)
            {
                auto it = control.end() - bodySize;
                control.insert(it, marker);
            }
            else
            {
                ctrlPush(marker);
            }
            return;
        }

        throw runtime_error("Cannot apply: " + rator->toString() + " to " + rand->toString());
    }

    // Return the top-most non-environment value left on the stack.
    // The stack may still contain env markers after execution, so this scans
    // backward to find the actual result value the caller cares about.
    Token *result()
    {
        if (stack.empty())
            throw runtime_error("Empty stack at end");
        for (int i = (int)stack.size() - 1; i >= 0; i--)
            if (stack[i]->type != Token::ENV_MARKER)
                return stack[i];
        throw runtime_error("No value on stack");
    }
};

// Build the control structures and execute the CSE machine for the given AST.
// This is the single entry point used by the rest of the RPAL pipeline.
Token *runCSE(Node *ast)
{
    DeltaBuilder builder;
    builder.build(ast);

    CSEMachine machine(builder.deltas);
    machine.run();
    return machine.result();
}
