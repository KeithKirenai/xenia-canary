# Xenia Kinect Development Guidelines

## Config and Flag Management
- **Rule**: Xenia Canary saves/overwrites its configuration file (`xenia-canary.config.toml`) in the executable directory upon clean exit.
- **Action**: When modifying default configurations for development builds (e.g., enabling NUI initialization by default):
  1. Modify the `DEFINE_bool` or `DEFINE_string` default values directly in the source code.
  2. Proactively delete or regenerate existing `.config.toml` files in the build output folder to ensure updated defaults are written.

## Kinect (NUI) HLE Emulation Dependencies
- **Rule**: Guest NUI initialization is split between user-mode XAM stubs (`xam_nui.cc`) and kernel-mode device requests (`xboxkrnl_ldi.cc` / `xboxkrnl_io.cc`).
- **Action**: If a guest game freezes during startup or path resolution:
  - Check the log for `NullDevice::ResolvePath` or dismount/mount calls (`IoDismountVolume`, `IoDismountVolumeByFileHandle`).
  - Verify if low-level device control calls (`PsCamDeviceRequest`, `McaDeviceRequest`, `DetroitDeviceRequest`) are being queried with custom request codes.
