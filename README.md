# hyprmosaic

The Mosaic layout separated from [Hyprdrive](https://github.com/dragosol/Hyprdrive).

> [!WARNING]
> This plugin uses only Hyprdrive's layout arrangement. Behavior may differ from the original.

## Features

- Infinite canvas
- Focus mode: keeps the active window centered
- Panning canvas by mouse and keyboard

## Install

```sh
hyprpm add https://github.com/WingsZeng/hyprmosaic
hyprpm enable hyprmosaic
hyprpm reload
```

## Config

```lua
hl.config({
    general = {
        layout = "mosaic",
    },
    plugin = {
        hyprmosaic = {
            -- Default window width: <= 1 = fraction of workspace width; > 1 = pixels
            default_width = 0.6,
            -- Default window height: <= 1 = fraction of workspace height; > 1 = pixels
            default_height = 0.6,
            -- Default focus mode;
            focus_mode = false,
        },
    },
})

-- Toggle focus mode
hl.bind("SUPER + C", hl.dsp.layout("togglefocus"))

-- Pan canvas by mouse;
hl.bind("SUPER + mouse:274", hl.dsp.layout("panmouse"), { mouse = true })

-- Pan canvas by keyboard;
hl.bind("SUPER + CTRL + LEFT", hl.dsp.layout("pan 200 0"), { repeating = true })
hl.bind("SUPER + CTRL + RIGHT", hl.dsp.layout("pan -200 0"), { repeating = true })
hl.bind("SUPER + CTRL + UP", hl.dsp.layout("pan 0 200"), { repeating = true })
hl.bind("SUPER + CTRL + DOWN", hl.dsp.layout("pan 0 -200"), { repeating = true })
```

## Credits

- [Hyprdrive](https://github.com/dragosol/Hyprdrive) for the original Mosaic window arrangement
