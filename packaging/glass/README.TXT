Glass98 - A01

Requires Windows 98 SE, Internet Explorer 6 SP1, High/True Color.
The widgets are HTML Active Desktop wallpaper, behind normal windows.

INSTALL / UPDATE
1. Extract the entire package to any folder or removable drive.
2. Close the widget manager and run INSTALL.BAT from that package.
   It copies the application to C:\Glass98 and completes registration.
   A writable TEMP directory and enough space for a temporary package copy
   are required. You can remove the source media after installation.
3. Open Manage widgets at the bottom-right corner of the desktop.

To upgrade, run INSTALL.BAT from the NEW package. No manual removal or file
copy is needed. Existing WIDGETS.INI settings are retained where valid; the
previous file is backed up as WIDGETS.BAK.INI before changes.

On a fresh install, Glass98 uses the current Windows desktop wallpaper and
its center/tile/stretch setting. If no usable image is set, it uses the bundled
640x480 solid Windows 98 teal bitmap, stretched to cover the desktop.
Upgrades keep your selected Glass98 wallpaper. The original Windows wallpaper
and style are saved in GLASS.INI and restored on removal.

UNINSTALL
Run C:\Glass98\REMOVE.BAT (or REMOVE.BAT in the package). It deregisters the
application, stops its helpers, restores the previous wallpaper, and deletes
files in C:\Glass98 except REMOVE.BAT and all .INI files. It does not recurse
into subfolders. The retained INIs allow a later reinstall to reuse settings.
If a file cannot be removed, setup reports the failure and keeps the removal
helper for retry. Nothing outside the installation directory is deleted.

CONTROLS
Newly enabled widgets are measured on the desktop, placed below the current
rightmost column, and start a column to the left when vertical space runs out.
Existing widgets are not rearranged. Leave a few seconds after enabling for
collection, sizing and placement. Gaps left by disabled widgets can be reused.
If the desktop is full, the new widget is marked Hidden instead of overlapping
other widgets. Disable something else, then enable the widget you want.

The manager opens in a resizable window. Widgets has a visibility list
beside the selected widget's settings; each pane scrolls separately. Narrow
windows use a widget dropdown instead. Apply/Close and save status stay visible.
Themes has an applied-appearance preview with example values and editable meter
colors. Theme actions apply immediately. Arrange widgets is on Desktop.

Drag titles to move widgets. Shift bypasses snapping; Lock positions
prevents dragging. Snap distance is 12 pixels, with a 10-pixel widget gap.
Shared width is 240-600 pixels. Heights follow the content; long content
scrolls when needed. Arrange widgets
reflows the current resolution using measured widget sizes and hides overflow.

Manage widgets has Widgets, Desktop and Themes tabs. Widgets holds individual options;
Desktop holds shared width, wallpaper and Global refresh. Enable the global
override to use one interval for all widgets; uncheck to restore individual
intervals. Buttons respond independently of either interval.

OPAQUE RENDERING / SLOW SYSTEMS
Manage widgets > Desktop > Rendering > Disable transparency (opaque widgets).
Click Apply desktop. Widget backgrounds become solid; alpha filters are removed
and the separate transparency layers are hidden, including the theme preview.
This is not just opacity set to 100 percent. It is a global setting shared by
all resolutions and is unaffected by changing themes. Saved widget opacities
are retained; uncheck it and Apply desktop to restore them. No restart is needed.

THEMES
From your wallpaper: choose Automatic, Dark panels or Light panels, then
Generate and apply. It analyzes the image selected on the Desktop tab and
chooses background/text/accent/meter colors and opacity for sampled contrast.
The meter-color on/off setting is preserved. Save current as new keeps a named
copy; Previous appearance restores the look before generation.
Check Regenerate when I choose a new wallpaper before generating to follow
image selections (and wallpaper layout-mode changes) in this manager.
Stop following keeps the current appearance. Applying another theme stops it.
BMP/JPEG/GIF only: up to 16 MiB, 12 megapixels and 8192 pixels per side.
No internet or background sampling is needed. Failed analysis keeps the current
theme. Analysis samples the image; small details, animation and display settings
can affect real readability. Colors and opacity can still be adjusted manually.

Choose Classic glass, Graphite, Midnight blue, Amber terminal or Paper on the
Themes tab and click Apply theme. Your previous look is kept as Previous
appearance in the dropdown; applying it swaps the current and previous looks.
Themes keep per-widget background, text, accent and opacity, plus meter colors.
They do not change widget selection, positions, contents, wallpaper or refresh.

To save your own theme, first Apply appearance edits on the Widgets tab. Then
choose meter settings on Themes, enter a name and click Save current as new.
Up to 16 custom themes are kept in WIDGETS.INI: preserve it when upgrading.
Replace selected and Delete selected work only on your saved themes.
Manual changes show as Custom (unsaved) until saved; switching never overwrites
named themes. Save a named theme for long-term preservation of your work.

Color meters by level is optional. Low, middle and high colors blend smoothly
at 0%, 50% and 100%. Battery charge and free system resources reverse the scale:
full/free is healthy, empty is critical. CPU/RAM/disk use increases normally.
Performance traffic colors follow the displayed graph scale, not device capacity.
Turning level colors off restores the widget accent color. Volume meters
always use the accent. Apply meter colors saves just the flag and palette.

Each widget has its own background, text/accent colors, opacity (0-100),
and refresh period (1-3600 seconds). Colors use six hex digits without #.
Only the background is transparent. Apply colors to all is optional.
Refresh controls display updates. CPU/RAM collection stops unless System or
Performance is enabled. Network collection stops unless Network, Network tools or Performance
is enabled. Other hardware collectors run only for their enabled widgets.
The resident helper retains lightweight settings checks. Opening Network/CD
options performs one discovery scan. Enabled CPU/network sampling stays at
one second. Disk capacity queries follow the effective refresh interval;
changing selected letters triggers an immediate rescan.

WIDGETS
System: CPU and RAM bars, CPU name, integer MB memory figures, display
adapter/mode, Windows version and uptime. Driver-supplied GPU names and
refresh rates may be incomplete. Uptime wraps after about 49.7 days.
CPU frequency is detected once per collector process. Startup estimates on
older CPUs and TSC reference rates on modern CPUs are labeled separately
from base frequency; these are not live turbo/throttling readings.
CPU frequency: a successful startup reading is saved. If a later calibration
fails, a reading saved for the same detected CPU is shown as cached. Without
a matching saved reading, the frequency line says unavailable. Calibration
still runs only once per collector process; this is not a live frequency meter.
Disks: select drive letters, including currently absent ones. New settings
select only C:. Optional automatic selection covers C:-Z:.
Empty media is labelled; missing letters disappear.
Network mappings are listed without capacity queries to avoid blocking.
Network: select interfaces; rates, monitored byte totals, IP, gateway and
hostname. Totals cover monitored periods and reset with the helper.
Clock: digital, with 12-hour or 24-hour mode.
Up to six extra clocks: one per line, e.g. Tokyo|+09:00 or New York|-04:00.
Extra clocks have fixed UTC offsets: adjust them for daylight saving time.
Calendar: month navigation, today highlight, dated notes. Long notes wrap and
scroll separately from the grid. Notes do not trigger alarms.
Battery: percentage, charging/AC state, and remaining runtime when the driver
reports it while on battery. This API does not provide time-to-full-charge.
Volume: master, Wave, CD and MIDI adjustment/mute where the mixer exposes
those controls. Uses the first mixer device. +/- steps are 10 percent.
Winamp: classic Winamp 2.x-compatible IPC, track/window title, transport,
and volume. Winamp must already be running.
Audio CD: choose a CD drive, Open it, then control physical audio CDs via
MCI. Shows current track and track count. Requires a working CD audio driver
and audio hardware/path. Edit titles opens CDTITLES.INI in Notepad with a
section for the current disc: Album, Artist, Track1, Track2, and so on. Save it
to update the widget. Titles are local; no online lookup or extra library is
needed. Keep CDTITLES.INI to preserve your disc collection.
If the package includes CDMETA.DAT, setup offers to install the optional
offline MusicBrainz title database and shows its disk-space requirement.
Titles are then matched locally; no Internet connection is used. Your own
INI titles override database entries. Multiple matches can be selected with
the Match ... Next button. See CDDATA.TXT for coverage and license details.
Quick Launch: six compact file/program/shortcut/folder slots with optional
labels. Direct files start with their containing folder as working directory.
Shortcuts retain their own Start in directory. Use .lnk files for arguments
and .cpl files for Control Panel items.
News: a configured RSS/Atom feed, rotating plain-text titles. Basic feed
parsing, at most 12 titles / 256 KiB. Uses installed WinInet; many modern
HTTPS feeds cannot connect. No certificate checks are bypassed.
Performance: rolling 60-sample CPU, RAM and selected-interface traffic
history, plus File read and File write graphs. File activity includes cache
access and the helper's own snapshot writes; it is not physical disk busy time
or per-drive throughput. Sample spacing follows the widget/global interval.
Network tools: click to ping a configured hostname/IPv4 address or an adapter's
gateway. Also opens Network settings. No automatic internet probes. A timeout
does not prove a host is offline; ICMP may be blocked.
System resources: free SYSTEM, USER and GDI percentages. Uses the included tiny
RSRC16.EXE helper only while enabled. These resource pools are separate from RAM.
Sticky notes: text or [ ] / [x] checklist lines. Click a checkbox to toggle it;
Edit opens this widget's settings. Up to 900 UTF-8 input bytes, stored using the
Windows code page. Long notes scroll.
Timer: countdown and stopwatch, start/pause/resume/reset, optional finish sound.
Reset applies configured mode and duration. State survives reloads/reboots.
Running timers continue across shutdown; changing the system clock affects
them. Alarms are checked only while the widget is enabled on the desktop.
Places: four favorite folders, plus eight newest Windows Recent Documents.
Recent items refresh every ten seconds; shortcut launch settings are preserved.
Removable media: C:-Z: removable/CD drives, media readiness, Open and CD Eject.
Checks every five seconds. A:/B: are skipped. Drives identified by Windows as
fixed disks are handled by the Disks widget. There is no generic USB eject.
Picture frame: choose up to six BMP/JPEG/GIF files. Previous/Next or slideshow;
set slideshow seconds to 0 for manual use. Images fit without stretching and
stay at their selected paths. Missing/unsupported images are labelled.
Event countdown: label and YYYY-MM-DD date, days remaining/Today/days elapsed.
Desktop controls: choose up to six wallpaper images, then use Previous/Next.
Start or enable/disable the screensaver, apply a saved Glass98 theme, or choose
from the Windows Appearance schemes installed on this computer. Applying a
Windows scheme uses the Display applet to update colors, fonts and sizes.
Custom schemes saved in Display Properties / Appearance are also listed.
Memory details: physical RAM, Windows-reported paging capacity, collector
virtual address space and largest free virtual block. Paging capacity is not
the swap file size on disk; virtual readings are for the collector process.
The largest-block scan runs at most once every ten seconds.
The paging-capacity percentage uses Windows-reported total/available values.
Automatic sizing can change that capacity; 100% is not necessarily a fixed system limit.
Calculator: arithmetic with parentheses and + - * / % (remainder) ^ (power),
unit conversion, and unsigned 32-bit hexadecimal/decimal/binary conversion.
Character map: Latin characters, configurable font, click to copy. A character
must be supported by the active Windows code page to copy as plain text.
Daily checklist: up to 24 task lines and 900 UTF-8 input bytes. Check marks reset
at local midnight. Editing the task list clears its check marks.
Clipboard: plain-text preview, up to 2048 bytes, with no history. While enabled,
its preview is in the local telemetry file; disabling clears it on the next
collection. Images and file lists are not displayed.

Disabled collectors release file counters, resource helper and mixer handles.
Notes, clocks, timers, pictures and event countdowns need no hardware collection.

WALLPAPER / REMOVAL
Choose an existing BMP/JPEG/GIF using Manage widgets. Center, Tile, Stretch
and Fit are supported. Keep the image at that path; it is not copied.
Run REMOVE.BAT to stop the helper and restore the original wallpaper.
WIDGETS.INI contains your personal settings and saved themes.

RESOLUTION LAYOUTS
Widget visibility and positions are saved separately for each resolution.
The first visit to a resolution copies the current selection and fits widgets
from the right-hand columns, top to bottom. Widgets that do not fit are hidden;
the desktop reports their count and the manager labels them Hidden. You can
disable other widgets and enable the ones you need. Returning to a known
resolution restores its selection and positions. Arrange widgets retries fitting
hidden widgets after arranging the currently visible ones.

Appearance, width, wallpaper, refresh intervals and widget options remain shared.
Color depth does not create a separate layout. Work-area changes also trigger
fitting. Hidden widgets do not collect data unless another visible widget needs
that data. Switching profiles does not refresh or reinstall Active Desktop.

LICENSE
Glass98 project code is MIT licensed; see LICENSE.TXT.
Open Watcom runtime portions retain their own license; see NOTICE.TXT and
WATCOM.TXT. Source locations and acknowledgements are in NOTICE.TXT.
