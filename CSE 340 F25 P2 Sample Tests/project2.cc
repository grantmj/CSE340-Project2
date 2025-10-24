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
    unordered_set<string> nonTerminals;
    for (const auto &r : g_rules) {
        nonTerminals.insert(r.LHS);
    }

    // Build ordered lists based on first appearance anywhere in grammar
    vector<string> terminalsOrdered;
    vector<string> nonTerminalsOrdered;
    terminalsOrdered.reserve(g_firstAppearance.size());
    nonTerminalsOrdered.reserve(g_firstAppearance.size());

    for (const string &sym : g_firstAppearance) {
        if (nonTerminals.find(sym) != nonTerminals.end()) {
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
}

// Task 3: FIRST sets
void Task3()
{
}

// Task 4: FOLLOW sets
void Task4()
{
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

