TODO:
  - [x] Create testing infra
  - [x] Basic operation
    - [x] Report events
    - [x] Support for debugging
  - [x] Keypress support
  - [x] Basic special chars
  - [ ] Escape code parsing (state machine)
  - [ ] Functions/escape sequences controlling cursor movement (including attrib and visibility)
  - [ ] Functions/escape sequences controlling scroll (including new line)
  - [ ] Functions/escape sequences controlling color and formatting (including true color)
    - [ ] Blinking
  - [ ] Functions/escape sequences controlling alternate charsets
  - [ ] Functions/escape sequences controlling save states
  - [ ] Functions/escape sequences controlling mouse
  - [ ] Resize support
  - [ ] Functions/escape sequences for advanced xterm stuff
  - [ ] Unicode support
  - [ ] Selection support


Code organization:
  - Testing infrastructure
  - Hide VT members
  - Header with control codes
  - Character set management
  - State management
  - Attribute management (including true-color)
  - Central functions: control and send events
  - Central functions: memory functions within terminal
  - Central function: scroll to all sides (consider scroll area)
  - Central function: cursor movement
  - Central function: new character
    - State machine
    - Control dirty characters / rows
    - Escape code parsing
  - Key translations

