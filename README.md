# BSP sekai
BSP sekai (BSP world) is a level converter for Quake 3's BSP format and games based on it. It is based on the BSP loaders in the [Spearmint engine](http://spearmint.pw).

This is alpha software, there are many issues of things not properly change or removed when converting to other BSP formats.
The only supported conversion is NSCO maps (Q3 with modified surface/content bits) to Enemy Territory BSP format.

## Usage
```
bspsekai <conversion> <input-BSP> <format> <output-BSP>
BSP sekai - v0.1
Convert a BSP for use on a different engine
BSP conversion can lose data, keep the original BSP!

<conversion> specifies how surface and content flags are remapped.
Conversion list:
  none      - No conversion.
  nsco2et   - Convert Navy SEALS: Covert Operation surface/content flags to ET values.
  et2nsco   - Convert ET surface/content flags to Navy SEALS: Covert Operation values.

The format of <input-BSP> is automatically determined from the file.
Input BSP formats: (not all are fully supported)
  Quake 3, RTCW, ET, EF, EF2, FAKK, Alice, Dark Salvation, MOHAA, Q3Test 1.06 or later, SoF2, JK2, JA

<format> is used to determine output BSP format.
BSP format list:
  quake3    - Quake 3.
  rtcw      - Return to Castle Wolfenstein.
  et        - Wolfenstein: Enemy Territory.
  darks     - Dark Salvation.
```

## BSP Formats
Quake 3 BSP format is also used by Elite Force, Tremulous, Smokin' Guns, World of Padman, Turtle Arena, and other games.
Soldier of Fortune 2 BSP format is also used by Jedi Knight 2: Jedi Outcast and Jedi Knight: Jedi Academy.

### Write formats
Currently, only Q3 BSP format and games that changed the version number are supported.

Game | BSP ident & version
---- | ----
Quake III Arena              | IBSP 46
Return to Castle Wolfenstein | IBSP 47
Wolfenstein: Enemy Territory | IBSP 47
Dark Salvation               | IBSP 666

### Read formats
Game | BSP ident & version | notes
---- | ---- | ----
Q3Test 1.06/1.07/1.08        | IBSP 45
Quake III Arena              | IBSP 46
Return to Castle Wolfenstein | IBSP 47  | different version than Q3
Wolfenstein: Enemy Territory | IBSP 47  | adds foliage surface type
QuakeLive                    | IBSP 47  | advertisements lump is ignored
Dark Salvation               | IBSP 666 | different version than Q3
Soldier of Fortune 2         | RBSP 1   | missing light styles support
Heavy Metal: F.A.K.K.2       | FAKK 12  | major differences from Q3, not fully supported
American McGee's Alice       | FAKK 42  | different version than FAKK, not fully supported
Medal of Honor Allied Assult | 2015 19  | major differences from FAKK, not fully supported
Elite Force 2                | EF2! 20  | major differences from FAKK, not fully supported

## License
BSP sekai is licensed under a [modified version of the GNU GPLv3](https://github.com/zturtleman/bsp-sekai/blob/master/COPYING.txt#L625) (or at your option, any later version). The license is also used by Spearmint, Return to Castle Wolfenstein, Wolfenstein: Enemy Territory, and Doom 3.

