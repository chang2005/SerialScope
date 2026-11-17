# Asset generator

`make_assets.cpp` is a one-off development tool used to render the SerialScope
icon set and the theme-aware GitHub marks. It is not part of the application
build and is not required to compile or run SerialScope.

Requirements: Qt 6 Core, Gui and Svg development files, plus the official
`mark-github-24.svg` from [Primer Octicons](https://github.com/primer/octicons).

Example (MinGW, from the repository root):

```bat
g++ -std=c++17 -O2 -DQT_NO_DEBUG ^
  -ID:/Qt/6.8.3/mingw_64/include ^
  -ID:/Qt/6.8.3/mingw_64/include/QtCore ^
  -ID:/Qt/6.8.3/mingw_64/include/QtGui ^
  -ID:/Qt/6.8.3/mingw_64/include/QtSvg ^
  tools/make_assets.cpp ^
  -LD:/Qt/6.8.3/mingw_64/lib -lQt6Svg -lQt6Gui -lQt6Core ^
  -o build/make_assets.exe

set PATH=D:\Qt\6.8.3\mingw_64\bin;%PATH%
build\make_assets.exe resources\icons mark-github-24.svg
```

After rendering the PNG sizes, package them into a Windows ICO. The ICO
directory stores each PNG frame, with 256 px encoded as width/height `0`.
