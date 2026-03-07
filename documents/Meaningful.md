# Meaningful Language Documentation
**Version**: ALPHA.0.99.49.2 **License**: BMLL 2.0

Meaningful is an interpreted, dynamically typed language written in C. It features garbage collection, a REPL, and support for complex data structures like lists and dictionaries.

---

## 1. Getting Started

### Running the Interpreter

The interpreter produces a standalone executable (`meaningful.exe`).

**REPL (Shell): Run the executable without arguments.**
```bash
meaningful
```

**Run a File: Pass the filename as an argument.**
```bash
meaningful script.mean
```

**Flags:**
- `-v` / `--version` — Display version info.
- `-src` / `--source` — Open the GitHub repository.
- `-lic` / `--licence` — Open the license file.
- `-t` / `--test [file].mean` — Run a file in test mode. Exits with code 0 on success, 4 on runtime error.

---

## 2. Data Types

Meaningful supports the following core data types:

| Type | Description | Example |
|------|-------------|---------|
| **Integer** | Whole numbers | `42`, `-10` |
| **Float** | Decimal numbers | `3.14`, `0.01` |
| **String** | Text enclosed in double quotes | `"Hello World"` |
| **Boolean** | Logical truth values (stored as 1/0) | `true`, `false` |
| **Non** | Represents the absence of a value (Null/Void) | `non` |
| **List** | Ordered collection of values | `[1, 2, "three"]` |
| **Dict** | Key-value pairs (keys must be strings) | `{"name": "John", "age": 30}` |

---

## 3. Variables and Assignment

Variables are dynamically typed. Use the `set` keyword or implicit assignment.

```meaningful
set x = 10
set name = "Meaningful"
y = 20  :: Implicit assignment is also valid
```

### Compound Assignment

```meaningful
x += 1
x -= 5
x *= 2
x /= 2
```

---

## 4. Operators

### Arithmetic

| Operator | Description |
|----------|-------------|
| `+` | Addition (also concatenates strings) |
| `-` | Subtraction |
| `*` | Multiplication |
| `/` | Division |
| `//` | Integer (floor) division |
| `**` | Exponentiation |

### Comparison

| Operator | Description |
|----------|-------------|
| `==` or `equals` | Equal to |
| `!=` or `isnt` | Not equal to |
| `<` | Less than |
| `>` | Greater than |
| `<=` | Less than or equal to |
| `>=` | Greater than or equal to |

`equals` and `isnt` are keyword aliases for `==` and `!=`, useful for more readable conditions.

### Logical

| Operator | Description |
|----------|-------------|
| `and` | Logical AND |
| `or` | Logical OR |
| `!` | Logical NOT (unary) |

### Operator Precedence (highest to lowest)

1. Unary `!`
2. `**` (right-associative)
3. `*`, `/`, `//`
4. `+`, `-`
5. `<`, `>`, `<=`, `>=`, `==`, `!=`, `equals`, `isnt`
6. `and`, `or`

---

## 5. Truthiness

The following values are **falsy**:

- `0` (integer zero)
- `0.0` (float zero)
- `""` (empty string)
- `non`

Everything else is **truthy**.

---

## 6. Control Flow

### If / Else

```meaningful
if x > 5
    print "x is big"
else x == 5
    print "x is exactly 5"
else
    print "x is small"
end
```

- `else` on the **same line** as a condition behaves as `else if`.
- `else` on its **own line** (no condition) is a plain fallback and always executes.
- Only one plain `else` should appear, at the end of the chain.

### While Loops

```meaningful
set i = 0
while i < 5
    print i
    i += 1
end
```

Use `break` to exit a loop early.

```meaningful
while true
    if done
        break
    end
end
```

### Repeat Loops

Repeats a block a fixed number of times. The count must be greater than 0.

```meaningful
repeat 3
    print "Hello!"
end
```

---

## 7. Functions

Functions are defined using `set` followed by a name and parentheses.

### Definition

```meaningful
set add(a, b)
    return a + b
end
```

### Calling

```meaningful
set result = add(10, 20)
print result
```

### Scope

Functions create their own scope. Variables defined inside a function are not accessible outside it.

---

## 8. Data Structures

### Lists

Ordered collections created with `[]`.

```meaningful
set myList = [10, 20, 30]
print myList[0]      :: prints 10

set myList[1] = 99   :: modify by index
```

### Dictionaries

Key-value stores created with `{}`. Keys must be strings.

```meaningful
set myDict = {"name": "Alice", "role": "Admin"}
print myDict["name"]           :: prints Alice

set myDict["score"] = 100      :: add or update an entry
```

---

## 9. String Methods

Methods are called with dot syntax: `"string".method()`.

| Method | Description | Example |
|--------|-------------|---------|
| `.isDigit()` | Returns `1` if the string contains only digits, `0` otherwise. Returns `0` for empty strings. | `"123".isDigit()` → `1` |

---

## 10. Native Functions

These built-in functions are available globally.

| Function    | Arguments | Returns | Description                                                                          |     |
| -------------| -----------| ---------| --------------------------------------------------------------------------------------| -----|
| `input()`   | none      | String  | Reads a line of text from the user.                                                  |     |
| `clock()`   | none      | Integer | Returns program execution time in milliseconds.                                      |     |
| `length(x)` | Any value | Integer | Returns the length of a string, list, or dict. For numbers, returns the digit count. |     |

---

## 11. Modules / Import

Import another Meaningful source file. The path must be a string. Imported files are executed in the current scope.

```meaningful
Import "math_utils.mean"
```

---

## 12. Comments

**Single-line:** `::` — Everything after `::` on the same line is ignored.

```meaningful
print "Hello" :: This is a comment
```

**Block comments:** `:;` to open, `;:` to close.

```meaningful
:;
This is a block comment.
It spans multiple lines.
;:
```

---

## 13. Syntax Notes

- **Semicolons** `;` are optional statement terminators.
- **Booleans** `true` and `false` are stored internally as `1` and `0`.
- **`non`** represents the absence of a value (similar to null/nil/None in other languages).
- **`end`** closes `if`, `while`, `repeat`, and function definition blocks.

---

## 14. Exit Codes

| Code | Meaning         |
| ------| -----------------|
| `0`  | Success         |
| `1`  | General failure |
| `2`  | File not found  |
| `3`  | Malformed flags |
| `4`  | Runtime error   |
| `5`  | Parser error    |
| `6`  | Lexer error     |