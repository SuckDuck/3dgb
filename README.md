# 3DGB

A proof-of-concept Game Boy DMG emulator that renders games in 3D,  
inspired by [3DSEN](https://geodstudio.net/).

The core emulator is [peanut_gb](https://github.com/deltabeard/Peanut-GB).  
This project implements an additional layer over the render pipeline that allows  
applying preconfigured metadata associated with individual tiles, such as color  
information and depth levels.

The project is experimental and incomplete, with the long-term idea  
of targeting Nintendo 3DS and MetaQuest 3.

![demo_gif](media/demo.gif)

## Build

The only dependency required is [raylib](https://www.raylib.com/).  
To build the project, simply run `make`.

## Usage

* Run `./3dgb [rom_path]`
* Use `WASD` to select a tile in the VRAM inspector
* Press `Space` to open the command bar
* Type a command and press `Enter` to execute it
* Supported commands:
    * `set_meta bg_color [r] [g] [b]`
    * `set_meta win_color [r] [g] [b]`
    * `set_meta obj_color [r] [g] [b]`
    * `set_meta all_z [z_layer]`
    * `set_meta bg_for_z [z_layer]`
    * `set_meta bg_back_z [z_layer]`
    * `set_meta win_z [z_layer]`
    * `set_meta obj_z [z_layer]`
    * `set_meta obj_behind_z [z_layer]`
    * `save_meta [meta_filename.meta]`
    * `load_meta [meta_filename.meta]`
* The `convert_meta.py` script can also be used to edit metadata profiles:
    * `python convert_meta.py bin2json < meta/[metafile] > meta/[output].json`
    * `python convert_meta.py json2bin < meta/[json_file] > meta/[output].meta`
* Controls:
    * D-Pad: arrow keys
    * Start: `P`
    * A: `Z`
    * B: `X`

## Notes

Right now, creating profiles is a pain in the ass, however, once on 3DS I will
try to make it far better.