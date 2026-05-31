# CICAL

CICAL (C Interpretable Calculator) is a console calculator
capable of handling complex algebraic expressions
and dynamic variable reassignment.

---

### Interpreting pipeline:

```
    "INPUT LINE"
         |
         V
    +---------+     Produces stream of tokens,
    |  LEXER  |     objects parser can easily work with,
    +---------+     from given string.
         |
         V
    +----------+    Analyzes given stream of tokens
    |  PARSER  |    and constructs Abstract Syntax Tree
    +----------+    to represent hierarchical structure
         |
         V
    +----------+    Recursively goes through AST,
    | COMPUTER |    computes numbers, resolves and
    +----------+    assigns variables
         |
         V
     (res, err)
```

---

### Parsing

LL parser is used in this project. Grammar is written in Extended Backus-Naur form in `grammar.ebnf`
