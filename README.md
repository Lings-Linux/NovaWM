# NovaWM

This is a temporary repository for the "Lings Linux" Organization on GitHub, I myself am a part of it, I'm in charge of making this Window Manager for X11/XWayland, but since GitHub doesn't want to connect to my IDE and git won't push my code I'm going to post it here (temporarily)

# Quick how to use:

- create your config directory and file: ~/.config/novawm/novawm.conf
- and you are basically done
## demo config (default):
master_factor = 0.6
border_width = 2
border_color_active = 0x00ff00
border_color_inactive = 0x222222

bind = SUPER, Return, spawn, kitty
bind = SUPER, Q, killactive
bind = SUPER, F, togglefloating
bind = SUPER, J, focusnext
bind = SUPER, K, focusprev
bind = SUPER, H, shrink
bind = SUPER, L, grow
bind = SUPER SHIFT, E, quit
