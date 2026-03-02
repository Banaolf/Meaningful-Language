# Meaningful Language Documentation
**Version**: ALPHA.0.99.46.3 **License**: BMLL 2.0

Meaningful is an interpreted, dynamically typed language written in C. It features garbage collection, a REPL, and support for complex data structures like lists and dictionaries.

## 1. ***Getting Started***
### Running the Interpreter
The interpreter produces a standalone executable (meaningful.exe).

**REPL (Shell): Run the executable without arguments.**
```bash
meaningful
```
**Run a File: Pass the filename as an argument.**
```bash
meaningful script.mean
```
Flags:
- -v / --version: Display version info.
- -src / --source: Open the GitHub repository.
- -lic / --licence: Open the license file.
## 2. Data Types
Meaningful supports the following core data types:

### Meaningful Language: Data Types

* **Integer**
  * **Description:** Whole numbers
  * **Example:** `42`, `-10`
* **Float**
  * **Description:** Decimal numbers
  * **Example:** `3.14`, `0.01`
* **String**
  * **Description:** Text enclosed in double quotes
  * **Example:** `"Hello World"`
* **Boolean**
  * **Description:** Logical truth values
  * **Example:** `true`, `false`
* **Non**
  * **Description:** Represents the absence of value (Null/Void)
  * **Example:** `non`
* **List**
  * **Description:** Ordered collection of values
  * **Example:** `[1, 2, "three"]`
* **Dict**
  * **Description:** Key-Value pairs (Keys must be strings)
  * **Example:** `{"name": "John", "age": 30}`

## 3. Variables and Assignment
Variables are dynamically typed. You can define them using the set keyword or via implicit assignment.

```meaningful
set x = 10
set name = "Meaningful"
y = 20  :: Implicit assignment is also supported
```
Compound Assignment: 

The language supports compound operators for integers and floats:

```meaningful
x += 1
x -= 5
x *= 2
x /= 2
```
## 4. Operators
**Arithmetic**

- + : Addition (also concatenates Strings)
- - : Subtraction
- * : Multiplication
- / : Division
- // : Integer Division (Floor division)
- ** : Exponentiation (Power)

**Comparison**

- == : Equal to
- != : Not equal to
- <, >, <=, >= : Standard relational operators

**Logical**

- and : Logical AND
- or : Logical OR
- ! : Logical NOT (Unary)

## 5. Control Flow
### If Statements

Executes a block of code if the condition is true. Note: Currently, there is no else or elif keyword implemented.

```meaningful
if x > 5
    print "x is greater than 5"
end
```
### While Loops

Repeats a block of code while the condition is true.

```meaningful
set i = 0
while i < 5
    print i
    set i = i + 1
end
```
break: Exits the loop immediately.

## 6. Functions

Functions are first-class citizens and are defined using the set keyword followed by parentheses.

### Definition
```meaningful
set add(a, b)
    return a + b
end
```
### Calling Functions

```meaningful
set result = add(10, 20)
print result
```

### Native Functions

The language comes with built-in native functions:

input(): Reads a line of text from the user (Always returns a string).
clock(): Returns the program execution time in milliseconds.

## 7. Data Structures

### Lists

Lists are ordered collections created with square brackets [].

```meaningful
set myList = [10, 20, 30]
print myList[0] ; Accessing index 0 (prints 10)

:: Modifying a list
set myList[1] = 99
```

### Dictionaries

Dictionaries are key-value stores created with curly braces {}. Keys must be strings.

```meaningful
set myDict = {"name": "Alice", "role": "Admin"}
print myDict["name"]

:: Modifying/Adding entries
set myDict["score"] = 100
```

## 8. Modules
You can import other Meaningful source files using the Import keyword. The path must be a string.

```meaningful
Import "math_utils.mean"
```

## 9. Syntax Notes
Semicolons: Semicolons ; are tokenized but generally optional as statement terminators in the parser.
Scope: The language uses block scoping (new scopes are created for functions).
Truthiness:
0 and empty strings "" are treated as false.
All other values are treated as true
Comments: Double colon. 
```
print "Something" :: <- THIS IS A COMMENT
```