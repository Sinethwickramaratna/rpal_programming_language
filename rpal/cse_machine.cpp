#include <iostream>
#include <string>
#include <vector>
#include <queue>
#include "tree.h"
using namespace std;

struct Token {
    bool isList;
    string value;
    vector<string> list;
    Token(string v) : isList(false), value(v) {}
    Token(vector<string> l) : isList(true), list(l) {}
};

using Delta = vector<Token>;

static string tokenOf(Node* n) {
    if (n->label == "ID" || n->label == "INT" || n->label == "STR")
        return n->value;
    return n->label;
}

static void walk(Node* node, int deltaIdx,
                 vector<Delta>& deltas, int& counter,
                 queue<pair<int,Node*>>& wl) {
    if (!node) return;

    if (node->label == "lambda") {
        int myIndex = ++counter;
        string param = tokenOf(node->children[0]);

        if ((int)deltas.size() <= myIndex)
            deltas.resize(myIndex + 1);

        // ── Use index to push, not reference — resize-safe ──
        deltas[deltaIdx].push_back(
            Token("lambda_" + to_string(myIndex) + "_" + param));

        wl.push({myIndex, node->children[1]});
        return;
    }

    if (node->label == "->") {
        Node* cond  = node->children[0];
        Node* then_ = node->children[1];
        Node* else_ = node->children[2];

        // Branches get temporary deltas at end of vector
        int thenIdx = deltas.size(); deltas.push_back({});
        walk(then_, thenIdx, deltas, counter, wl);

        int elseIdx = deltas.size(); deltas.push_back({});
        walk(else_, elseIdx, deltas, counter, wl);

        vector<string> thenStrs, elseStrs;
        for (auto& t : deltas[thenIdx]) thenStrs.push_back(t.value);
        for (auto& t : deltas[elseIdx]) elseStrs.push_back(t.value);

        // Remove the temporary branch deltas
        deltas.resize(deltas.size() - 2);

        deltas[deltaIdx].push_back(Token(thenStrs));
        deltas[deltaIdx].push_back(Token(elseStrs));
        deltas[deltaIdx].push_back(Token("beta"));
        walk(cond, deltaIdx, deltas, counter, wl);
        return;
    }

    deltas[deltaIdx].push_back(Token(tokenOf(node)));
    for (Node* child : node->children)
        walk(child, deltaIdx, deltas, counter, wl);
}

vector<Delta> getDeltas(Node* root) {
    queue<pair<int,Node*>> wl;
    vector<Delta> deltas(1);
    int counter = 0;

    walk(root, 0, deltas, counter, wl);

    while (!wl.empty()) {
        auto [idx, bodyNode] = wl.front();
        wl.pop();
        if ((int)deltas.size() <= idx)
            deltas.resize(idx + 1);
        walk(bodyNode, idx, deltas, counter, wl);
    }

    return deltas;
}