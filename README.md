<p align="center">
    <a href="https://github.com/xenia-canary/xenia-canary/tree/canary_experimental/assets/icon">
        <img height="256px" src="https://raw.githubusercontent.com/xenia-canary/xenia/master/assets/icon/256.png" />
    </a>
</p>

# 🚧 Xenia Canary - Kinect Implementation (WIP) 🚧

**Notice:** This is an experimental, work-in-progress fork of [xenia-canary](https://github.com/xenia-canary/xenia-canary) dedicated specifically to reverse-engineering and implementing **Xbox 360 Kinect (NUI) support** for emulator-related research.

> [!WARNING]
> **AI-Assisted Development Notice:** This repository features significant reverse-engineering, analysis, and coding contributions created with generative AI assistance via **Antigravity (Gemini)** and **Claude**. AI models were utilized to analyze decompiled PowerPC guest code, stub/implement complex XAM/Kernel Kinect message handlers, map USB/SDK data structures, and resolve filesystem path mapping issues.

> [!NOTE]
> This fork is built primarily for fun and exploration. It is **not** meant to be a production-grade, serious implementation, nor an official Kinect implementation just yet. The primary goal is to see how far we can make it work, experiment with getting Kinect games running, and jumpstart a solid foundation for future developers to build a proper implementation upon.

Come chat with us about **emulator-related topics** on [Discord](https://discord.gg/Q9mxZf9). Please check the [FAQ](https://github.com/xenia-canary/xenia-canary/wiki/FAQ) page before asking questions.

---

## Current Status

We are working toward full Kinect hardware-level emulation (HLE). Our testing focuses primarily on `nuitest` (NUIView.exe) and *Fruit Ninja Kinect* (title ID 58410B79).

### ✅ What Works
- **Dynamic SDK & Driver Loading**: Dynamic loading of Windows `Kinect10.dll` SDK (for real Kinect v1 hardware) as well as OpenNI2/NiTE2 and libfreenect drivers.
- **Poll Thread & Skeleton Telemetry**: An active background thread mapping joint angles and positions (including 15-joint OpenNI2 to 20-joint Xbox Layout) or generating a synthetic T-pose skeleton for testing.
- **NUI Kernel/XAM APIs**: 50+ `XamNui*` function stubs and kernel `PsCamDeviceRequest`/`McaDeviceRequest` device interfaces returning successful states.
- **COM Message Dispatch**: Message handlers implemented for NUI session management and request/response frames (`0x2B001`-`0x2B005`, `0x2C009`, `0x2C00C`, `0x2C00D`, `0x58004`, `0x58035`).
- **Filesystem Resolution**: Fixed symbolic link VFS relative path resolution fallbacks allowing games to load Kinect config files (e.g., `itemList.fnk`, etc.).

### 🚧 What's Under Active Development / Troubleshooting
- **Black Screen / Frame Loop Troubleshooting**: Resolving worker/JIT thread execution events to prevent the game engine from blocking during JIT compiling or background wait cycles.
- **Render Viewport Integration**: Mapping video/depth streams to display buffers correctly.
- **Windows SDK Elevation Hooks**: Aligning actual Windows SDK camera tilt functions (`NuiCameraElevationSetAngle` / `NuiCameraElevationGetAngle`) with the game's expected internal exports.

---

## Status

Buildbot | Status | Releases
-------- | ------ | --------
Canary (🪟, 🐧) | [![CI](https://github.com/xenia-canary/xenia-canary/actions/workflows/Orchestrator.yml/badge.svg?branch=canary_experimental)](https://github.com/xenia-canary/xenia-canary/actions/workflows/Orchestrator.yml/badge.svg?branch=canary_experimental) [![Codacy Badge](https://app.codacy.com/project/badge/Grade/cd506034fd8148309a45034925648499)](https://app.codacy.com/gh/xenia-canary/xenia-canary/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade) | [Latest](https://github.com/xenia-canary/xenia-canary/releases/latest) ◦ [All](https://github.com/xenia-canary/xenia-canary/releases) ◦ [Old](https://github.com/xenia-canary/xenia-canary-releases/releases)

### Experimental Netplay

Buildbot | Status | Releases
-------- | ------ | --------
Windows | [![Codacy Badge](https://app.codacy.com/project/badge/Grade/d814c4b6aa444dcc9c1631e0224b2739)](https://app.codacy.com/gh/AdrianCassar/xenia-canary/dashboard?utm_source=gh&utm_medium=referral&utm_content=&utm_campaign=Badge_grade) | [Latest](https://github.com/AdrianCassar/xenia-canary/releases/latest)

## Quickstart

See the [Quickstart](https://github.com/xenia-canary/xenia-canary/wiki/Quickstart) page.

## FAQ

See the [frequently asked questions](https://github.com/xenia-canary/xenia-canary/wiki/FAQ) page.

## Building

See [building.md](docs/building.md) for setup and information about the
`xb` script. When writing code, check the [style guide](docs/style_guide.md)
and be sure to run clang-format!

## Contributors Wanted!

Have some spare time, know advanced C++, and want to write an emulator or work with depth sensors?
Contribute! There's a ton of work that needs to be done, a lot of which
is wide open greenfield fun.

**For general rules and guidelines please see [CONTRIBUTING.md](.github/CONTRIBUTING.md).**

Fixes and optimizations are always welcome (please!), but in addition to
that there are some major work areas still untouched:

* Help work through [missing functionality/bugs in games](https://github.com/xenia-canary/xenia-canary/labels/compat)
* Reduce the size of Xenia's [huge log files](https://github.com/xenia-canary/xenia-canary/issues/1526)
* Skilled with Linux? A strong contributor is needed to [help with porting](https://github.com/xenia-canary/xenia-canary/labels/platform-linux)

See more projects [good for contributors](https://github.com/xenia-canary/xenia-canary/labels/good%20first%20issue). It's a good idea to ask on Discord and check the issues page before beginning work on
something.

## Disclaimer

The goal of this project is to experiment, research, and educate on the topic
of emulation of modern devices and operating systems. **It is not for enabling
illegal activity**. All information is obtained via reverse engineering of
legally purchased devices and games and information made public on the internet.
