Code organization:
  - Hide VT members
  - Header with control codes
  - Central functions: control and send events
  - Central functions: memory functions within terminal
  - Central function: scroll to all sides
  - Central function: new character
    - State machine
    - Control dirty characters / rows
  - Key translations


-[x] Create sample application
  - [x] Load and manage font
  - [x] Terminal framebuffer
  - [x] Draw with colors and formatting
  - [x] Connect to shell
- [ ] Terminal
  - [ ] Escape sequences
    - [x] Basic formatting and positioning
  - [ ] Add protections
  - [ ] Encoding
  - [ ] Blink
  - [ ] Resize
  - [ ] Mouse support
  - [ ] Check TODOs in the code
