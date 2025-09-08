TODO:
  - [x] Create testing infra
  - [x] Basic operation
    - [x] Report events
    - [x] Support for debugging
  - [x] Keypress support
  - [x] Basic special chars
  - [x] Escape code parsing (state machine)
  - [x] Functions/escape sequences controlling cursor movement (including attrib and visibility)
  - [x] Functions/escape sequences controlling scroll (including new line)
            enter_insert_mode=\E[4h,
            exit_insert_mode=\E[4l,
  - [x] Application mode
    - ESC [ ? 1 h
    - ESC [ ? 1 l
    - \E=
    - \E>
  - [x] Reset
            reset_1string=\Ec,
            reset_2string=\E[\041p\E[?3;4l\E[4l\E>,
  - [x] Functions/escape sequences controlling color and formatting (including true color)
  - [x] Functions/escape sequences controlling alternate charsets
            enter_alt_charset_mode=\E(0,
            enter_am_mode=\E[?7h,
            exit_alt_charset_mode=\E(B,
            exit_am_mode=\E[?7l,
    - [x] Functions/escape sequences controlling cursor
      cursor_invisible=\E[?25l,
      cursor_normal=\E[?12l\E[?25h,
      cursor_visible=\E[?12;25h,
      restore_cursor=\E8,
      save_cursor=\E7,
  - [x] Functions/escape sequences controlling save states
            enter_ca_mode=\E[?1049h,
            exit_ca_mode=\E[?1049l,
  - [x] Add DEC chars
  - [x] Resize support (basic)
  - [x] Application not exiting on exit
      - [x] separate library for PTY
  - [x] Mouse support
      - [x] ncurses
      - [x] Others (vim, etc)
  - [x] Blinking
  - [ ] Other functions
      flash_screen=\E[?5h$<100/>\E[?5l,
  - [ ] Make applications work
    - [x] vim
    - [x] top
    - [x] curses
    - [x] htop
    - [x] dialog
    - [x] cmatrix
    - [x] ncdu
    - [x] mc
      - [x] update window title
      - [x] add text to the event (?)
      - [ ] ENTER in help not working
    - [x] tmux
    - [ ] hollywood
    - [ ] vttest
  - [ ] Test with event update
  - [ ] Bug - backspace on shell
  - [ ] Reduce CPU usage (use cmatrix as example)
  - [ ] Reorganize things

Version 2:
  - [ ] XTerm specific sequences
  - [ ] Functions/escape sequences for advanced xterm stuff
  - [ ] Unicode support
  - [ ] Selection support
  - [ ] 256 color support
  - [ ] icons on console (lsd)
  - [ ] resize while keeping text
  - [ ] refactor example

Maybe someday implement:
    memory_lock=\El,
    memory_unlock=\Em,

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

    scroll_reverse=\EM,
    set_tab=\EH,
    user6=\E[%i%d;%dR,
    user7=\E[6n,
    user8=\E[?1;2c,
    user9=\E[c,
    parm_index=\E[%p1%dS,
    parm_rindex=\E[%p1%dT,
