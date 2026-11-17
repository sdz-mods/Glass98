# Glass98 desktop and settings helpers

GLASSCTL installs/removes the HTML wallpaper, preserving the previous wallpaper
in GLASS.INI. GLASSPRF handles the widget manager's local commands, settings,
themes and per-resolution layouts in WIDGETS.INI. G98SETUP stages the complete
package, stops the existing installation and installs/updates C:\Glass98.

Build the release using scripts/package-glass.ps1. User installation and removal
are through the package's INSTALL.BAT and REMOVE.BAT. The package source folder
can be removed after installation. See the root README and package manual.

One package supports Win98 SE and 32-bit XP. Setup chooses native CPU accounting
on XP and never registers a VxD there. The XP manager is a local MANAGER.HTA
launched by GLASSPRF; Win98 opens SETTINGS.HTM in its browser host. The desktop
remains an Active Desktop HTML wallpaper on both systems.
