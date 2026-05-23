// standardize.cpp
// Transforms an AST into a Standardized Tree (ST).
// Call: Node* st = standardize(copyTree(ast));
// This file applies the RPAL standardization rules that rewrite source-level
// syntax into the canonical tree shape expected by the CSE machine.

#include <string>
#include <vector>
#include <stdexcept>
#include "tree.h"

// Forward declaration
Node* standardize(Node* n);

// ── helpers ──────────────────────────────────────────────────────────────────

static Node* mkNode(const std::string& label, const std::string& val = "") {
    return new Node(label, val);
}

// Standardize all children in-place, return n unchanged.
static Node* stdChildren(Node* n) {
    for (size_t i = 0; i < n->children.size(); ++i)
        n->children[i] = standardize(n->children[i]);
    return n;
}

// ── rules ────────────────────────────────────────────────────────────────────

Node* standardize(Node* n) {
    if (!n) throw std::runtime_error("standardize: null node");

    const std::string lbl = n->label;   // copy — safe even if n is deleted

    // ── Leaf nodes: nothing to do ─────────────────────────────────────────
    if (lbl == "ID"   || lbl == "INT" || lbl == "STR" ||
        lbl == "true" || lbl == "false" || lbl == "nil" ||
        lbl == "dummy" || lbl == "()")
        return n;

    // ── Rule 1: let ───────────────────────────────────────────────────────
    // let              gamma
    //  / \            /     \
    //  =   P      lambda    E
    // / \          /    \
    // X   E       X      P
    if (lbl == "let") {
      Node* eqNode  = n->children[0];
      Node* bodyE   = n->children[1];

      // ✅ Standardize eqNode FIRST — fcn_form becomes "=" node
      Node* stdEq = standardize(eqNode);

      // Now stdEq is guaranteed to be "=", safe to extract children
      Node* X = stdEq->children[0];
      Node* E = stdEq->children[1];
      Node* P = standardize(bodyE);

      Node* lambda = mkNode("lambda");
      lambda->addChild(X);
      lambda->addChild(P);

      Node* gamma = mkNode("gamma");
      gamma->addChild(lambda);
      gamma->addChild(E);

      stdEq->children.clear(); delete stdEq;
      n->children.clear();      delete n;
      return gamma;
    }

    // ── Rule 2: where ─────────────────────────────────────────────────────
    // where            gamma
    //  / \            /     \
    //  P   =       lambda    E
    //     / \      /    \
    //    X   E    X      P
    // ── Rule 2: where ─────────────────────────────────────────────────────
    if (lbl == "where") {
      Node* bodyP  = n->children[0];
      Node* eqNode = n->children[1];

      Node* P     = standardize(bodyP);
      Node* stdEq = standardize(eqNode);   // ← standardize FIRST
                                              //   handles rec, fcn_form, etc.
                                              //   guaranteed to produce "=" node

      Node* X = stdEq->children[0];
      Node* E = stdEq->children[1];

      Node* lambda = mkNode("lambda");
      lambda->addChild(X);
      lambda->addChild(P);

      Node* gamma = mkNode("gamma");
      gamma->addChild(lambda);
      gamma->addChild(E);

      stdEq->children.clear(); delete stdEq;
      n->children.clear();     delete n;
      return gamma;
    }
    
    // ── Rule 4: within ────────────────────────────────────────────────────
    // within               =
    //  /    \             / \
    // =      =           X2  gamma
    // / \   / \              /    \
    // X1 E1 X2 E2         lambda   E1
    //                     /    \
    //                    X1    E2
    if (lbl == "within") {
      Node* eq1 = n->children[0];
      Node* eq2 = n->children[1];

      Node* std_eq1 = standardize(eq1);   // eq1 may already be freed inside
      Node* std_eq2 = standardize(eq2);

      Node* X1 = std_eq1->children[0];
      Node* E1 = std_eq1->children[1];
      Node* X2 = std_eq2->children[0];
      Node* E2 = std_eq2->children[1];

      Node* lambda = mkNode("lambda");
      lambda->addChild(X1);
      lambda->addChild(E2);

      Node* gamma = mkNode("gamma");
      gamma->addChild(lambda);
      gamma->addChild(E1);

      Node* result = mkNode("=");
      result->addChild(X2);
      result->addChild(gamma);

      // ✅ Only free the shell nodes, NOT their children (already reused above)
      std_eq1->children.clear(); delete std_eq1;
      std_eq2->children.clear(); delete std_eq2;
      n->children.clear();       delete n;
      return result;
    }

    // ── Rule 5: rec ───────────────────────────────────────────────────────
    // rec              =
    //  |              / \
    //  =             X   gamma
    // / \                /    \
    // X   E            Y*    lambda
    //                          /   \
    //                         X     E
    if (lbl == "rec") {
        Node* eqNode = n->children[0];

        Node* stdEq = standardize(eqNode);

        Node* X = stdEq->children[0];
        Node* E = stdEq->children[1];

        // Deep-copy X so it appears on both sides
        Node* Xcopy = new Node(X->label, X->value);
        for (Node* c : X->children)
            Xcopy->addChild(new Node(c->label, c->value));

        Node* lambda = mkNode("lambda");
        lambda->addChild(Xcopy);
        lambda->addChild(E);

        Node* gamma = mkNode("gamma");
        gamma->addChild(mkNode("", "Y*"));
        gamma->addChild(lambda);

        Node* result = mkNode("=");
        result->addChild(X);
        result->addChild(gamma);

        stdEq->children.clear(); delete stdEq;
        n->children.clear();      delete n;
        return result;
    }

    // ── Rule 6: fcn_form ──────────────────────────────────────────────────
    // fcn_form          =
    //  / | \           / \
    // P  V+ E         P  lambda
    //                    /    \
    //                   V1   lambda
    //                        /    \
    //                       V2    ...E
    if (lbl == "fcn_form") {
      // children: [P, V1, V2, ..., Vn, E]  (at least 3 children)
      Node* P    = standardize(n->children[0]);
      Node* body = standardize(n->children.back());  // E is the last child

      // Build right-associative lambda chain from Vn down to V1
      // Parameters are at indices 1 .. size-2  (exclude P at 0 and E at back)
      Node* result = body;
      for (int i = (int)n->children.size() - 2; i >= 1; i--) {  //  stop at size-2
          Node* V   = standardize(n->children[i]);
          Node* lam = mkNode("lambda");
          lam->addChild(V);
          lam->addChild(result);
          result = lam;
      }

      Node* eq = mkNode("=");
      eq->addChild(P);
      eq->addChild(result);

      n->children.clear(); delete n;
      return eq;
    }

    // ── Rule 7: and (simultaneous definitions) ────────────────────────────
    // and                =
    //  |                / \
    // =++              ,   tau
    // / \              |    |
    // X   E           X++  E++
    if (lbl == "and") {
        Node* comma = mkNode(",");
        Node* tau   = mkNode("tau");

        for (Node* child : n->children) {
            // each child must be a "=" node
            Node* X = standardize(child->children[0]);
            Node* E = standardize(child->children[1]);
            comma->addChild(X);
            tau->addChild(E);
            child->children.clear();
            delete child;
        }

        Node* result = mkNode("=");
        result->addChild(comma);
        result->addChild(tau);

        n->children.clear(); delete n;
        return result;
    }

    // ── Rule 8: @ (infix application) ────────────────────────────────────
    // @                gamma
    // /|\             /     \
    // E1 N E2       gamma    E2
    //               /    \
    //              N      E1
    if (lbl == "@") {
        Node* E1 = standardize(n->children[0]);
        Node* N  = standardize(n->children[1]);
        Node* E2 = standardize(n->children[2]);

        Node* inner = mkNode("gamma");
        inner->addChild(N);
        inner->addChild(E1);

        Node* outer = mkNode("gamma");
        outer->addChild(inner);
        outer->addChild(E2);

        n->children.clear(); delete n;
        return outer;
    }

    // ── Rule 9: lambda ────────────────────────────────────────────────────
    // If Vb is a "," (tuple pattern), expand into selector chain.
    // Otherwise just recurse.
    if (lbl == "lambda") {
        Node* V = n->children[0];   // parameter (not yet standardized)
        Node* E = standardize(n->children[1]);

        // Tuple parameter  (,)
        if (V->label == ",") {
            std::string tempName = "_tup";
            Node* Temp = mkNode("ID", tempName);

            Node* body = E;
            // Build inside-out so first element ends up outermost
            for (int i = (int)V->children.size() - 1; i >= 0; --i) {
                Node* Xi = standardize(V->children[i]);
                V->children[i] = nullptr;

                // gamma( gamma(Select, i+1), Temp )
                Node* g1 = mkNode("gamma");
                g1->addChild(mkNode("ID", "Select"));
                g1->addChild(mkNode("INT", std::to_string(i + 1)));

                Node* g2 = mkNode("gamma");
                g2->addChild(g1);
                g2->addChild(mkNode("ID", tempName));

                // gamma( lambda(Xi, body), g2 )
                Node* lam = mkNode("lambda");
                lam->addChild(Xi);
                lam->addChild(body);

                Node* g3 = mkNode("gamma");
                g3->addChild(lam);
                g3->addChild(g2);
                body = g3;
            }
            V->children.clear(); delete V;

            Node* outer = mkNode("lambda");
            outer->addChild(Temp);
            outer->addChild(body);

            n->children.clear(); delete n;
            return outer;
        }

        // Plain single-variable lambda
        Node* lam = mkNode("lambda");
        lam->addChild(standardize(V));
        lam->addChild(E);

        n->children.clear(); delete n;
        return lam;
    }

    // ── Default: recurse into children ────────────────────────────────────
    return stdChildren(n);
}