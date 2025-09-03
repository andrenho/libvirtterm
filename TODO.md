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

    cursor_invisible=\E[?25l,
    cursor_normal=\E[?12l\E[?25h,
    cursor_visible=\E[?12;25h,
    enter_alt_charset_mode=\E(0,
    enter_am_mode=\E[?7h,
    enter_ca_mode=\E[?1049h,
    enter_insert_mode=\E[4h,
    exit_alt_charset_mode=\E(B,
    exit_am_mode=\E[?7l,
    exit_attribute_mode=\E(B\E[m,
    exit_ca_mode=\E[?1049l,
    exit_insert_mode=\E[4l,
    flash_screen=\E[?5h$<100/>\E[?5l,
    init_2string=\E[\041p\E[?3;4l\E[4l\E>,
    keypad_local=\E[?1l\E>,
    keypad_xmit=\E[?1h\E=,
    memory_lock=\El,
    memory_unlock=\Em,
    meta_off=\E[?1034l,
    meta_on=\E[?1034h,
    parm_index=\E[%p1%dS,
    parm_rindex=\E[%p1%dT,
    print_screen=\E[i,
    prtr_off=\E[4i,
    prtr_on=\E[5i,
    reset_1string=\Ec,
    reset_2string=\E[\041p\E[?3;4l\E[4l\E>,
    restore_cursor=\E8,
    save_cursor=\E7,
    scroll_reverse=\EM,
    set_tab=\EH,
    user6=\E[%i%d;%dR,
    user7=\E[6n,
    user8=\E[?1;2c,
    user9=\E[c,
