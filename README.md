# PSVshell
Yet another overclocking plugin

## Features:
- Allows you to change CPU, GPU, BUS and XBAR clocks separately, in these steps:
  - **CPU:** 41, 83, 111, 166, 222, 333, 444, 500 MHz
  - **GPU (ES4):** 41, 55, 83, 111, 166, 222 MHz
  - **BUS:** 55, 83, 111, 166, 222 MHz
  - **XBAR:** 83, 111, 166 MHz
- Supports per-app profiles
- Shows per-core CPU usage in %, including peak single-thread load
- Runs in kernelland (=> visible in LiveArea)
- Pretty GUI with some useless eye-candy metrics such as ram/vram usage, battery temp, etc...
- Does not slow down games when menu is open
- Does not crash Adrenaline
- Clean code and patches
- **3.60** and **3.65** FWs are supported

## How to use:
- Press **SELECT + UP** or **SELECT + DOWN** to toggle between 3 GUI modes

#### When in 'FULL' mode:
- Use **UP/DOWN** to move in the menu
- Press **X** to toggle frequency mode for currently selected **> device <**:
  - **Default freq.** (WHITE) - the plugin will not interfere, but rather use the default freq. for current game
  - **Manual freq.** (BLUE) - the plugin will use your specified freq.
    - press **LEFT/RIGHT** to immediately change the frequency
- Press **X** when **> save profile <** is selected to save/delete profiles
  - All **Manual freq.** (BLUE) will be loaded and applied next time you start/resume the game
  - All **Default freq.** (WHITE) will be kept to default (set to whatever freq. the game asks for)
- Press and hold **LEFT TRIGGER** and **> save profile <** will change to **> save global <**
  - Press **X** when **> save global <** is selected and the options will be saved to *global* (default) profile
  - *Global* profile will be used as default profile when game-specific profile doesn't exist

## Screenshots:
![2019-12-21-181613](https://user-images.githubusercontent.com/12598379/71311342-c15df300-241e-11ea-8baf-c67ec2bcbbd7.png)

### 'FPS only' mode
![2019-11-01-194930](https://user-images.githubusercontent.com/12598379/68051962-e10f4f80-fce8-11e9-92d0-9662cc6f0d04.png)

### 'HUD' mode
![2019-12-21-181809](https://user-images.githubusercontent.com/12598379/71311344-c1f68980-241e-11ea-9ca1-4207d4887002.png)

### 'FULL' mode
![2019-12-21-181801](https://user-images.githubusercontent.com/12598379/71311343-c1f68980-241e-11ea-8249-5f2e0c44d642.png)

## Credits:
- [Yifan Lu](https://github.com/yifanlu) - for [ScePervasive](https://wiki.henkaku.xyz/vita/Pervasive) RE
- [dots-tb](https://github.com/dots-tb) - for ksceKernelInvokeProcEventHandler() hook
- [Rinnegatamante](https://github.com/Rinnegatamante) - for orig. framecounter impl.
