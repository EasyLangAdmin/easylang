# EasyLang — EL1

<p align="center">
  <img src="icon.svg" width="120" alt="EasyLang Logo"/>
</p>

<p align="center">
  <strong>The human-readable programming language that reads like English.</strong><br/>
  Fast. Simple. Powerful. No compilation. No boilerplate.
</p>

<p align="center">
  <img src="https://img.shields.io/badge/version-EL1%20v1.0.0-e94560?style=for-the-badge"/>
  <img src="https://img.shields.io/badge/runs%20on-.el%20files-f5a623?style=for-the-badge"/>
  <img src="https://img.shields.io/badge/language-C%2B%2B17-16213e?style=for-the-badge"/>
</p>

---

## What is EasyLang?

EasyLang (EL1) is a scripting language designed to be as close to plain English as possible. You write code the way you'd explain it to a person — no cryptic symbols, no arcane syntax rules, no steep learning curve.

```
let name be "Alice"
let age be 25

however if age is greater than 18 then do
  say "Welcome, " + name + "! You are an adult."
otherwise
  say "Sorry, " + name + ". You must be 18 or older."
done
```

That's it. That's real, running code.

---

## Features

- **English-like syntax** — reads naturally, writes naturally
- **No compilation** — run your `.el` files instantly
- **Variables, functions, tables, loops, conditionals**
- **Math, string manipulation, type checking**
- **Blazing fast** — interpreter written in optimised C++17
- **CLI flags** — `-noconsole`, `-faster`, `-optimize`
- **Versioned spec** — EL1, EL2, EL3 ... future-proof
- **Zero dependencies** — single binary, runs anywhere

---

## Installation

### Linux / macOS

```bash
git clone https://github.com/easylangadmin/easylang
cd easylang
chmod +x install.sh
./install.sh
```

### Windows

```batch
git clone https://github.com/easylangadmin/easylang
cd easylang
install.bat
```

### Manual build (any platform with g++ or clang++)

```bash
g++ -O3 -std=c++17 -o el src/main.cpp
# Then move `el` to somewhere on your PATH
```

---

## Running EasyLang

```bash
# Basic run
el script.el

# Suppress all output (for background scripts)
el script.el -noconsole

# Enable optimizer hints
el script.el -optimize

# Skip startup overhead
el script.el -faster

# Combine flags
el script.el -faster -optimize

# Print version
el -version
```

### Auto-run `.el` files (Linux)

```bash
echo ':EasyLang:E::el::/usr/local/bin/el:' | sudo tee /proc/sys/fs/binfmt_misc/register
chmod +x script.el
./script.el   # runs directly!
```

### Auto-run via shebang

Add this as the first line of any `.el` file:

```
#!/usr/bin/env el
```

---

## Language Reference

---

### Comments

```
# This is a comment
// This is also a comment
```

---

### Variables

Use `let` to create a variable, `set` to reassign it.

```
let name be "Alice"
let age be 30
let score be 9.5
let active be true
let nothing be nil

set age to 31
set name to "Bob"
```

#### Change a variable

```
change score by 5         # adds 5
change score by -3        # subtracts 3

increase age by 1         # same as change age by +1
decrease score by 2       # same as change score by -2
```

---

### Defines (Constants)

Defines are named constants evaluated once at definition time.

```
define PI as 3.14159
define MAX_SCORE as 100
define APP_NAME as "MyApp"

say PI          # 3.14159
say MAX_SCORE   # 100
```

---

### Output

```
say "Hello, World!"
say "Your score is " + score
say name + " is " + age + " years old"
```

---

### Input

```
ask "What is your name? " into name
ask "Enter a number: " into number

say "Hello, " + name
say "Double your number: " + (number * 2)
```

---

### Conditionals — `however if`

EasyLang uses `however if` for all conditional logic. Blocks open with `then do` and close with `done`.

#### Basic if

```
however if age is 18 then do
  say "Just turned 18!"
done
```

#### If / else

```
however if score is greater than 50 then do
  say "You passed!"
otherwise
  say "Try again."
done
```

#### If / else-if / else

```
however if score is greater than 90 then do
  say "Grade: A"
also if score is greater than 70 then do
  say "Grade: B"
also if score is greater than 50 then do
  say "Grade: C"
otherwise
  say "Grade: F"
done
```

#### Negative conditions

```
however if name is not "Bob" then do
  say "You are not Bob."
done
```

#### Either / or matching

```
however if day is either "Saturday" or "Sunday" then do
  say "It's the weekend!"
done

however if x is not 1 or 2 or 3 then do
  say "x is something else entirely"
done
```

#### Comparison operators

```
however if age is greater than 18 then do ... done
however if age is less than 65 then do ... done
however if age is greater than or equal to 18 then do ... done
however if age is less than or equal to 64 then do ... done
however if age is equal to 30 then do ... done
however if age is not equal to 30 then do ... done
```

#### Exit on condition

```
however if score is 0 then exit
however if name is not "admin" then exit
```

#### Invert block (do not)

```
however if x is 5 then do not
  say "This runs when x is NOT 5"
done
```

#### Boolean and nil checks

```
however if active is true then do ... done
however if active is false then do ... done
however if result is nil then do ... done
however if name is empty then do ... done
```

---

### Loops

#### Repeat N times

```
repeat 5 times do
  say "Hello!"
done

repeat 10 times do
  say "Iteration: " + iteration    # `iteration` is auto-set
done
```

#### Loop while

```
let count be 1
loop while count < 10 do
  say count
  increase count by 1
done
```

English comparisons also work in loop conditions:

```
loop while count is less than 10 do
  increase count by 1
done
```

#### For each (iterate a table)

```
create table names
add "Alice" to names
add "Bob" to names
add "Charlie" to names

for each person in names do
  say "Hello, " + person
done
```

#### Break and continue

```
repeat 10 times do
  however if iteration is 5 then do
    stop      # break out of loop
  done
  however if iteration is 3 then do
    skip      # continue to next iteration
  done
  say iteration
done
```

---

### Functions

Define a function with `make function`. Call it with `run` or `call`.

#### No parameters

```
make function sayHello do
  say "Hello, World!"
done

run sayHello
```

#### With parameters

```
make function greet with name do
  say "Hello, " + name + "!"
done

run greet with "Alice"
run greet with "Bob"
```

#### Multiple parameters

```
make function add with a and b do
  return a + b
done

let result be add(10, 20)
say result    # 30
```

#### Return values

```
make function square with n do
  return n * n
done

let area be square(7)
say "7 squared is " + area    # 49
```

---

### Tables

Tables are EasyLang's all-purpose data structure — arrays, dictionaries, and mixed collections.

#### Create and add items

```
create table fruits

add "apple" to fruits
add "banana" to fruits
add "cherry" to fruits
```

#### Key-value (dictionary) style

```
create table person

add "name" -> "Alice" to person
add "age"  -> 30      to person
add "city" -> "Paris" to person

say person["name"]    # Alice
say person["age"]     # 30
```

#### Remove an item

```
remove 0 from fruits       # remove index 0
remove "city" from person  # remove key
```

#### Iterate

```
for each fruit in fruits do
  say fruit
done

for each value in person do
  say key + " = " + value    # `key` is auto-set
done
```

---

### Math

#### Arithmetic

```
let a be 10 + 5      # 15
let b be 10 - 3      # 7
let c be 4 * 6       # 24
let d be 20 / 4      # 5
let e be 17 % 5      # 2  (remainder)
let f be (3 + 4) * 2 # 14
```

#### Math functions

```
let r be math sqrt of 144       # 12
let r be math abs of -42        # 42
let r be math floor of 3.9      # 3
let r be math ceil of 3.1       # 4
let r be math round of 3.5      # 4
let r be math sin of 0          # 0
let r be math cos of 0          # 1
let r be math log of 100        # 4.605...
let r be math log10 of 1000     # 3
```

#### Built-in constants (defines)

```
say pi          # 3.14159265...
say e           # 2.71828182...
say tau         # 6.28318530...
say infinity    # ∞
```

---

### Strings

#### Concatenation

```
let full be "Hello" + ", " + "World" + "!"
say full
```

#### Length of a string

```
let name be "Alice"
let length be length of name
say length    # 5
```

#### String from number

```
let n be 42
let s be tostring(n)
say "The number is: " + s
```

---

### Type Checking

```
let x be 42
let t be type of x
say t    # number

let name be "Alice"
let t be type of name
say t    # string
```

Types: `number`, `string`, `bool`, `nil`, `table`, `function`

---

### Built-in Functions

You can call these anywhere in your code:

| Function           | Description                              |
|--------------------|------------------------------------------|
| `say(value)`       | Print to console                         |
| `tostring(value)`  | Convert to string                        |
| `tonumber(value)`  | Convert to number                        |
| `typeof(value)`    | Returns type as string                   |
| `len(value)`       | Length of string or table                |
| `upper(string)`    | Uppercase string                         |
| `lower(string)`    | Lowercase string                         |
| `sqrt(n)`          | Square root                              |
| `abs(n)`           | Absolute value                           |
| `floor(n)`         | Floor                                    |
| `ceil(n)`          | Ceiling                                  |
| `round(n)`         | Round to nearest integer                 |
| `random(min, max)` | Random number between min and max        |
| `time()`           | Current time in milliseconds             |

---

### Miscellaneous

#### Sleep

```
sleep 2 seconds
sleep 500            # 500 milliseconds
```

#### Exit

```
exit                     # exit with code 0
exit with 1              # exit with specific code
```

---

## Full Example Programs

### FizzBuzz

```
let i be 1
loop while i < 101 do
  however if i % 15 is 0 then do
    say "FizzBuzz"
  also if i % 3 is 0 then do
    say "Fizz"
  also if i % 5 is 0 then do
    say "Buzz"
  otherwise
    say i
  done
  increase i by 1
done
```

### Fibonacci

```
make function fibonacci with n do
  however if n is less than or equal to 1 then do
    return n
  done
  return fibonacci(n - 1) + fibonacci(n - 2)
done

let i be 0
repeat 10 times do
  say fibonacci(i)
  increase i by 1
done
```

### Simple Calculator

```
ask "Enter first number: " into a
ask "Enter second number: " into b
ask "Operation (+, -, *, /): " into op

however if op is "+" then do
  say a + b
also if op is "-" then do
  say a - b
also if op is "*" then do
  say a * b
also if op is "/" then do
  however if b is 0 then do
    say "Error: division by zero!"
  otherwise
    say a / b
  done
otherwise
  say "Unknown operation: " + op
done
```

### Countdown Timer

```
let count be 10
loop while count > 0 do
  say count
  sleep 1 seconds
  decrease count by 1
done
say "Blast off!"
```

### Grade Book

```
create table grades

add "Alice" -> 92 to grades
add "Bob"   -> 78 to grades
add "Carol" -> 85 to grades

make function letterGrade with score do
  however if score is greater than or equal to 90 then do
    return "A"
  also if score is greater than or equal to 80 then do
    return "B"
  also if score is greater than or equal to 70 then do
    return "C"
  otherwise
    return "F"
  done
done

for each score in grades do
  let letter be letterGrade(score)
  say key + ": " + score + " (" + letter + ")"
done
```

---

## CLI Flags Reference

| Flag          | Description                                      |
|---------------|--------------------------------------------------|
| `-noconsole`  | Suppress all `say` / `ask` output                |
| `-faster`     | Skip startup checks for quicker boot             |
| `-optimize`   | Pass optimisation hints to the interpreter       |
| `-version`    | Print version info and exit                      |

---

## EL Versioning

EasyLang is versioned per spec:

| Version | Status    | Notes                              |
|---------|-----------|------------------------------------|
| **EL1** | ✅ Current | Core language, stable              |
| EL2     | 🔜 Planned | Modules, imports, async            |
| EL3     | 🔜 Planned | Classes, interfaces, error handling|

Each version is backward-compatible. EL1 scripts will always run on EL2+ interpreters.

---

## File Extension

EasyLang scripts use the `.el` extension.

```
my_script.el
calculator.el
game.el
```

---

## License

EasyLang is released under the MIT License. See `LICENSE` for details.

---

<p align="center">Made with ❤️ for humans who just want to write code that reads like words.</p>
