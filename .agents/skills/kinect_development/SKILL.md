---
name: kinect_development
description: Guides building, running, and debugging Kinect (NUI) features in Xenia Canary.
---

# Kinect Development Cheatsheet

## Building Xenia Canary with Kinect Support
To compile the emulator on Windows with MSVC:
```powershell
$env:VULKAN_SDK = "C:\VulkanSDK\1.4.350.0"
.\xb.ps1 build --target xenia-app
```

## Running the NUI Diagnostic Test
Run the NUI diagnostic tool (`nuitest`) directly from the build outputs:
```powershell
# Remove old configuration to ensure new defaults apply
Remove-Item C:\Users\Carlos\xenia_kinect\xenia-canary\build\bin\Windows\Debug\xenia-canary.config.toml -Force

# Launch the emulator pointing to the nuitest package
& "C:\Users\Carlos\xenia_kinect\xenia-canary\build\bin\Windows\Debug\xenia_canary.exe" "C:\Users\Carlos\xenia_kinect\misc_files\nuitest"
```
