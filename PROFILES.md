> Note: the profile creation interface is still very rough and unfinished.

___


To create a profile and add metadata to different tiles, the project currently uses a simple command-line style interface.

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

___

Once a profile is created, the `convert_meta.py` script can be used to serialize it and edit it as JSON:

* `python convert_meta.py bin2json < meta/[metafile] > meta/[output].json`
* `python convert_meta.py json2bin < meta/[json_file] > meta/[output].meta`