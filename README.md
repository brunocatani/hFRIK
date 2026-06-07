# Fallout 4 VR Body - FRIK Experimental

Experimental hFRIK build of Fallout 4 VR Body used by the current ROCK release.

This release exists to provide the FRIK body/skeleton provider changes that ROCK v0.5 currently depends on. The dependency is temporary and should go away once these modifications are merged upstream into `github.com/rollingrock/Fallout-4-VR-Body`.

## ROCK Compatibility

Install **FRIK Experimental** before installing or using ROCK v0.5.

The plugin still installs as `FRIK.dll` and uses the normal FRIK data/config layout. The experimental name is for the release package and user-facing dependency note.

## Nexus

- [Main](https://www.nexusmods.com/fallout4/mods/53464/)
- [Support](https://www.nexusmods.com/fallout4/mods/53464?tab=posts)

## Wiki

- [Main](https://github.com/rollingrock/Fallout-4-VR-Body/wiki)
- [Changelog](https://github.com/rollingrock/Fallout-4-VR-Body/wiki/Changelog)
- [In-Game Configuration Guide](<https://github.com/rollingrock/Fallout-4-VR-Body/wiki/In%E2%80%90Game-Configuration-Guide-(v69)>)
- [FAQ](https://github.com/rollingrock/Fallout-4-VR-Body/wiki/FAQ)

## Development

- [Development](https://github.com/rollingrock/Fallout-4-VR-Body/wiki/Development)
- [Features Backlog](https://github.com/rollingrock/Fallout-4-VR-Body/wiki/Features-Backlog)

## Build

```powershell
cmake --preset custom-fast
cmake --build build-fast --config Release --target FRIK -- /m
```

Fast output: `build-fast/Release/FRIK.dll` and `build-fast/Release/FRIK.pdb`.
The local `custom-fast` preset deploys both files to `D:/FO4/mods/FRIK 80/F4SE/Plugins`.

Release package builds stay explicit:

```powershell
cmake --preset custom-release
cmake --build build-release --config Release --target FRIK -- /m
```

## Test

```powershell
cmake --preset custom-tests
cmake --build build-tests --config Release --target FRIKPolicyTestBinaries -- /m
ctest --test-dir build-tests -C Release --output-on-failure -j %NUMBER_OF_PROCESSORS%
```

## Credits

- [Credits](https://github.com/rollingrock/Fallout-4-VR-Body/wiki/Credits)
