# ASWIni CMake Example

Configure and build from the repository root:

```text
cmake -S example/cmake -B example/cmake/build
cmake --build example/cmake/build --config Release
```

The executable is written to `example/cmake/build/bin` (in a `Release` sub folder for multi-config
generators such as Visual Studio). It builds at C++17, the library's minimum; add
`-DASWINI_CXX_STANDARD=20` (in another build folder) to build it at C++20.

The example writes `ASWIniExample.ini` in the `ASWIniExample` folder of the temp folder, starting
afresh on each run, and prints each step, the messages the library reports and the file. It returns
0 when every step did what it shows. Run it with `--pause` to wait for Enter before it exits.
