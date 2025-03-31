# Important
This is still WIP! Not guaranteed to work yet.

## About

This consists in a simple template for a UCI chess engine that includes 
[Disservin's chess library](https://github.com/Disservin/chess-library)
and my [UCI protocol library (libuci)](https://github.com/mergener/libuci), alongside 
example source files that initialize a simple UCI-compliant randomly-moving engine.

The goal of this template is to allow quick bootstrapping of a chess engine, without
having worry about implementing the UCI protocol and a chess library.

## Requirements

- C++17 compliant compiler
- CMake version 3.10 or higher (lower versions might work but haven't been tested)

## Usage

Start by cloning the repository. You can either fork it or simply clone it, delete the .git
folder and then create a Git repository on your own.

The project working directory is structured as follows:

```sh
/ext                    -- External dependencies
    /chess
        ...
    /libuci
        ...
    CMakeLists.txt
/src                    -- Your engine code goes here
    engine.cpp          -- UCI handlers
    search.cpp          -- Basic search function
    main.cpp            -- Program entry point
    engine.h
    search.h
    CMakeLists.txt
CMakeLists.txt
...
```

The template code already provides handlers for most UCI commands, including `uci`, `isready`, `position`, `go` and `stop`.
`search.cpp` contains a basic search function that randomly selects a legal move from the current position and
takes time into consideration.

Several TODOs are scattered throughout the code, suggesting where you can add your own logic.

## License

All code in this repository -- except for code under the `ext/` directory and its subdirectories -- is public domain as specified in `unlicense.md`.

Each library under `ext/` have their respective license files inside them.

