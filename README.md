# Meaningful Language

<p align="center">
  <img src="icons/meaningfulD.png" alt="Meaningful Language Logo" width="120"/>
</p>

<p align="center">
  A simple, expressive, and powerful interpreted language written in C.
  <br/>
  Made by <a href="https://github.com/Banaolf">Brwolf (Banaolf)</a>
</p>

<p align="center">
  <img src="https://img.shields.io/badge/license-BMLL2.0-a78bfa?style=flat-square"/>
  <img src="https://img.shields.io/badge/platform-Windows%20%7C%20Linux%20%7C%20macOS%20%7C%20BSD%20%7C%20more-4ade80?style=flat-square"/>
  <img src="https://img.shields.io/badge/status-alpha-fdba74?style=flat-square"/>
</p>

---

## What is Meaningful?

Meaningful is a hand-crafted interpreted language built from scratch in C. It aims to be readable and expressive while giving the programmer real control — including optional pointer access, a garbage collector, type annotations, and more.

---

## Features

- Clean, readable syntax
- Dynamic typing with optional type annotations and conditions
- Functions, lambdas, closures
- Lists, dictionaries, strings, floats, booleans
- `forEach`, `while`, `repeat` loops
- File I/O (`readfile`, `overwrite`, `put`, `createfile`, `deletefile`)
- Import system
- Mark-and-sweep garbage collector
- **Pointers** — with automatic sibling invalidation and runtime warnings
- Type casting with `@`
- Default function parameters
- Ternary operator
- Native functions: `input`, `random`, `sleep`, `length`, `clock`, and more
- Shell / REPL mode
- `--check` mode for editor integration
- Cross-platform

---

## Platform Support

| Platform | Supported |
|----------|-----------|
| Windows (32/64, Cygwin) | ✅ |
| Linux | ✅ |
| macOS | ✅ |
| FreeBSD / NetBSD / OpenBSD / DragonFly | ✅ |
| Android | ✅ |
| Haiku | ✅ |
| Solaris | ✅ |
| AIX | ✅ |
| HP-UX / IRIX / Tru64 | ✅ |
| iOS | ⚠️ (no URL opening) |

---

## Installation

### Linux (Debian/Ubuntu)
Download the `.deb` file from the [releases page](https://github.com/Banaolf/Meaningful-Language/releases) and run:
```bash
sudo apt install ./meaningfulang.deb
```

### Windows
A `.msi` installer is coming soon. In the meantime, compile from source.

### Build from source
```bash
gcc interpreter.c lexer.c parser.c -o meaningful -lm
```

---

## Usage

```bash
meaningful script.mean        # Run a file
meaningful                    # Start the REPL shell
meaningful --check file.mean  # Check for errors (for editor integration)
```

---

## Examples

### Hello World
```meaningful
print "Hello, World!"
```

### Functions with type annotations and default parameters
```meaningful
set greet(name@string, greeting = "Hello")
    print greeting + ", " + name + "!"
end

greet("Brwolf")           :: Hello, Brwolf!
greet("Brwolf", "Hey")    :: Hey, Brwolf!
```

### Pointers
```meaningful
set x = 10
set y = ^x   :: y points to x
set z = ^x   :: z also points to x

print y^     :: 10
y^ = 20
print x      :: 20 — x was changed through y

unset x      :: frees x, y and z are automatically invalidated
             :: Warning: pointer 'y' was invalidated because its target 'x' was freed.
             :: Warning: pointer 'z' was invalidated because its target 'x' was freed.
```

Meaningful is one of the only languages that automatically invalidates and removes all pointers to a variable when it is freed, with runtime warnings — giving you C-like control with a safety net.

### Lambdas
```meaningful
set myDict = {
    "greet": do(name)
        print "Hello, " + name
    end
}

myDict.greet("Brwolf")   :: Hello, Brwolf
```

### Type annotations with conditions
```meaningful
set safeDivide(a@int:positive, b@int:positive)
    return a / b
end
```

---

## Editor Support

A VSCodium/VS Code extension is available with:
- Syntax highlighting
- Snippets
- Language server diagnostics (coming soon)

---

## License

Licensed under [BMLL2.0](LICENSE). See `LICENSE` for details.
