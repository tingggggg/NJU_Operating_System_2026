import sys
import time

def print_sixel():
    # Sixel image: smiley face (6x10)
    # See https://en.wikipedia.org/wiki/Sixel
    # Format: ESC P ... ESC \
    smiley = (
        "\x1bPq"                              # DCS Sixel introducer
        # Yellow color: #ffff00
        "#1;2;255;255;0"                      # Color register 1 = yellow
        # Black color: #000000
        "#2;2;0;0;0"                          # Color register 2 = black

        # 10 columns. Each '!' means repeat.
        # Each letter encodes 6 pixels vertically, LSB = top
        # Face background in yellow, outline in black
        "#1"                                  # Use color 1
        "!2~"                                 # 2 columns, 000000 (bg)
        "#2"                                  # Use color 2
        "~"                                   # 000000 (left eye bg)
        "#1"
        "!4~"                                 # 4 columns bg
        "#2"
        "~"                                   # 000000 (right eye bg)
        "#1"
        "!2~$"                                # 2 columns bg, row sep

        "#1!1~#2!1B#1!2~#2!1B#1!5~$"          # Eyes row
        "#1!1~#2!1B#1!6~#2!1B#1!1~$"
        "#1!10~$"                             # More yellow
        "#1!3~#2!4B#1!3~$"                    # Mouth
        "#1!2~#2!6B#1!2~$"
        "#1!1~#2!8B#1!1~$"
        "#2!10B$"                             # Bottom edge
        "\x1b\\"                             # ST (String Terminator)
    )
    sys.stdout.write(smiley)
    sys.stdout.flush()

if __name__ == "__main__":
    print_sixel()
    # Give the terminal a brief moment to render before exit (optional)
    time.sleep(0.1)
