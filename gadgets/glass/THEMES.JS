/* IE6-compatible themes and load-dependent meter colors. */
var themeList = [],
    themeActive = -1,
    themeRevision = '0',
    themeSeen = '',
    themeCatalog = '',
    themeAutoMode = 0,
    themeAutoFollow = 0,
    themeAutoStatus = '',
    themeAutoImage = '';

function hexColor(value, fallback) {
    return /^[0-9a-fA-F]{6}$/.test(String(value || '')) ? String(value) : fallback;
}

function mixColor(a, b, t) {
    var s = '',
        j, v;
    for (j = 0; j < 6; j += 2) {
        v = Math.round(parseInt(a.substr(j, 2), 16) * (1 - t) + parseInt(b.substr(j, 2), 16) * t).toString(16);
        s += (v.length < 2 ? '0' : '') + v;
    }
    return s.toUpperCase();
}

function levelColor(value, i, reverse) {
    if (savedOptions.meterColors != '1' || !isFinite(value) || value < 0) return panels[i].accent;
    value = Math.max(0, Math.min(100, reverse ? 100 - value : value));
    var low = hexColor(savedOptions.meterLow, '62D98B'),
        mid = hexColor(savedOptions.meterMid, 'FFD166'),
        high = hexColor(savedOptions.meterHigh, 'FF6262');
    return value <= 50 ? mixColor(low, mid, value / 50) : mixColor(mid, high, (value - 50) / 50);
}

function meterColor(value, i) {
    return i == 0 || i == 1 || i == 6 || i == 14 ? levelColor(value, i, i == 6 || i == 14) : panels[i].accent;
}
/* Style generated markup before insertion: avoid walking/mutating IE6 live DOM collections. */
function themeMarkup(s, i) {
    var p = panels[i];
    return s.replace(/<a /g, '<a style="color:#' + p.accent + ';border-color:#' + p.accent + '" ').replace(
        /<span class="launchfile"/g, '<span style="color:#' + p.fg + '" class="launchfile"').replace(
        /<td class="today"/g, '<td style="border-color:#' + p.accent + '" class="today"');
}

function themeBusy() {
    if (awaiting || inFlight || optionQueue.length) {
        el('notice').innerText = 'Please wait for Settings saved.';
        return true;
    }
    return false;
}

function themeQueue(commands) {
    optionQueue = commands;
    awaiting = 'pending';
    waitSince = (new Date()).getTime();
    el('notice').innerText = 'Saving theme settings...';
}

function themeMeterCommand() {
    var a = [el('meterColors').checked ? '1' : '0'],
        keys = ['meterLow', 'meterMid', 'meterHigh'],
        j, v;
    for (j = 0; j < 3; j++) {
        v = el(keys[j]).value;
        if (!/^[0-9a-fA-F]{6}$/.test(v)) {
            el('notice').innerText = 'Meter colors must have six hexadecimal digits, without #.';
            return '';
        }
        a[a.length] = v.toUpperCase();
    }
    return 'theme/meter/' + a.join(',');
}

function themeApply() {
    if (themeBusy()) return;
    var id = Number(el('themeChoice').value);
    if (id < 0) {
        el('notice').innerText = 'Choose a saved or built-in theme first.';
        return;
    }
    themeQueue(['theme/apply/' + id]);
}

function themeGenerate() {
    if (themeBusy()) return;
    themeQueue(['theme/auto/' + num(el('autoStyle').value, 0, 2) + '/' + (el('autoFollow').checked ? '1' : '0')]);
    el('notice').innerText = 'Analyzing wallpaper and checking contrast...';
}

function themeUnfollow() {
    if (themeBusy()) return;
    themeQueue(['theme/unfollow']);
}

function themeMeters() {
    if (themeBusy()) return;
    var cmd = themeMeterCommand();
    if (cmd) themeQueue([cmd]);
}

function themeSave(replace) {
    if (themeBusy()) return;
    var id = Number(el('themeChoice').value),
        cmd = themeMeterCommand(),
        name = el('themeName').value.replace(/^\s+|\s+$/g, ''),
        raw, h = '',
        j, v;
    if (!cmd) return;
    if (replace) {
        if (id < 100 || id >= 116) {
            el('notice').innerText = 'Choose one of your saved themes to replace.';
            return;
        }
        if (!window.confirm('Replace this saved theme with your currently applied widget appearance?')) return;
        themeQueue([cmd, 'theme/update/' + id]);
        return;
    }
    raw = unescape(encodeURIComponent(name));
    if (!raw.length || raw.length > 32 || /[\x00-\x1f\"]/.test(name)) {
        el('notice').innerText = 'Use a name of 1-32 UTF-8 bytes, without quotes or control characters.';
        return;
    }
    var count = 0;
    for (j = 0; j < themeList.length; j++)
        if (themeList[j].id >= 100 && themeList[j].id < 116) {
            count++;
            if (String(themeList[j].name).toLowerCase() == name.toLowerCase()) {
                el('notice').innerText = 'That name is already saved. Select it and use Replace selected.';
                return;
            }
        } if (count >= 16) {
        el('notice').innerText = 'All 16 custom theme slots are used. Replace or delete one first.';
        return;
    }
    for (j = 0; j < raw.length; j++) {
        v = raw.charCodeAt(j).toString(16);
        h += (v.length < 2 ? '0' : '') + v;
    }
    themeQueue([cmd, 'theme/new/' + h]);
}

function themeDelete() {
    if (themeBusy()) return;
    var id = Number(el('themeChoice').value);
    if (id < 100 || id >= 116) {
        el('notice').innerText = 'Built-in themes and Previous appearance cannot be deleted.';
        return;
    }
    if (window.confirm('Delete this saved theme? Your current desktop appearance will stay as it is.')) themeQueue([
        'theme/delete/' + id
    ]);
}

function updateThemePreview() {
    var low = hexColor(el('meterLow').value, '62D98B'),
        mid = hexColor(el('meterMid').value, 'FFD166'),
        high = hexColor(el('meterHigh').value, 'FF6262'),
        j, v;
    for (j = 0; j <= 10; j++) {
        v = el('meterColors').checked ? (j <= 5 ? mixColor(low, mid, j / 5) : mixColor(mid, high, (j - 5) / 5)) :
            panels[0].accent;
        el('palette' + j).style.backgroundColor = '#' + v;
    }
    if (settingsView) managerPreview();
}

function loadThemes() {
    var list = el('themeChoice'),
        j, name = 'Custom (unsaved)',
        p = panels[Number(el('target').value) || 0];
    var catalog = '';
    for (j = 0; j < themeList.length; j++) catalog += '|' + themeList[j].id + ':' + themeList[j].name;
    if (catalog != themeCatalog) {
        themeCatalog = catalog;
        list.options.length = 0;
        list.options.add(new Option('Custom (unsaved)', '-1'));
        for (j = 0; j < themeList.length; j++) list.options.add(new Option((themeList[j].id < 100 ? 'Built-in: ' :
            themeList[j].id >= 1000 ? '' : 'Saved: ') + themeList[j].name, String(themeList[j].id)));
    }
    for (j = 0; j < themeList.length; j++)
        if (themeList[j].id == themeActive) name = themeList[j].name;
    list.value = String(themeActive);
    el('themeCurrent').innerText = 'Active appearance: ' + name;
    el('meterColors').checked = savedOptions.meterColors == '1';
    el('meterLow').value = hexColor(savedOptions.meterLow, '62D98B');
    el('meterMid').value = hexColor(savedOptions.meterMid, 'FFD166');
    el('meterHigh').value = hexColor(savedOptions.meterHigh, 'FF6262');
    if (p) {
        el('bg').value = p.bg;
        el('fg').value = p.fg;
        el('accent').value = p.accent;
        el('alpha').value = p.alpha;
        el('allColors').checked = false;
    }
    el('autoStyle').value = String(themeAutoMode);
    el('autoFollow').checked = themeAutoFollow != 0;
    el('autoResult').innerText = themeAutoStatus || 'No wallpaper theme generated yet.';
    el('autoImage').innerText = 'Wallpaper: ' + savedWallpaper;
    updateThemePreview();
}

function themeTick() {
    if (!settingsView || awaiting || inFlight) return;
    var stamp = themeRevision + '|' + themeActive + '|' + themeAutoMode + '|' + themeAutoFollow + '|' +
        themeAutoStatus + '|' + savedWallpaper,
        j;
    for (j = 0; j < themeList.length; j++) stamp += '|' + themeList[j].id + ':' + themeList[j].name;
    if (stamp != themeSeen) {
        themeSeen = stamp;
        loadThemes();
    }
}
