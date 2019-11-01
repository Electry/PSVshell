# PSVshell
Yet another overclocking plugin

### Features:
- Allows you to change CPU, GPU, BUS and XBAR clocks separately, in these steps:
  - **CPU:** 41, 83, 111, 166, 222, 333, 444, 500 MHz
  - **GPU:** 41, 55, 83, 111, 166, 222 MHz
  - **BUS:** 55, 83, 111, 166, 222 MHz
  - **XBAR:** 83, 111, 166 MHz
- Supports per-app profiles
- Shows per-core CPU usage in %, including peak single-thread load
- Runs in kernelland (=> visible in LiveArea)
- Pretty GUI with some useless eye-candy metrics such as ram/vram usage, battery temp, etc...
- Does not slow down games when menu is open
- Does not crash Adrenaline
- Clean code and patches
- **3.60** and **3.65** fws are supported, **any other will bootloop!** (hold LT on boot if that happens -- you shouldn't be using other fws anyway)

### How to use:
- Press SELECT + UP/DOWN to toggle between 3 GUI modes
- When in 'FULL' mode:
  - Use UP/DOWN to move in menu
  - Press X to toggle frequency mode -- default freq. (WHITE) / manual freq. (BLUE)
  - If manual frequency mode is selected (freq. shown in BLUE), press LEFT/RIGHT to change frequency
  - Press X when 'save profile' is selected to save/delete profile

### Screenshots:
![2019-11-01-193451](https://user-images.githubusercontent.com/12598379/68050015-9a1f5b00-fce4-11e9-80dc-e358e7527bac.png)

## 'FPS only' mode
![2019-11-01-194930](https://user-images.githubusercontent.com/12598379/68051962-e10f4f80-fce8-11e9-92d0-9662cc6f0d04.png)

## 'HUD' mode
![2019-11-01-194921](https://user-images.githubusercontent.com/12598379/68051994-f5534c80-fce8-11e9-9f20-ee2ced7a5cf9.png)

## 'FULL' mode
![2019-11-01-194903](https://user-images.githubusercontent.com/12598379/68052016-03a16880-fce9-11e9-8e82-218749d06694.png)
