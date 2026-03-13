# EasyLang — EL1 v2.0

The human-readable programming language that reads like English.
Build real apps. Write real scripts. No compiler. No boilerplate.

---

## Installation

### Linux / macOS
```bash
chmod +x install.sh && ./install.sh
```

### Windows
```batch
install.bat
```

### Manual
```bash
g++ -O3 -std=c++17 -o el src/main.cpp
```

---

## Running
```bash
el script.el
el script.el -faster -optimize
el script.el -noconsole
el -version
```

---

## Language — Full Reference

### Comments
```
# comment
// also comment
```

### Variables
```
let name be "Alice"
let age  be 30
set age to 31
change age by 1
increase age by 5
decrease age by 2
```

### Constants
```
define MAX as 100
define APP_NAME as "MyApp"
```
Built-in: `pi`, `e`, `tau`, `sqrt2`, `infinity`, `newline`, `tab`, `version`

### Output
```
say "Hello!"
write "no newline"
log "info message"
log warn "warning"
log error "error"
```

### Input
```
ask "Name? " into name
let answer be input prompt "Type: "
```

### Strings
```
let t  be trim   of msg
let up be upper  of msg
let lo be lower  of msg
let wl be words  of sentence
let ll be lines  of text
let j  be joined of list by "-"
let p  be split  of "a,b,c" by ","
let n  be length of name
let i  be index  of "x" in name
let c  be count  of "l" in name
let f  be first  of name
let la be last   of name
let m  be format "Hello, {}! Score: {}" with name and score

replace "old" with "new" in myVar

however if s contains "x" then do ... done
however if s starts with "http" then do ... done
however if s ends with ".el" then do ... done
```

### Math
```
let r be math sqrt  of 144
let r be math power of 2 exp 10
let r be math clamp of 150 between 0 and 100
let r be math lerp  of 0 to 100 by 0.5
let r be math abs / floor / ceil / round / sin / cos / tan / log of x
let r be random int from 1 to 100
```

### Types
```
let t be type   of x        # number / string / bool / nil / table / function
let s be string of 42
let n be number of "3.14"
```

### Conditionals
```
however if x is 10 then do ... done
however if x is not 10 then do ... done
however if x is either 1 or 2 or 3 then do ... done
however if x is greater than 5 then do ... done
however if x is less than 100 then do ... done
however if x is greater than or equal to 18 then do ... done
however if name contains "Alice" then do ... done
however if url starts with "https" then do ... done
however if x is 5 then exit
however if x is 5 then do not   # inverted
... also if y is 2 then do ...
... otherwise ...
done
```

### Match
```
match status with
  when "ok" do
    say "Good"
  done
  when "warn" or "error" do
    say "Problem"
  done
  else do
    say "Unknown"
  done
done
```

### Loops
```
repeat 5 times do
  say iteration        # 1-based auto variable
done

loop while x < 10 do
  increase x by 1
done

for i from 1 to 10 do ... done
for i from 0 to 100 step 5 do ... done

for each item in myTable do ... done
for each char in "hello" do ... done
for each value in myDict do
  say key + " = " + value   # key auto-set
done

stop    # break
skip    # continue
```

### Functions
```
make function greet with name do
  say "Hello, " + name + "!"
done

make function add with a and b do
  return a + b
done

run greet with "Alice"
let sum be add(10, 20)
```

### Tables
```
create table items
add "apple" to items
add "key" -> "value" to items
remove 0 from items
sort table items
reverse table items
clear table items
merge table extras into items

let n  be length   of items
let f  be first    of items
let la be last     of items
let ks be keys     of items
let vs be values   of items
let r  be reversed of items
let s  be sorted   of items
let i  be index of "x" in items
let c  be count of "x" in items
```

### File I/O
```
read file "data.txt" into content
write file "out.txt" with "Hello"
append to file "log.txt" with "line\n"
delete file "temp.txt"
file exists "config.json" into exists
list files in "/my/dir" into files
```

### JSON
```
let j be json of myTable
decode json j into parsed
say parsed["key"]
```

### HTTP (requires curl)
```
fetch url "https://api.example.com/data" into response
post to url "https://api.example.com" with body into result
decode json response into data
```

### Error Handling
```
try do
  throw error "Something failed"
catch error as e do
  say "Caught: " + e
done

assert x is greater than 0 or fail "must be positive"
```

### Events
```
on event "startup" do
  say "App started"
done

trigger event "startup"
```

### Modules
```
import "utils.el"
export myFunction
```

### GUI Windows
```
open window "My App" as win

add title  "Welcome"    to win
add label  "Hello!"     to win
add input  "Your name"  to win as nameField
add button "Submit"     to win onclick handleSubmit
add button "Quit"       to win onclick handleQuit

make function handleSubmit do
  say "Submitted!"
done

make function handleQuit do
  exit
done

show window win
set nameField text to "Updated!"
close window win
```
Windows: real WinForms via PowerShell.
Linux/macOS: beautiful interactive TUI.

### Process & Environment
```
run command "mkdir output"
let result be output of "ls -la"
let home   be env of "HOME"
let ts     be timestamp()
sleep 2 seconds
exit
exit with 1
```

---

## Built-in Functions

```
len(x)              upper(s)            lower(s)
split(s, delim)     join(tbl, delim)    replace(s, from, to)
contains(s, n)      startswith(s, p)    endswith(s, p)
substr(s, i, len)   charat(s, i)        charcode(s)
fromcharcode(n)     sqrt(n)             abs(n)
floor(n)            ceil(n)             round(n)
pow(a,b)            min(a,b)            max(a,b)
clamp(n,lo,hi)      random(lo,hi)       randomint(lo,hi)
tostring(x)         tonumber(s)         typeof(x)
time()              timestamp()         encode_json(x)
decode_json(s)      keys(tbl)           values(tbl)
push(tbl, val)      pop(tbl)            env(name)
exec(cmd)
```

---

## CLI Flags

| Flag | Effect |
|---|---|
| `-noconsole` | Suppress all output |
| `-faster` | Skip startup checks |
| `-optimize` | Optimizer hints |
| `-version` | Print version |

---

## App Examples

See the complete examples in the README for:
- **To-Do List App** — interactive CLI app with tables
- **File Word Counter** — file I/O + string ops
- **Dice Roller** — random numbers + functions
- **HTTP JSON Fetcher** — fetch URL + decode JSON
- **GUI Calculator** — window + buttons + inputs
- **Config File Reader** — parse KEY=VALUE files

---

## Versioning

| Version | Status |
|---|---|
| EL1 v2.0 | Current — full app layer |
| EL2 | Planned — async, classes |
| EL3 | Planned — package manager |

---

Made with love — code that reads like words.
