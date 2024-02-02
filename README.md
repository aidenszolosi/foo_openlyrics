# foo_openlyrics

[![](.github/readme/bmc-button.png)](https://www.buymeacoffee.com/jacquesheunis)
[![Donate](https://liberapay.com/assets/widgets/donate.svg)](https://liberapay.com/jacquesheunis/donate)

An open-source lyrics plugin for [foobar2000](https://www.foobar2000.org/) that includes its own UI panel for displaying and sources for downloading lyrics that are not available locally. It is intended to be a replacement for LyricShowPanel3 so it is fully-featured and supports lyric searching, saving and editing directly from within foobar2000.

## 1.9 Nightly Changelog (commits from https://github.com/ditstyle-yang/foo_openlyrics)
(commit ids: https://github.com/jacquesh/foo_openlyrics/compare/master...ditstyle-yang:foo_openlyrics:master)

*  Add an option to only save unsynced lyrics (Commit: e17156b)

*  Auto-search can now find lyrics when the source provides no album (Commit: fc75d79)

*  Fix lyrics being re-saved when loaded from another local source (Commit: e853244)

*  Automatically highlight the first manual search result (Commit: 2a6aa71)

*  Fix lyrics not saving to tags if there was already a lyric tag (Commit: 9100fa6)

*  Manual searches now query sources in parallel (Commit: 0f815b9)

*  Update version number for v0.9 release (Commit: 4623d0b)

*  Bump the version number post-release of v0.9 (Commit: 108f0a2)

*  Fix ColumnsUI config export complaints of corrupted file (Commit: 783662a)

*  Make auto-searches case-insensitive when matching track metadata (Commit: 0d3845a)

*  Support applying a manual search result by double-clicking it (Commit: de1062c)

*  Allow sorting the manual search result list by any column (Commit: 2780a1b)

*  Allow applying lyrics from manual search without closing the dialog (Commit: f612289)

*  Remember modified column widths in the manual search table UI (Commit: edc6f2b)

* Add a bulk search option to the playlist context menu (Commit: d3c8bf5)

* Avoid auto-searching a track after a while if it keeps failing (Commit: 5af74f7)

## Features
* Buttery-smooth lyric scrolling (either horizontally or vertically)
* Supports retrieving lyrics from local files, ID3 tags or the internet
* Customise the font & colours to perfectly suite your layout & theme
* Easily edit lyrics directly inside foobar2000 with built-in support for timestamps
* Check the saved lyrics of any track in your library (whether it is currently playing or not)
* Apply common edits (such as removing blank lines) in just 2 clicks
* ...and more!

## Screenshots
Fonts & colours are fully configurable
![](.github/readme/lyrics_vertical_scroll.gif)

Horizontal scrolling is also supported
![](.github/readme/lyrics_horizontal_scroll.gif)

The editor window
![](.github/readme/editor.jpg)

## How to install foo_openlyrics
1. Find the latest [release on Github](https://github.com/jacquesh/foo_openlyrics/releases).
2. Download the `fb2k-component` file attached to the release (don't worry about the `debug_symbols` zip file).
3. Double-click on the file you just downloaded. Assuming foobar2000 is installed, it should open up with the installation dialog. Restart foobar2000 when asked.
4. Add the "OpenLyrics Panel" to your layout.

## Why another lyrics plugin?
At the time that I started this, the most widely-used lyrics plugin was [foo_uie_lyrics3](https://www.foobar2000.org/components/view/foo_uie_lyrics3) which had several built-in sources but those had largely stopped working due to the relevant websites going down or otherwise becoming generally unavailable. The original developer seemed to be nowhere in sight though and the source for the plugin did not appear to be available anywhere online. There is an SDK for building one's own sources for foo_uie_lyrics3 but building plugins for plugins didn't really take my fancy. Other (more up-to-date) plugins did exist but were mostly distributed by people posting binaries for you to download from their Dropbox on Reddit. Running binaries published via Dropbox by random people on Reddit did not seem like the most amazing idea.

## Contributing
Please do log an issue or send a pull request if you have found a bug, would like a feature added. If you'd like to support the project you can also [make a small donation](https://www.buymeacoffee.com/jacquesheunis).
