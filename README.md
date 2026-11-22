# hyprmosaic

The Mosaic layout separated from [Hyprdrive](https://github.com/dragosol/Hyprdrive).

> [!WARNING]
> This plugin uses only Hyprdrive's layout arrangement. Behavior may differ from the original.

## Features

- Infinite canvas
- Focus mode: keeps the active window centered

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
            -- Initial focus-mode state for each workspace: 0 = off, 1 = on
            focus_mode = 1,
        },
    },
})

-- Toggle focus mode for the current workspace
hl.bind("SUPER + C", hl.dsp.layout("togglefocus"))
```

## Credits

- [Hyprdrive](https://github.com/dragosol/Hyprdrive) for the original Mosaic window arrangement
