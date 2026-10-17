# The Seventh Night (C++ / OpenGL 3.3, no engine)

Milestone 1: window, forest models from .glb files, 2.5D side camera with three depth lanes,
lantern light, fog, and a test "Will" control. This project has not been compiled yet, so expect a
small fix or two on the first build.

## One-time setup (Windows)

1. Install **Visual Studio 2022 Build Tools** (workload: "Desktop development with C++"), **CMake**, and **Git**.
2. Install vcpkg:
   ```
   git clone https://github.com/microsoft/vcpkg C:\vcpkg
   C:\vcpkg\bootstrap-vcpkg.bat
   setx VCPKG_ROOT C:\vcpkg
   ```
   Close and reopen the terminal afterwards.

## Build and run

```
cmake -B build -S . -DCMAKE_TOOLCHAIN_FILE=%VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
cmake --build build --config Release
build\Release\SeventhNight.exe assets\Tree_1.glb assets\Rock_1.glb
```

The first configure downloads and builds the libraries (a few minutes).

## Assets

Make an `assets` folder and copy the **glTF/GLB** files from your Quaternius packs into it
(use the Standard zips; every pack includes glTF). File names differ per pack, so check the folders.
Also put the **animation GLBs** in `assets/animations/`: from Universal Animation Library 1 and 2, pick the
**Root Motion disabled** versions. All packs are CC0, so you can use them freely.

## Controls

A/D move, Space jump, W/S change depth lane, Q/E lower or raise Will (lantern brightness), Esc quit.

See PLAN.md for the milestone list to give to the Antigravity agent.
