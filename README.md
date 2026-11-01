# Analog Kid Digital Man FM1 Open V1

English · [简体中文](README.zh-CN.md)

8 tracks, 8 sample slots, 16 FX demos; binary SDK included.

[Workbench and development kit](https://open.m-vave.com/fm1-workbench) · [Official forum](https://forum.m-vave.com)

Use the workbench to update firmware, preview the instrument and manage projects, samples and DX7 voices. Select "Source + SDK" to download the development kit matching the online release. Back up projects, voices and samples before updating, and keep the device powered during writes.

## V14 features

- **DX7 library:** 128 voices in four groups of 32, with the M-VAVE FM1 original V15 factory voices and order. Saved user banks take priority. DX7 single-voice and 32-voice bank SysEx import/export is supported; extension parameters are not handled. Changing the library does not change patches already saved in projects.
- **Track roles:** on SEQ **8/8 TRACK ROLE**, K1 selects NORMAL, PLAY or OFF; K2 selects input MIDI channel 1–16. PLAY retains the track's timbre and held notes across project changes. MUTE/SOLO use the existing controls, with independent NORMAL and PLAY solo groups. PLAY timbre edits are not saved with projects: export a timbre file, or save DX7 voices to the independent library.
- **Step recording:** with the track set to NORMAL, press REC while stopped and play notes. Releasing all played keys advances to the next step, including DRUM 808 and SAMPLER. Overlapping notes share a step; during playback, REC records in time. PLAY tracks are for live performance and do not record notes.
- **Tempo and delay:** internal tempo is 30–300 BPM. Tap Tempo is available on the workbench, with SEQ on the FX DELAY page, or with HOME held while tapping SEQ elsewhere. Delay supports tempo synchronization and fixed times of about 30–2040 ms.

## Classic and progressive rock bank (this fork)

The 16 built-in FM tones, the four acid-engine recipes and the 16 demo projects in [app/presets.c](app/presets.c) are revoiced for classic and progressive rock.

- **FM tones**, named in the shorthand players use for the instruments they imitate: B3 ORGAN (rotary tonewheel), COMBO FARF, PIPE ORGAN, WURLI 200A, DX7 EP, HARPSICHRD, CLAV D6, MINI LEAD, P5 SYNC, PEDAL BASS, MODEL D BS, SOLINA STR (string machine), M400 FLUTE and M400 CHOIR (tape replay), OB BRASS, J60 PAD (chorus pad), JP8 POLY. All are FM approximations on the DX7 engine; the names describe the target sound and imply no affiliation.
- **Acid engine:** warm bass, deep pedal, singing lead and square bass.
- **Demo projects:** ARENA ROCK, GALLOP PROG, CATHEDRAL, RONDO 7/8, SYMPHO POP, LUNAR DRIFT, ASTRO ORGAN, WURLI BOUNCE, BAROQUE ROCK, TAPE CHOIR, ODYSSEY 5/4, DUEL 9/8, FUSION 11/8, PEDAL 7/4, SEQUENCER 78 (late-70s French electronic: sixteenth sequencer, string machine, rhythm box) and COLD MACHINE (late-70s new wave: driving bass, polysynth pad, steady beat). New rhythms include straight rock, half-time, shuffle, gallop, 3+3+2 accents and stomp.

- **Mixed meters:** the sequencer loops over 16, 32, 48 or 64 steps, so odd bars are packed into one 64-step loop with a short filler bar: 7/8 ×4 + 4/8, 5/4 ×2 + 6/4, 7/4 ×2 + 2/4, 9/8 ×3 + 5/8 and 11/8 ×2 + 10/8. Drums, bass and chords follow each bar's 2+3 beat groups. Add a layout to `meters` in `app/presets.c` and set it as a scene's last field.

These are style sketches, not transcriptions of any recording. The 128-voice DX7 library is unchanged.

## Panel themes (this fork)

Seven screen themes borrow the colours of classic instruments: **FM1** (original), **JOVE 8** (black panel with red, orange and yellow button rows), **JUNE 60** (slate, cream, red and mustard), **WALNUT D** (black, orange and blue rockers, walnut cheeks), **OBELISK** (black with silver and blue trim), **ORACLE 5** (black, cream, red and gold, oak cheeks) and **SIX OPS** (brown panel, green and maroon membrane buttons, green display). The names nod to the instruments without using their trademarks.

Hold **HOME** and press **GLO** to open the Bluetooth / clock page (also under ACTIONS as BT / CLOCK / THEME), then turn **SEL** to change the theme. It is a device preference: it applies immediately, saves after 1.5 s and survives power cycles. It is stored in an optional tail of the session/tuning sector, so existing tuning and track-role data keep their offsets; firmware without theme support ignores it.

## MIDI

FM1 accepts DIN, USB and BLE MIDI input, with USB/BLE output. Default input channels 1–7 address tracks 1–7; channel 10 addresses track 8. Change each input channel on TRACK ROLE.

- **5/8 SYNC / LOOP:** select internal or external clock, tempo, loop length and clock input.
- **6/8 MIDI TRACK:** select SEQ/ARP LOCAL, BOTH or MIDI output, output channel, output ports and physical KEY OUT.
- **7/8 MIDI ROUTING:** configure LOCAL, CLK OUT and MIDI Thru. For DAW monitoring with Local Off, enable KEY OUT and select the output ports.

MIDI preferences are saved independently of projects. The browser simulator previews these settings without sending performance MIDI to external ports; configure the instrument on its panel. MIDI Clock, Start, Continue, Stop and Song Position Pointer input are supported. Clock loss stops synchronized playback.

## Updates and compatibility

V13 can update to V14. The workbench supports returning to original V15 and migrating from compatible third-party firmware. The standard FM1 update protocol remains available. Compatibility has been checked with selected firmware versions; it is not a guarantee for every third-party release.

Firmware switching does not make project and sample formats interchangeable. Keep exported backups for rollback. Older firmware may not support V14 track preferences, the independent voice library or synchronized/extended delay settings.

To back up the independent voice library, read and export groups A–D separately. Project files do not contain sample audio. Restore custom samples from their sample bank backup; writing the built-in sample bank replaces the current samples.

The eight sample slots share about 19.55 seconds of new sample storage. Existing larger banks remain readable; they may need to be backed up and reduced before writing the independent DX7 library.

## Build

Requires Windows x64, PowerShell, `curl.exe` and `tar.exe`. Firmware builds require **JieLi Windows toolchain 2.5.3**, which includes PI32v2 clang 4.0.1. [Download the 2.5.3 installer from JieLi](https://jl-update.oss-cn-shenzhen.aliyuncs.com/2.5.3.exe) and follow the [installation guide](https://doc.zh-jieli.com/Tools/zh-cn/dev_tools/dev_env/jl_toolchain.html). Set `FM1_PI32_TOOLCHAIN` to its `pi32` folder containing `bin/clang.exe` (normally `C:\JL\pi32`). FM1 continues to use the `r3` CPU target.

The build checks all 22 toolchain files against `sdk/manifest.json`. Older V14 kits were pinned to 2.5.2 and reject 2.5.3; update the complete source + SDK kit together, keeping the manifest checks enabled. For this revision, the expected SHA-256 of `pi32/bin/clang.exe` is `d4bc97cf421c1ccc373dd8924ce88720ff7457b3f1478d15df4aeda6ae923bce`.

For a local web preview:

~~~powershell
.\tools\Build-Web.ps1 -LocalPreview
~~~

For distribution builds, first set `FM1_SOURCE_URL` to the HTTPS URL of the matching source archive, including your changes, licenses and build/install scripts. Then run:

~~~powershell
.\build.bat --web
$env:FM1_PI32_TOOLCHAIN = 'C:\JL\pi32'
.\build.bat --package
~~~

For a local firmware build without a distribution source URL, run `./sdk/Build.ps1 -FirmwareOnly -Toolchain 'C:\JL\pi32'`. A moving GitHub `tree/main` page is not a matching source archive for distribution.

The web version builds from the included C sources. Its first build downloads WASI SDK 27; set `FM1_WASI_SDK` to reuse an installation. Outputs are `build/preview/index.html`, `build/dx7/generated/FM1_OFFICIAL_OPEN.fwsc` and `build/official-web/FM1-Official-Open-Source.html`.

The [per-track tuning API](app/instrument.h) supports custom tuning; a dedicated tuning editor is not included.

## License

APP: [GPLv3 with the FM1 SDK linking exception](LICENSE.md). The device engine is supplied as a binary SDK with separate terms. Retain the applicable [SDK terms, third-party licenses and contributor notices](THIRD_PARTY.md) when distributing builds.
