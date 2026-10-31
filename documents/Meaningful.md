# Meaningful Language Documentation
**License**: BMLL 2.0

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

| Type        | Description                                                  | Example                           |
| -------------| --------------------------------------------------------------| -----------------------------------|
| **Integer** | Whole numbers                                                | `42`, `-10`                       |
| **Float**   | Decimal numbers                                              | `3.14`, `0.01`                    |
| **String**  | Text enclosed in double quotes                               | `"Hello World"`                   |
| **Boolean** | Logical truth values (stored as 1/0)                         | `true`, `false`                   |
| **Non**     | Represents the absence of a value (Null/Void)                | `non`                             |
| **List**    | Ordered collection of values                                 | `[1, 2, "three"]`                 |
| **Dict**    | Key-value pairs (keys must be strings)                       | `{"name": "John", "age": 30}`     |
| **File**    | A file instance that contains its path(when closed and open) | `readfile "C:\\that.txt" as file` |
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

### Type Conversion

Variables can also be changed types when needed. Use @ for this.

```meaningful
set i = "0"
i = i@int :: Convert i into an int, as long as its valid.
represent i ::Should print just a 0
```

Note that conversion from dictionaries and lists to either floats or ints just gives the lengths of the object.

## Unset

If you no longer need an object and you want it gone, you can unset it!

```meaningful
set i = 0
::Do stuff with i
unset i
print i ::WILL throw IdentifierNotFoundException
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

### Running shell commands

Use $ for this task. Like this:
```meaningful
$"echo Hi" ::If the command has an error, it will fail silently. A good practice is to do the following:

set v = $"echo Hi" ::Can also wrap it in a set to get the exit code
if v isnt 0
  print "Something went wrong"
end
```

### ForEach

For each allows you to iterate through objects, like this:

```meaningful
set str = "Hello, World!"
forEach character in str
  if character.isSpace() ::Still treated as strings
    print "Just a space"
  else character.isAlphabetical()
    print "Its alphabetical"
  else character.isDigit()
    print "Its a digit!"
  end
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

You can also check the type of the argument and add a little condition. Plans on letting the user add their own conditions exist.

```meaningful
set divide(a@int, b@int:not_zero)
    return a / b
end
```

### Calling

```meaningful
set result = add(10, 20)
print result
```

### Scope

Functions create their own scope. Variables defined inside a function are not accessible outside it.

### Lambdas

Functions without a name, generally used for adding methods to dictionaries, which mimick classes.

```meaningful
set RedApple {
  "green": false,
  "makeGreen": do(x@bool)
    self.green = x
  end, ::Or also do(x@bool); self.green = x; end,
  "hasAWorm": false,
  "changeWormState": do(x@bool); self.hasAWorm = x; end
}
RedApple.makeGreen(true)
print RedApple.green
```

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

## 12. File I/O

### readfile

Use it to open and read files. 

```meaningful
readfile "C:\\Users\\User\\test.txt" as file ::as file being optional but strongly recommended. Since you need it to actually get the contents of the file.
  print file.contents()
  overwrite ""
end
```

### File Interactions

| Keyword   | Use               |
| -----------| -------------------|
| overwrite | Overwrites a file |
| put       | Appends to a file.|

---
## 13. Comments

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

## 14. Pointers

A pointer is the memory adress where its contents live. Example to understand it:
```meaningful
set x = 10 ::Set the variable 'x' to 10
set y = ^x ::Set y to be a pointer of x, so the memory adress (Where the variable x lives).

y^ = 9 ::Reassign y using the pointer after the variable does not give the memory adress, but its contents. In here, x would also be 9
unset y ::This is how you 'free' a pointer. It is not needed since the GC already does this, but a good practice.

set z = ^x :: Assign another pointer, remember x is now 9
z^ = 10 :: Set x to 10 again
unset z^ ::What this will do is unset x, itself and all pointers pointing into x.
```

That simple. Think of pointers as a link to the variable they are pointing to.
Pointers are optional. They can be of used for functions that change the variable they are given, too!

```meaningful
set x = "this is a string"
set y = ^x
set changeX(y@pointer) ::Not needed to add the type checking but really good practice as it assures you get that type or an exception is thrown.
  y^ = "This is another string now!" :: This changes because again, y is basically a link to x, so if y^ is changed, x does too
end

changeX(y)
print y ::This should print the memory address (example: 0x52A132F3)
unset y
print x ::Should print "This is another string now!".
```

---

## 15. Syntax Notes

- **Semicolons** `;` are optional statement terminators.
- **Booleans** `true` and `false` are stored internally as `1` and `0`.
- **`non`** represents the absence of a value (similar to null/nil/None in other languages).
- **`end`** closes `if`, `while`, `repeat`, `readfile`, function definition blocks, and more.

---

## 16. Exit Codes

| Code | Meaning         |
| ------| -----------------|
| `0`  | Success         |
| `1`  | General failure |
| `2`  | File not found  |
| `3`  | Malformed flags |
| `4`  | Runtime error   |
| `5`  | Parser error    |
| `6`  | Lexer error     |