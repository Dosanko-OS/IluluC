# 🩷 IluluC

Simple, expressive and beginner-friendly programming language inspired by C, Go and Lua.

## Features

- 🩷 Readable syntax
- ⚡ C backend
- 🐉 Pattern matching
- 📦 Structs and enums
- 🔍 Semantic analysis
- ✅ Type checking
- ❤️ Open source

## Hello, world!

```ilc
write "Hello, world!"
```

Compile:

```bash
ilulu comp hello.ilc
```

Run:

```bash
ilulu run hello.ilc
```

## Installation

### Arch Linux

```bash
sudo pacman -U iluluc-1.0-1-x86_64.pkg.tar.zst
```

## Commands

```bash
ilulu help
ilulu version
ilulu comp <file.ilc>
ilulu run <file.ilc>
ilulu c <file.ilc>
ilulu ast <file.ilc>
ilulu tokens <file.ilc>
ilulu fmt <file.ilc>
ilulu init
```

## Architecture

```
.ilc
 ↓
Lexer
 ↓
Parser
 ↓
AST
 ↓
Semantic Analysis
 ↓
Type Checker
 ↓
C Generator
 ↓
output.c
 ↓
gcc / clang
 ↓
Executable
```

## Long-term goals

- Self-hosting compiler
- Package manager
- Modules
- Better diagnostics
- IluluOS 😈🐉

---

Made with ❤️ by humans.
