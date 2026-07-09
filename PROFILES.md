# 3DGB PC Profile Editor

The PC build includes a profile editor for creating and testing `.meta` files.

## Keys

- `F1`: toggle the profile editor
- `F2`: pause or resume emulation
- `F3`: open the command bar
- `F5`: save the current profile
- `F6`: export a package
- `F7`: preview `imports/profile_import.json`
- `F8`: apply the previewed import
- `F9`: load the current profile file
- `F10`: switch editor page
- `Ctrl+Z`: undo the last profile edit
- `Ctrl+Y`: redo the last undone edit

Click a tile in the left VRAM grid to edit it. `WASD` still moves the current tile. Tiles used by the current frame have a thin green outline. Hover UI controls for short tooltips.

Selection controls:

- Click: select one tile
- Drag: rectangle selection
- Ctrl+click: toggle one tile
- Ctrl+drag: add a rectangle to the current selection
- Shift+click: select a range from the anchor tile
- `M`: toggle the current tile
- `Esc`: reset selection to the current tile

Color values can be changed by dragging the sliders or by clicking the numeric value next to a slider and typing a value from `0` to `255`.

Unsaved profile edits are shown with a `*` next to the file name. Loading a profile with unsaved changes requires clicking `Load` twice.

## Command bar

The original command-line style interface is still available.

- Use `WASD` to select a tile in the VRAM inspector
- Press `F3` to open the command bar
- Type a command and press `Enter` to execute it
- Supported commands:
    - `set_meta bg_color [r] [g] [b]`
    - `set_meta win_color [r] [g] [b]`
    - `set_meta obj_color [r] [g] [b]`
    - `set_meta all_z [z_layer]`
    - `set_meta bg_for_z [z_layer]`
    - `set_meta bg_back_z [z_layer]`
    - `set_meta win_z [z_layer]`
    - `set_meta obj_z [z_layer]`
    - `set_meta obj_behind_z [z_layer]`
    - `save_meta [meta_filename.meta]`
    - `load_meta [meta_filename.meta]`

## Pause mode

Pause stops CPU/PPU progress but keeps the editor active. Color and Z-depth edits are redrawn against the current VRAM/OAM snapshot.

## Export package

`Export package` creates a timestamped directory under `exports/`.

Main files:

- `window_snapshot.png`: full application window at export time
- `frame_full.png`: emulator frame only
- `tile_grid.png`: current VRAM tile grid
- `manifest.json`: package metadata
- `profile_current.json` and `profile_current.meta`: current profile snapshot
- `all_tiles/`: data and images for every VRAM tile

The `all_tiles/` folder contains:

- `all_tiles.png`
- `all_tiles_labeled.png`
- `tiles.csv`
- `tiles.json`
- one `tile_XXX_hash_YYYY.png` file per tile

## Import format

The importer accepts either a raw array of objects or an object with an `entries` array. Each entry can use `tile_hash` or `hash`.

Canonical color fields use arrays:

```json
{
  "entries": [
    {
      "tile_hash": 3714332214,
      "bg_color": [18, 24, 18, 255],
      "win_color": [236, 244, 210, 255],
      "obj_color": [255, 255, 255, 255],
      "bg_for_z": 1,
      "bg_back_z": 0,
      "win_z": 2,
      "obj_z": 3,
      "obj_behind_z": 1,
      "flags": 0
    }
  ]
}
```

Compact RGB string fields are also accepted:

```json
[
  {
    "hash": 3714332214,
    "bg_color_rgb": "18,24,18",
    "win_color_rgb": "236,244,210",
    "obj_color_rgb": "255,255,255",
    "bg_for_z": 1,
    "bg_back_z": 0,
    "win_z": 2,
    "obj_z": 3,
    "obj_behind_z": 1
  }
]
```

Only fields present in an entry are applied.

Once a profile is created, the `convert_meta.py` script can be used to serialize it and edit it as JSON:

- `python convert_meta.py bin2json < meta/[metafile] > meta/[output].json`
- `python convert_meta.py json2bin < meta/[json_file] > meta/[output].meta`
