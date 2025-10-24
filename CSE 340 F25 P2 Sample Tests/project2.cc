/*
 * Copyright (C) Mohsen Zohrevandi, 2017
 *               Rida Bazzi 2019
 * Do not share this file with anyone
 */
#include <iostream>
#include <cstdio>
#include <cstdlib>
#include <vector>
#include <string>
#include <unordered_set>
#include <unordered_map>
#include "lexer.h"

using namespace std;

// -------------------- Grammar Data Structures --------------------
struct Rule {
    string LHS;
    vector<string> RHS; // empty RHS represents epsilon
};

static vector<Rule> g_rules;           // all rules expanded (one per alternative)
static LexicalAnalyzer g_lexer;        // provided lexer (reads stdin at construction)
static vector<string> g_firstAppearance;      // IDs in first-appearance order
static unordered_set<string> g_seenAppearance; // to guard first-appearance
static unordered_set<string> g_nonTerminals;   // LHS symbols
static unordered_map<string, unordered_set<string>> g_FIRST; // FIRST sets per symbol name
static string g_startSymbol;            // start symbol = LHS of first rule

// -------------------- Parser Utilities --------------------
static void SyntaxErrorAndExit()
{
    cout << "SYNTAX ERROR !!!!!!!!!!!!!!" << "\n";
    exit(1);
}

static Token Expect(TokenType expected)
{
    Token t = g_lexer.GetToken();
    if (t.token_type != expected) {
        SyntaxErrorAndExit();
    }
    return t;
}

static Token Peek(int k = 1)
{
    return g_lexer.peek(k);
}

// forward declarations of recursive-descent nonterminals
static void ParseGrammar();
static void ParseRuleList();
static void ParseRule();
static void ParseRightHandSide(const string &lhs);
static void ParseIdList(vector<string> &out);
static void RecordAppearance(const string &name)
{
    if (g_seenAppearance.find(name) == g_seenAppearance.end()) {
        g_seenAppearance.insert(name);
        g_firstAppearance.push_back(name);
    }
}

// helper to add a rule
static void AddRule(const string &lhs, const vector<string> &rhs)
{
    Rule r;
    r.LHS = lhs;
    r.RHS = rhs; // copy
    g_rules.push_back(std::move(r));
}

// read grammar
void ReadGrammar() {
    // The lexer has already read tokens from stdin upon construction.
    // We now parse per the specified input grammar; on any mismatch we emit the
    // exact syntax error string and exit.
    ParseGrammar();
}

/* 
 * Task 1: 
 * Printing the terminals, then nonterminals of grammar in appearing order
 * output is one line, and all names are space delineated
*/
void Task1()
{
    // Determine set of non-terminals: those that appear as LHS of any rule
    g_nonTerminals.clear();
    for (const auto &r : g_rules) {
        g_nonTerminals.insert(r.LHS);
    }

    // Build ordered lists based on first appearance anywhere in grammar
    vector<string> terminalsOrdered;
    vector<string> nonTerminalsOrdered;
    terminalsOrdered.reserve(g_firstAppearance.size());
    nonTerminalsOrdered.reserve(g_firstAppearance.size());

    for (const string &sym : g_firstAppearance) {
        if (g_nonTerminals.find(sym) != g_nonTerminals.end()) {
            nonTerminalsOrdered.push_back(sym);
        } else {
            terminalsOrdered.push_back(sym);
        }
    }

    // Print terminals first, then non-terminals, space-delimited on one line
    bool first = true;
    for (const string &t : terminalsOrdered) {
        if (!first) cout << ' ';
        cout << t;
        first = false;
    }
    for (const string &nt : nonTerminalsOrdered) {
        if (!first) cout << ' ';
        cout << nt;
        first = false;
    }
    cout << "\n";
}

/*
 * Task 2:
 * Print out nullable set of the grammar in specified format.
*/
void Task2()
{
    // Ensure g_nonTerminals is populated
    if (g_nonTerminals.empty()) {
        for (const auto &r : g_rules) g_nonTerminals.insert(r.LHS);
    }

    // Initialize Nullable set as empty
    unordered_set<string> nullable;

    // Worklist-style fixed-point algorithm (Notes 4, page 31):
    // 1) If A -> epsilon exists (RHS empty), A is nullable
    // 2) If A -> X1 X2 ... Xk and all Xi are nullable non-terminals, then A nullable
    bool changed = true;
    while (changed) {
        changed = false;
        for (const Rule &r : g_rules) {
            // Consider rule r
            if (r.RHS.empty()) {
                if (nullable.insert(r.LHS).second) changed = true;
                continue;
            }

            // All symbols on RHS must be nullable to make LHS nullable.
            bool allNullable = true;
            for (const string &sym : r.RHS) {
                // Terminals are never nullable by definition here
                if (g_nonTerminals.find(sym) == g_nonTerminals.end()) {
                    allNullable = false;
                    break;
                }
                if (nullable.find(sym) == nullable.end()) {
                    allNullable = false;
                    break;
                }
            }
            if (allNullable) {
                if (nullable.insert(r.LHS).second) changed = true;
            }
        }
    }

    // Output in NT appearance order (per spec), with "Nullable = { A , B }" formatting
    cout << "Nullable = { ";
    bool first = true;
    for (const string &sym : g_firstAppearance) {
        if (g_nonTerminals.find(sym) != g_nonTerminals.end()) {
            if (nullable.find(sym) != nullable.end()) {
                if (!first) cout << " , ";
                cout << sym;
                first = false;
            }
        }
    }
    cout << " }\n";
}

// Task 3: FIRST sets
void Task3()
{
    // Prepare non-terminals and terminals universe
    if (g_nonTerminals.empty()) {
        for (const auto &r : g_rules) g_nonTerminals.insert(r.LHS);
    }

    unordered_set<string> nullable;
    // Reuse Task2's logic to compute nullable (could be factored, duplicating for simplicity)
    {
        bool changed = true;
        while (changed) {
            changed = false;
            for (const Rule &r : g_rules) {
                if (r.RHS.empty()) {
                    if (nullable.insert(r.LHS).second) changed = true;
                    continue;
                }
                bool allNullable = true;
                for (const string &sym : r.RHS) {
                    if (g_nonTerminals.find(sym) == g_nonTerminals.end()) { allNullable = false; break; }
                    if (nullable.find(sym) == nullable.end()) { allNullable = false; break; }
                }
                if (allNullable) {
                    if (nullable.insert(r.LHS).second) changed = true;
                }
            }
        }
    }

    // Initialize FIRST for terminals and non-terminals
    g_FIRST.clear();
    // Terminals: FIRST(t) = { t }
    for (const string &sym : g_firstAppearance) {
        if (g_nonTerminals.find(sym) == g_nonTerminals.end()) {
            g_FIRST[sym].insert(sym);
        }
    }
    // Non-terminals: start empty
    for (const string &nt : g_nonTerminals) {
        (void)g_FIRST[nt];
    }

    // Fixed-point: for each rule A -> X1 X2 ... Xk
    // add FIRST(X1) except epsilon; if X1 nullable add FIRST(X2), etc.
    bool changed = true;
    while (changed) {
        changed = false;
        for (const Rule &r : g_rules) {
            const string &A = r.LHS;
            bool allPrevNullable = true;
            if (r.RHS.empty()) {
                // epsilon production: FIRST(A) includes epsilon conceptually, but
                // per project specs, FIRST sets printed contain terminals only.
                // So we do not add epsilon to output sets.
                continue;
            }
            for (size_t i = 0; i < r.RHS.size(); ++i) {
                const string &Xi = r.RHS[i];
                // add FIRST(Xi) (terminals only) to FIRST(A)
                for (const string &t : g_FIRST[Xi]) {
                    if (g_nonTerminals.find(t) == g_nonTerminals.end()) {
                        if (g_FIRST[A].insert(t).second) changed = true;
                    }
                }
                // stop if Xi is not nullable
                if (g_nonTerminals.find(Xi) == g_nonTerminals.end() || nullable.find(Xi) == nullable.end()) {
                    allPrevNullable = false;
                    break;
                }
            }
            (void)allPrevNullable;
        }
    }

    // Print FIRST for each NT in appearance order, elements ordered by appearance order
    for (const string &sym : g_firstAppearance) {
        if (g_nonTerminals.find(sym) != g_nonTerminals.end()) {
            cout << "FIRST(" << sym << ") = { ";
            bool first = true;
            for (const string &orderSym : g_firstAppearance) {
                if (g_nonTerminals.find(orderSym) == g_nonTerminals.end()) {
                    if (g_FIRST[sym].find(orderSym) != g_FIRST[sym].end()) {
                        if (!first) cout << " , ";
                        cout << orderSym;
                        first = false;
                    }
                }
            }
            cout << " }\n";
        }
    }
}

// Task 4: FOLLOW sets
void Task4()
{
    // Ensure NTs
    if (g_nonTerminals.empty()) {
        for (const auto &r : g_rules) g_nonTerminals.insert(r.LHS);
    }
    // Determine start symbol if not set (fallback: first rule's LHS)
    if (g_startSymbol.empty() && !g_rules.empty()) {
        g_startSymbol = g_rules.front().LHS;
    }

    // Compute Nullable
    unordered_set<string> nullable;
    {
        bool changed = true;
        while (changed) {
            changed = false;
            for (const Rule &r : g_rules) {
                if (r.RHS.empty()) {
                    if (nullable.insert(r.LHS).second) changed = true;
                    continue;
                }
                bool allNullable = true;
                for (const string &sym : r.RHS) {
                    if (g_nonTerminals.find(sym) == g_nonTerminals.end()) { allNullable = false; break; }
                    if (nullable.find(sym) == nullable.end()) { allNullable = false; break; }
                }
                if (allNullable) {
                    if (nullable.insert(r.LHS).second) changed = true;
                }
            }
        }
    }

    // Compute FIRST for all symbols (terminals and non-terminals). Store only terminals in FIRST sets.
    g_FIRST.clear();
    for (const string &sym : g_firstAppearance) {
        if (g_nonTerminals.find(sym) == g_nonTerminals.end()) {
            g_FIRST[sym].insert(sym);
        }
    }
    for (const string &nt : g_nonTerminals) {
        (void)g_FIRST[nt];
    }
    bool firstChanged = true;
    while (firstChanged) {
        firstChanged = false;
        for (const Rule &r : g_rules) {
            if (r.RHS.empty()) continue; // epsilon not output in FIRST
            const string &A = r.LHS;
            for (size_t i = 0; i < r.RHS.size(); ++i) {
                const string &Xi = r.RHS[i];
                // add FIRST(Xi) terminals into FIRST(A)
                for (const string &t : g_FIRST[Xi]) {
                    if (g_nonTerminals.find(t) == g_nonTerminals.end()) {
                        if (g_FIRST[A].insert(t).second) firstChanged = true;
                    }
                }
                // stop if Xi not nullable
                if (g_nonTerminals.find(Xi) == g_nonTerminals.end() || nullable.find(Xi) == nullable.end()) {
                    break;
                }
            }
        }
    }

    // FOLLOW computation
    unordered_map<string, unordered_set<string>> FOLLOW;
    for (const string &nt : g_nonTerminals) {
        (void)FOLLOW[nt];
    }
    if (!g_startSymbol.empty()) {
        FOLLOW[g_startSymbol].insert("$");
    }

    bool changed = true;
    while (changed) {
        changed = false;
        for (const Rule &r : g_rules) {
            const string &A = r.LHS;
            const vector<string> &rhs = r.RHS;
            // For each position i with non-terminal Xi
            for (size_t i = 0; i < rhs.size(); ++i) {
                const string &Xi = rhs[i];
                if (g_nonTerminals.find(Xi) == g_nonTerminals.end()) continue; // only for non-terminals

                // Compute FIRST of suffix beta = rhs[i+1..]
                bool suffixNullable = true;
                unordered_set<string> addSet;
                for (size_t j = i + 1; j < rhs.size(); ++j) {
                    const string &Y = rhs[j];
                    if (g_nonTerminals.find(Y) == g_nonTerminals.end()) {
                        // terminal
                        addSet.insert(Y);
                        suffixNullable = false;
                        break;
                    } else {
                        // non-terminal: add its FIRST terminals
                        for (const string &t : g_FIRST[Y]) {
                            if (g_nonTerminals.find(t) == g_nonTerminals.end()) addSet.insert(t);
                        }
                        if (nullable.find(Y) == nullable.end()) {
                            suffixNullable = false;
                            break;
                        }
                    }
                }
                // Add FIRST(beta)\{epsilon} to FOLLOW(Xi)
                for (const string &t : addSet) {
                    if (FOLLOW[Xi].insert(t).second) changed = true;
                }
                // If beta is nullable or beta is empty, add FOLLOW(A)
                if (suffixNullable) {
                    for (const string &t : FOLLOW[A]) {
                        if (FOLLOW[Xi].insert(t).second) changed = true;
                    }
                }
            }
        }
    }

    // Print FOLLOW in appearance order of non-terminals
    for (const string &sym : g_firstAppearance) {
        if (g_nonTerminals.find(sym) != g_nonTerminals.end()) {
            cout << "FOLLOW(" << sym << ") = { ";
            bool firstOut = true;
            // $ first
            if (FOLLOW[sym].find("$") != FOLLOW[sym].end()) {
                cout << "$";
                firstOut = false;
            }
            // then terminals in appearance order
            for (const string &orderSym : g_firstAppearance) {
                if (g_nonTerminals.find(orderSym) == g_nonTerminals.end()) {
                    if (FOLLOW[sym].find(orderSym) != FOLLOW[sym].end()) {
                        if (!firstOut) cout << " , ";
                        cout << orderSym;
                        firstOut = false;
                    }
                }
            }
            cout << " }\n";
        }
    }
}

// Task 5: left factoring
void Task5()
{
}

// Task 6: eliminate left recursion
void Task6()
{
}
    
int main (int argc, char* argv[])
{
    int task;

    if (argc < 2)
    {
        cout << "Error: missing argument\n";
        return 1;
    }

    /*
       Note that by convention argv[0] is the name of your executable,
       and the first argument to your program is stored in argv[1]
     */

    task = atoi(argv[1]);
    
    ReadGrammar();  // Reads the input grammar from standard input
                    // and represent it internally in data structures
                    // ad described in project 2 presentation file

    switch (task) {
        case 1: Task1();
            break;

        case 2: Task2();
            break;

        case 3: Task3();
            break;

        case 4: Task4();
            break;

        case 5: Task5();
            break;
        
        case 6: Task6();
            break;

        default:
            cout << "Error: unrecognized task number " << task << "\n";
            break;
    }
    return 0;
}


// -------------------- Parser Implementation --------------------

static void ParseGrammar()
{
    // Grammar -> Rule-list HASH
    ParseRuleList();
    Expect(HASH);
    // After HASH, no more tokens (except whitespace) are allowed
    if (Peek(1).token_type != END_OF_FILE) {
        SyntaxErrorAndExit();
    }
}

static void ParseRuleList()
{
    // Rule-list -> Rule Rule-list | Rule
    // At least one Rule must be present
    ParseRule();

    // If next token begins another Rule (i.e., next token is ID), parse repeatedly
    while (Peek(1).token_type == ID) {
        ParseRule();
    }
}

static void ParseRule()
{
    // Rule -> ID ARROW Right-hand-side STAR
    Token idTok = Expect(ID);
    RecordAppearance(idTok.lexeme);
    if (g_rules.empty()) {
        g_startSymbol = idTok.lexeme;
    }
    Expect(ARROW);
    ParseRightHandSide(idTok.lexeme);
    Expect(STAR);
}

static void ParseRightHandSide(const string &lhs)
{
    // Right-hand-side -> Id-list | Id-list OR Right-hand-side
    // This means: one or more Id-list separated by OR
    vector<string> rhs;
    ParseIdList(rhs);
    AddRule(lhs, rhs);

    while (Peek(1).token_type == OR) {
        Expect(OR);
        rhs.clear();
        ParseIdList(rhs);
        AddRule(lhs, rhs);
    }
}

static void ParseIdList(vector<string> &out)
{
    // Id-list -> ID Id-list | epsilon
    // epsilon when next is OR or STAR (end of this alternative)
    // Otherwise, one or more IDs
    out.clear();

    Token t = Peek(1);
    if (t.token_type == ID) {
        // consume one or more IDs
        do {
            Token idTok = Expect(ID);
            out.push_back(idTok.lexeme);
            RecordAppearance(idTok.lexeme);
            t = Peek(1);
        } while (t.token_type == ID);
        return;
    }

    // epsilon is allowed only if the lookahead is one of OR or STAR (the caller context)
    if (t.token_type == OR || t.token_type == STAR) {
        // epsilon: leave 'out' empty
        return;
    }

    // otherwise, syntax error
    SyntaxErrorAndExit();
}

