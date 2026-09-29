/* IE6-compatible desktop UI. No canvas, SVG, modern JS, ActiveX or HTML injection from telemetry. */
var ids = ['system', 'disks', 'network', 'clock', 'calendar', 'reserved', 'battery', 'volume', 'winamp', 'cd', 'launch',
    'rss', 'graphs', 'nettools', 'resources', 'sticky', 'timer', 'places', 'media', 'picture', 'event',
    'reservedVu', 'desktop', 'memory', 'calculator', 'characters', 'daily', 'clipboard'
];
var titles = ['System', 'Disks', 'Network', 'Clock', 'Calendar', '', 'Battery', 'Volume', 'Winamp', 'Audio CD',
    'Quick Launch', 'News', 'Performance', 'Network tools', 'System resources', 'Sticky notes', 'Timer', 'Places',
    'Removable media', 'Picture frame', 'Event countdown', '', 'Desktop controls', 'Memory details', 'Calculator',
    'Character map', 'Daily checklist', 'Clipboard'
];
var heights = [306, 210, 240, 118, 255, 195, 112, 236, 145, 130, 175, 175, 245, 180, 160, 150, 130, 200, 150, 260, 140,
        150, 260, 260, 200, 245, 170, 200
    ],
    panels = [],
    width = 280,
    locked = 0,
    snap = 1,
    wallmode = 0;
var measured = [],
    inFlight = '',
    sentAckTime = 0,
    sentAt = 0;
var renderingModeSeen = null;
var optionsStamp = '',
    lastExtra = 0,
    fastUntil = 0,
    fastRunning = false,
    pendingDevice = null,
    inspectWaiting = 0,
    settingsNetworks = [];
var applied = '',
    awaiting = '',
    waitSince = 0,
    dragging = -1,
    dx = 0,
    dy = 0,
    wall = '',
    wallLoader = null,
    naturalW = 0,
    naturalH = 0,
    lastDraw = [],
    history = [],
    lastHistory = 0,
    lastRss = 0,
    lastRssURL = '',
    optionQueue = [],
    calendarOffset = 0;

function el(id) {
    return document.all[id];
}

function esc(s) {
    return String(s == null ? '' : s).replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;').replace(/"/g,
        '&quot;');
}

function num(v, min, max) {
    v = Number(v);
    return isNaN(v) ? min : Math.max(min, Math.min(max, Math.round(v)));
}

function bounds() {
    return [screen.availWidth, screen.availHeight];
}

function defaults() {
    var a = [3, 280, 0, 1, 0],
        i;
    for (i = 0; i < ids.length; i++) a = a.concat([i == 0 ? 1 : 0, 32000, i ? 350 : 1, '000000', 'FFFFFF', '80FFFF', 55,
        i == 1 ? 10 : 2
    ]);
    return a.join('|');
}

function parse(s) {
    var a = String(s).split('|'),
        i, b;
    if (a.length == 109 && a[0] == '2') {
        a[0] = '3';
        for (i = 13; i < ids.length; i++) a = a.concat([0, 32000, 350, '000000', 'FFFFFF', '80FFFF', 55, 2]);
    }
    if (a.length == 181 && a[0] == '3') {
        for (i = 22; i < ids.length; i++) a = a.concat([0, 32000, 350, a[8], a[9], a[10], a[11], 2]);
    }
    if (a.length != 5 + ids.length * 8 || a[0] != '3') return false;
    for (i = 1; i < a.length; i++) {
        b = i < 5 ? -1 : (i - 5) % 8;
        if (b >= 3 && b <= 5) {
            if (!/^[\da-fA-F]{6}$/.test(a[i])) return false;
        } else if (!/^\d+$/.test(a[i])) return false;
    }
    width = num(a[1], 240, 600);
    locked = num(a[2], 0, 1);
    snap = num(a[3], 0, 1);
    wallmode = num(a[4], 0, 3);
    for (i = 0; i < ids.length; i++) {
        b = 5 + i * 8;
        panels[i] = {
            on: (i == 5 || i == 21) ? 0 : num(a[b], 0, 1),
            x: num(a[b + 1], 0, 32000),
            y: num(a[b + 2], 0, 32000),
            bg: a[b + 3],
            fg: a[b + 4],
            accent: a[b + 5],
            alpha: num(a[b + 6], 0, 100),
            seconds: num(a[b + 7], 1, 3600)
        };
        if (!settingsView) {
            if (panels[i].x == 32000 && panels[i].y == 32000) markPlacement(i);
            else pendingPlacement[i] = null;
        }
    }
    return true;
}

function serialize() {
    var a = [3, width, locked, snap, wallmode],
        i, p;
    for (i = 0; i < panels.length; i++) {
        p = panels[i];
        a = a.concat([p.on, p.x, p.y, p.bg, p.fg, p.accent, p.alpha, p.seconds]);
    }
    return a.join('|');
}

function clamp(i) {
    var p = panels[i],
        b = bounds();
    p.x = num(p.x, 1, Math.max(1, b[0] - width - 2));
    p.y = num(p.y, 1, Math.max(1, b[1] - (measured[i] ? heights[i] : 40) - 3));
}
/* Opaque mode removes the filter and hides the separate compositing layer.
   Saved per-widget opacity stays untouched for switching glass back on. */
function panelBackground(t, c, p) {
    if (savedOptions.disableAlpha == '1') {
        t.style.display = 'none';
        if (t._alpha != null || t.style.filter) {
            t.style.filter = '';
            t._alpha = null;
        }
        c.style.backgroundColor = '#' + p.bg;
        return;
    }
    c.style.backgroundColor = 'transparent';
    t.style.backgroundColor = '#' + p.bg;
    t.style.display = 'block';
    if (t._alpha != p.alpha) {
        if (t._alpha == null) t.style.filter = 'alpha(opacity=' + p.alpha + ')';
        else t.filters.item('alpha').opacity = p.alpha;
        t._alpha = p.alpha;
    }
}

function opacityControls() {
    if (!settingsView) return;
    var disabled = savedOptions.disableAlpha == '1';
    el('alpha').disabled = disabled;
    el('opacityHint').style.display = disabled ? 'block' : 'none';
}

function syncRenderingMode() {
    var disabled = savedOptions.disableAlpha == '1';
    if (renderingModeSeen === disabled) return;
    renderingModeSeen = disabled;
    drawAll();
    if (settingsView) {
        el('disableAlpha').checked = disabled;
        opacityControls();
        managerPreview();
    }
}

function draw(i) {
    if ((i == 5 || i == 21)) return;
    var p = panels[i],
        w = el('w' + i),
        t = el('t' + i),
        c = el('c' + i);
    if (savedOptions.disableAlpha == '1') panelBackground(t, c, p);
    if (settingsView || !p.on) {
        w.style.display = 'none';
        return;
    }
    clamp(i);
    w.style.visibility = displayBusy || (pendingPlacement[i] && !pendingPlacement[i].sent) ? 'hidden' : 'visible';
    w.style.display = 'block';
    w.style.pixelLeft = p.x;
    w.style.pixelTop = p.y;
    w.style.pixelWidth = width;
    if (t._width != width) {
        t.style.pixelWidth = width;
        t._width = width;
    }
    if (t._height != screen.availHeight) {
        t.style.pixelHeight = screen.availHeight;
        t._height = screen.availHeight;
    }
    if (savedOptions.disableAlpha != '1') panelBackground(t, c, p);
    c.style.pixelWidth = width;
    c.style.color = '#' + p.fg;
    c.style.borderColor = '#' + p.accent;
    el('h' + i).style.color = '#' + p.accent;
    el('h' + i).style.cursor = locked ? 'default' : 'move';
}

function drawAll() {
    for (var i = 0; i < ids.length; i++) draw(i);
}

function pollFast() {
    var v = (new Date()).getTime();
    if (v > fastUntil) {
        fastRunning = false;
        return;
    }
    el('extra').src = 'EXTRA.JS?v=' + v;
    if (inFlight || optionQueue.length) el('ackfeed').src = 'ACK.JS?v=' + v;
    el('prefs').src = 'PREFS.JS?v=' + v;
    window.setTimeout(function() {
        tick();
        pollFast();
    }, 120);
}

function command(s) {
    if (s.indexOf('device/') == 0) {
        var a = s.split('/'),
            i = a[1] == 'volume' ? 7 : a[1] == 'winamp' ? 8 : a[1] == 'cd' ? 9 : -1;
        if (i >= 0) {
            pendingDevice = {
                parts: a,
                widget: i,
                at: (new Date()).getTime()
            };
            applyOptimistic();
            window.setTimeout(function() {
                render(i);
                lastDraw[i] = 0;
            }, 25);
        }
        fastUntil = (new Date()).getTime() + 2500;
        if (!fastRunning) {
            fastRunning = true;
            window.setTimeout(pollFast, 120);
        }
    }
    el('commandSink').src = 'w98widgetsglass:' + s;
}

function applyOptimistic() {
    if (!pendingDevice) return;
    var a = pendingDevice.parts;
    if (a[1] == 'volume' && mixer[Number(a[2])]) mixer[Number(a[2])][a[3] == '1' ? 'mute' : 'volume'] = Number(a[4]);
    if (a[1] == 'winamp') {
        if (a[2] == 'volume') winamp.volume = Number(a[3]);
        if (a[2] == 'play') winamp.state = 1;
        if (a[2] == 'stop') winamp.state = 0;
        if (a[2] == 'pause') winamp.state = pendingDevice.pauseState == null ? (pendingDevice.pauseState = winamp
            .state == 3 ? 1 : 3) : pendingDevice.pauseState;
    }
    if (a[1] == 'cd') {
        if (a[2] == 'play') cdState = 'Playing (requested)';
        if (a[2] == 'pause') cdState = 'Paused (requested)';
        if (a[2] == 'stop') cdState = 'Stopped (requested)';
    }
}

function effectiveSeconds(i) {
    return num(savedOptions.globalSeconds || 0, 0, 3600) || panels[i].seconds;
}

function showTab(name) {
    var names = ['widget', 'desktop', 'themes'],
        j;
    for (j = 0; j < names.length; j++) {
        el(names[j] + 'Tab').style.display = name == names[j] ? 'block' : 'none';
        el(names[j] + 'TabButton').className = name == names[j] ? 'tab active' : 'tab';
    }
    el('applyButton').style.display = name == 'themes' ? 'none' : '';
    if (settingsView) managerTab(name);
}

function fitContent(i) {
    var b = el('b' + i),
        flow = el('flow' + i),
        head = el('h' + i),
        notes = el('notes'),
        natural, limit, bodyHeight, next, oldY;
    if (!panels[i].on || settingsView) return;
    if (i == 4 && notes && notes.scrollHeight > 90 && notes.style.pixelHeight != 90) {
        notes.style.pixelHeight = 90;
        notes.style.overflow = 'auto';
    }
    if (i == 15 && el('stickyBody') && el('stickyBody').scrollHeight > 240) el('stickyBody').style.pixelHeight = 240;
    natural = (i == 3 ? 0 : (head.offsetHeight || 25)) + (flow.offsetHeight || 15) + 20;
    limit = Math.max(60, bounds()[1] - 4);
    if (natural > limit) {
        bodyHeight = limit - (i == 3 ? 0 : (head.offsetHeight || 25)) - 2;
        if (b.style.pixelHeight != bodyHeight) b.style.pixelHeight = bodyHeight;
        b.style.overflow = 'auto';
    } else {
        if (b.style.height != 'auto') b.style.height = 'auto';
        b.style.overflow = 'visible';
    }
    next = Math.min(natural, limit);
    measured[i] = true;
    if (next != heights[i]) {
        heights[i] = next;
        oldY = panels[i].y;
        clamp(i);
        if (oldY != panels[i].y) el('w' + i).style.pixelTop = panels[i].y;
    }
}

function scheduleFit(i) {
    var f = el('flow' + i);
    if (f._width != width) {
        f.style.pixelWidth = Math.max(1, width - 24);
        f._width = width;
    }
    if (!f._measuring) {
        f._measuring = true;
        window.setTimeout(function() {
            f._measuring = false;
            fitContent(i);
        }, 100);
    }
}

function panelCommand(i, action) {
    var p = panels[i];
    return (action == 'browse' ? 'panelbrowse' : 'panel') + '/' + i + '/' + [width, locked, snap, wallmode, p.on, p.x, p
        .y, p.bg, p.fg, p.accent, p.alpha, p.seconds, settingsView && el('allColors').checked ? 1 : 0
    ].join(',') + '/' + (layoutResolution || resolutionKey());
}

function saveNotice(text) {
    if (settingsView) el('notice').innerText = text;
}

function save(action, index) {
    var i = index == null ? (settingsView ? num(el('target').value, 0, ids.length - 1) : 0) : index;
    awaiting = 'pending';
    waitSince = (new Date()).getTime();
    saveNotice('Saving; keep this window open...');
    var options = optionQueue;
    optionQueue = [panelCommand(i, action)];
    for (i = 0; i < options.length; i++) optionQueue[optionQueue.length] = options[i];
}

function queueOption(key, value) {
    var u = unescape(encodeURIComponent(value)),
        h = '',
        j, x;
    for (j = 0; j < u.length; j++) {
        x = u.charCodeAt(j).toString(16);
        h += (x.length < 2 ? '0' : '') + x;
    }
    if (h.length <= 64) optionQueue[optionQueue.length] = 'options/' + key + '=' + h;
    else {
        optionQueue[optionQueue.length] = 'begin/' + key;
        for (j = 0; j < h.length; j += 64) optionQueue[optionQueue.length] = 'part/' + h.substr(j, 64);
        optionQueue[optionQueue.length] = 'commit';
    }
}

function beginDrag(i) {
    if (locked || !resolutionReady()) return;
    dragging = i;
    dx = event.clientX - panels[i].x;
    dy = event.clientY - panels[i].y;
    el('h' + i).setCapture();
    event.returnValue = false;
}

function nearest(v, targets) {
    var best = v,
        dist = 13,
        j, d;
    for (j = 0; j < targets.length; j++) {
        d = Math.abs(v - targets[j]);
        if (d < dist) {
            dist = d;
            best = targets[j];
        }
    }
    return best;
}

function move(x, y, free) {
    var i = dragging,
        p = panels[i],
        b = bounds(),
        tx = [1, b[0] - width - 2],
        ty = [1, b[1] - (measured[i] ? heights[i] : 40) - 3],
        j, q;
    p.x = x;
    p.y = y;
    if (snap && !free) {
        for (j = 0; j < panels.length; j++)
            if (j != i && panels[j].on) {
                q = panels[j];
                if (y < q.y + heights[j] + 24 && y + heights[i] + 24 > q.y) tx = tx.concat([q.x, q.x + width + 10, q.x -
                    width - 10
                ]);
                if (x < q.x + width + 24 && x + width + 24 > q.x) ty = ty.concat([q.y, q.y + heights[j] - heights[i], q
                    .y + heights[j] + 10, q.y - heights[i] - 10
                ]);
            } p.x = nearest(p.x, tx);
        p.y = nearest(p.y, ty);
    }
    clamp(i);
    el('w' + i).style.pixelLeft = p.x;
    el('w' + i).style.pixelTop = p.y;
}
document.onmousemove = function() {
    if (dragging >= 0) move(event.clientX - dx, event.clientY - dy, event.shiftKey);
};
document.onmouseup = function() {
    if (dragging >= 0) {
        var moved = dragging;
        el('h' + dragging).releaseCapture();
        dragging = -1;
        save('save', moved);
    }
};
document.onselectstart = function() {
    if (dragging >= 0) return false;
};

function safeClose() {
    if (awaiting || optionQueue.length) {
        el('notice').innerText = 'Wait for Settings saved before closing.';
        return;
    }
    window.close();
}

function openManager(index) {
    window.open(settingsURL + (index == null ? '' : '?widget=' + index), 'W98WidgetsSettings', 'width=' + Math.min(840,
            screen.availWidth - 24) + ',height=' + Math.min(700, screen.availHeight - 40) +
        ',top=12,left=12,resizable=yes,scrollbars=yes,status=no,toolbar=no,menubar=no,location=no');
}

function resetLayout() {
    if (!resolutionReady()) {
        saveNotice('Please wait for the display layout to switch.');
        return;
    }
    if (awaiting || inFlight || optionQueue.length) return;
    optionQueue = ['arrange/' + layoutResolution];
    awaiting = 'pending';
    waitSince = (new Date()).getTime();
    saveNotice('Arranging widgets for this resolution...');
}

function selected(list, id) {
    return list == '*' || (',' + list + ',').indexOf(',' + id + ',') >= 0;
}

function loadWidget(inspected) {
    var i = num(el('target').value, 0, ids.length - 1),
        p = panels[i],
        s = '',
        j, o = savedOptions;
    el('enabled').checked = p.on;
    el('widgetFitHint').innerText = hiddenForSpace(i) ?
        'Hidden because it did not fit. Disable another widget, then enable this one.' :
        'Visibility and position apply only to this resolution.';
    el('seconds').value = p.seconds;
    el('bg').value = p.bg;
    el('fg').value = p.fg;
    el('accent').value = p.accent;
    el('alpha').value = p.alpha;
    el('allColors').checked = false;
    if (i == 1) {
        s = '<p>Drives (including letters currently absent):</p>';
        for (j = 0; j < 26; j++) s += '<label><input type="checkbox" id="drive' + j + '" ' + (selected(o.drives ==
            null ? 'C' : o.drives, String.fromCharCode(65 + j)) ? 'checked' : '') + '>' + String.fromCharCode(65 +
            j) + ':</label> ';
        s += '<p>Empty media is shown as unavailable. A:/B: are skipped by the default automatic selection.</p><label><input id="autoDrives" type="checkbox" ' +
            (o.drives == '*' ? 'checked' : '') + '> Automatically show connected drives (C:-Z:)</label>';
    }
    if (i == 2) {
        settingsNetworks = networks.slice(0);
        s = '<label><input id="autoNets" type="checkbox" ' + (o.adapters == '*' || o.adapters == null ? 'checked' :
            '') + '> All detected interfaces</label>';
        for (j = 0; j < networks.length; j++) s += '<p><label><input id="net' + j + '" type="checkbox" ' + (selected(o
                .adapters == null ? '*' : o.adapters, networks[j].id) ? 'checked' : '') + '>' + esc(networks[j].name) +
            '</label></p>';
        if (!networks.length) s += '<p>No interfaces currently detected.</p>';
    }
    if (i == 3) s =
        '<p>Style <select id="clockMode"><option value="digital24">Digital 24-hour</option><option value="digital12">Digital 12-hour</option></select></p>';
    if (i == 4) s =
        '<p>Reminders: one line per date, YYYY-MM-DD message. Displayed on that date; no alarm popup.</p><textarea id="reminders">' +
        esc(String(o.reminders || '').replace(/\\n/g, '\n')) + '</textarea>';
    if (i == 9) {
        s = '<p>CD drive <select id="cdDrive"><option value="">Select...</option>';
        for (j = 0; j < drives.length; j++)
            if (drives[j].type == 5) s += '<option value="' + drives[j].id + '">' + drives[j].id + ':</option>';
        s += '</select></p><p>Physical audio CDs require a working MCI CD-audio driver.</p>';
    }
    if (i == 10) {
        for (j = 0; j < 6; j++) s +=
            '<div class="shortcutsetup"><table class="shortcutrow"><tr><td class="shortcutinfo"><label>Label <input id="label' +
            j + '" maxlength="80" value="' + esc(o['label' + j] || '') + '"></label><span id="launchLabel' + j + '">' +
            esc((o['launch' + j] || 'Empty slot ' + (j + 1))) +
            '</span></td><td class="shortcutactions"><button onclick="command(\'choose/' + j +
            '\')">File...</button> <button onclick="command(\'folder/' + j +
            '\')">Folder</button> <button onclick="command(\'clear/' + j +
            '\')">Clear</button></td></tr></table></div>';
        s +=
            '<p class="help">Choose .lnk shortcuts for programs with arguments, or .cpl files for Control Panel items.</p>';
    }
    if (i == 11) s = '<p>RSS/Atom URL <input id="rssUrl" class="wide" value="' + esc(o.rss || '') +
        '"></p><p>Fetch every <input id="rssMinutes" class="num" value="' + esc(o.rssminutes || 15) +
        '"> minutes</p><p>Uses installed WinInet transport. Modern HTTPS feeds may not connect; no certificate checks are disabled.</p>';
    s += moreSettings(i);
    el('specific').innerHTML = s;
    el('specificBox').style.display = s ? 'block' : 'none';
    el('refreshHint').innerText = Number(o.globalSeconds) > 0 ? 'Global override: ' + o.globalSeconds +
        ' seconds (Desktop tab).' : 'Used when the global override is off.';
    if (!inspected && (i == 2 || i == 9)) {
        inspectWaiting = inventoryTime || -1;
        command('device/inspect');
    }
    if (i == 3) el('clockMode').value = String(o.clock || 'digital24').indexOf('12') >= 0 ? 'digital12' : 'digital24';
    if (i == 9) el('cdDrive').value = o.cd || '';
    if (i == 16) el('timerMode').value = o.timerMode || 'countdown';
    if (settingsView) {
        opacityControls();
        managerSelection();
    }
}

function applySettings(action) {
    if (!resolutionReady()) {
        saveNotice('Please wait for the display layout to switch.');
        return;
    }
    if (awaiting || inFlight) {
        el('notice').innerText = 'Please wait for the current save.';
        return;
    }
    var i = num(el('target').value, 0, ids.length - 1),
        p = panels[i],
        j, bg = el('bg').value,
        fg = el('fg').value,
        accent = el('accent').value,
        a = Number(el('alpha').value),
        w = Number(el('width').value),
        seconds = Number(el('seconds').value),
        list = [],
        v;
    if (!/^[\da-fA-F]{6}$/.test(bg) || !/^[\da-fA-F]{6}$/.test(fg) || !/^[\da-fA-F]{6}$/.test(accent) || !isFinite(a) ||
        a < 0 || a > 100 || !isFinite(w) || w < 240 || w > 600 || !isFinite(seconds) || seconds < 1 || seconds > 3600) {
        el('notice').innerText = 'Check colors, width (240-600), opacity (0-100), refresh (1-3600).';
        return;
    }
    if (el('useGlobal').checked && (!isFinite(Number(el('globalSeconds').value)) || Number(el('globalSeconds').value) <
            1 || Number(el('globalSeconds').value) > 3600)) {
        el('notice').innerText = 'Global interval must be 1-3600 seconds.';
        return;
    }
    if ((i == 4 && unescape(encodeURIComponent(el('reminders').value)).length > 900) || (i == 11 && unescape(
            encodeURIComponent(el('rssUrl').value)).length > 900)) {
        el('notice').innerText = 'Please keep this text under 900 UTF-8 bytes.';
        return;
    }
    if (i == 11) {
        v = el('rssUrl').value;
        if (v && !/^https?:\/\//i.test(v)) {
            el('notice').innerText = 'Enter an http:// or https:// feed URL.';
            return;
        }
    }
    if (i == 10)
        for (j = 0; j < 6; j++)
            if (unescape(encodeURIComponent(el('label' + j).value)).length > 80) {
                el('notice').innerText = 'Shortcut labels must be at most 80 bytes.';
                return;
            }
    v = validateMore(i);
    if (v) {
        el('notice').innerText = v;
        return;
    }
    if (!p.on && el('enabled').checked) {
        p.x = 32000;
        p.y = 32000;
    }
    p.on = el('enabled').checked ? 1 : 0;
    p.seconds = Math.round(seconds);
    for (j = 0; j < panels.length; j++)
        if (j == i || el('allColors').checked) {
            panels[j].bg = bg;
            panels[j].fg = fg;
            panels[j].accent = accent;
            panels[j].alpha = Math.round(a);
        } width = Math.round(w);
    locked = el('locked').checked ? 1 : 0;
    snap = el('snap').checked ? 1 : 0;
    wallmode = el('wallmode').selectedIndex;
    if (i == 1) {
        for (j = 0; j < 26; j++)
            if (el('drive' + j).checked) list[list.length] = String.fromCharCode(65 + j);
        queueOption('drives', el('autoDrives').checked ? '*' : list.join(','));
    }
    if (i == 2) {
        var networks = settingsNetworks;
        var previous = String(savedOptions.adapters || '').split(','),
            k, found;
        for (k = 0; k < previous.length; k++)
            if (previous[k] && previous[k] != '*') {
                found = false;
                for (j = 0; j < networks.length; j++)
                    if (networks[j].id == previous[k]) found = true;
                if (!found) list[list.length] = previous[k];
            } for (j = 0; j < networks.length; j++)
            if (el('net' + j).checked) list[list.length] = networks[j].id;
        queueOption('adapters', el('autoNets').checked ? '*' : list.join(','));
    }
    if (i == 3) queueOption('clock', el('clockMode').value);
    if (i == 4) queueOption('reminders', el('reminders').value.substr(0, 900));
    if (i == 9) queueOption('cd', el('cdDrive').value);
    if (i == 11) {
        queueOption('rss', el('rssUrl').value.substr(0, 900));
        queueOption('rssminutes', String(num(el('rssMinutes').value, 1, 1440)));
    }
    if (i == 10)
        for (j = 0; j < 6; j++) {
            v = el('label' + j).value;
            if (v != (savedOptions['label' + j] || '')) queueOption('label' + j, v);
        }
    v = el('useGlobal').checked ? String(num(el('globalSeconds').value, 1, 3600)) : '0';
    if (v != String(savedOptions.globalSeconds || 0)) queueOption('globalSeconds', v);
    v = el('disableAlpha').checked ? '1' : '0';
    if (v != String(savedOptions.disableAlpha || 0)) queueOption('disableAlpha', v);
    saveMore(i);
    drawAll();
    fitWallpaper();
    save(action);
}

function wallError() {
    naturalW = naturalH = 0;
    el('wallpaper').style.display = 'none';
}

function setWallpaper(path) {
    wall = path;
    naturalW = naturalH = 0;
    el('backdrop').style.backgroundImage = 'none';
    var url = path,
        k, parts;
    if (/^[a-zA-Z]:\\/.test(path)) {
        parts = path.replace(/\\/g, '/').split('/');
        for (k = 1; k < parts.length; k++) parts[k] = encodeURIComponent(parts[k]);
        url = 'file:///' + parts.join('/');
    }
    el('wallpaper').style.display = 'none';
    wallLoader = new Image();
    wallLoader.onload = function() {
        if (wall != path) return;
        naturalW = this.width;
        naturalH = this.height;
        el('wallpaper').src = this.src;
        fitWallpaper();
    };
    wallLoader.onerror = wallError;
    wallLoader.src = url;
}

function fitWallpaper() {
    var img = el('wallpaper'),
        back = el('backdrop'),
        b = bounds(),
        w = naturalW,
        h = naturalH,
        r;
    if (!w || !h) return;
    back.style.backgroundImage = 'none';
    img.style.display = wallmode == 1 ? 'none' : 'block';
    if (wallmode == 1) {
        back.style.backgroundImage = 'url("' + img.src.replace(/"/g, '%22') + '")';
        back.style.backgroundRepeat = 'repeat';
        return;
    }
    if (wallmode == 2) {
        w = b[0];
        h = b[1];
    } else if (wallmode == 3) {
        r = Math.min(b[0] / w, b[1] / h);
        w = Math.round(w * r);
        h = Math.round(h * r);
    }
    img.style.pixelWidth = w;
    img.style.pixelHeight = h;
    img.style.pixelLeft = Math.round((b[0] - w) / 2);
    img.style.pixelTop = Math.round((b[1] - h) / 2);
}

function section(label, body) {
    return '<div class="section"><div class="label">' + esc(label) + '</div>' + body + '</div>';
}

function detail(s) {
    return '<div class="detail">' + esc(s).replace(/\n/g, '<br>') + '</div>';
}

function meter(value, i) {
    return '<div class="meter"><div class="track"><div class="fill" style="width:' + num(value, 0, 100) +
        '%;background:#' + meterColor(value, i) + '"></div></div><span class="percent">' + (value >= 0 ? Math.round(
            value) + '%' : 'N/A') + '</span></div>';
}

function button(label, cmd, disabled) {
    return '<a href="#"' + (disabled ? ' disabled="disabled"' : '') + ' onclick="' + (disabled ? '' : 'command(\'' +
        cmd + '\');') + 'return false">' + esc(label) + '</a>';
}

function bytes(v) {
    if (v < 0 || !isFinite(v)) return 'N/A';
    var units = ['B', 'KB', 'MB', 'GB', 'TB'],
        n = 0;
    while (v >= 1024 && n < 4) {
        v /= 1024;
        n++;
    }
    return Math.round(v) + ' ' + units[n];
}

function uptime(s) {
    s = Number(s) || 0;
    return Math.floor(s / 86400) + 'd ' + Math.floor(s / 3600) % 24 + 'h ' + Math.floor(s / 60) % 60 + 'm';
}

function cpuFrequency(f) {
    if (!f || f.length < 16 || !(Number(f[14]) >= 10 && Number(f[14]) <= 20000)) return '';
    var kind = Number(f[15]), cached = kind >= 4 && kind <= 6, text;
    if (cached) kind -= 3;
    text = kind == 1 ? 'Base frequency: ' + f[14] + ' MHz' : kind == 2 ? 'Startup estimate: ~' + f[14] + ' MHz' :
        kind == 3 ? 'TSC reference: ~' + f[14] + ' MHz' : '';
    return text ? text + (cached ? ' (cached)' : '') : '';
}

function core() {
    if (snapshot == null || Math.abs((new Date()).getTime() - sampleTime) > 10000) return null;
    var f = String(snapshot).split('|');
    return (f.length == 14 || f.length == 16) && Number(f[2]) > 0 && Number(f[3]) >= 0 && Number(f[3]) <= Number(f[2]) ?
        f : null;
}

function freshExtra() {
    return Math.abs((new Date()).getTime() - extraTime) < 15000;
}

function networkSelected() {
    var r = [],
        i;
    for (i = 0; i < networks.length; i++)
        if (selected(savedOptions.adapters == null ? '*' : savedOptions.adapters, networks[i].id)) r[r.length] =
            networks[i];
    return r;
}

function calendar() {
    var now = new Date(),
        d = new Date(now.getFullYear(), now.getMonth() + calendarOffset, 1),
        month = d.getMonth(),
        year = d.getFullYear(),
        start = d.getDay(),
        days = new Date(year, month + 1, 0).getDate(),
        s =
        '<div style="text-align:center"><a href="#" onclick="calendarOffset--;lastDraw[4]=0;return false">&lt;</a> ' + [
            'January', 'February', 'March', 'April', 'May', 'June', 'July', 'August', 'September', 'October',
            'November', 'December'
        ][month] + ' ' + year +
        ' <a href="#" onclick="calendarOffset++;lastDraw[4]=0;return false">&gt;</a></div><table class="calendar"><tr>',
        labels = ['Su', 'Mo', 'Tu', 'We', 'Th', 'Fr', 'Sa'],
        j, day;
    for (j = 0; j < 7; j++) s += '<td>' + labels[j] + '</td>';
    s += '</tr>';
    for (j = 0; j < Math.ceil((start + days) / 7) * 7; j++) {
        if (j % 7 == 0) s += '<tr>';
        day = j - start + 1;
        s += '<td' + (day == now.getDate() && month == now.getMonth() && year == now.getFullYear() ? ' class="today"' :
            '') + '>' + (day > 0 && day <= days ? day : '') + '</td>';
        if (j % 7 == 6) s += '</tr>';
    }
    s += '</table><div class="notes" id="notes">';
    var date = now.getFullYear() + '-' + pad(now.getMonth() + 1) + '-' + pad(now.getDate()),
        lines = String(savedOptions.reminders || '').replace(/\\n/g, '\n').split(/\r?\n/);
    for (j = 0; j < lines.length; j++)
        if (lines[j].substr(0, 10) == date) s += detail(lines[j].substr(11));
    return s + '</div>';
}

function pad(n) {
    return n < 10 ? '0' + n : String(n);
}

function diskMax(index) {
    var v = 1,
        j;
    for (j = 0; j < history.length; j++)
        if (history[j][index] > v) v = history[j][index];
    return v;
}
/* Keep graph nodes mounted: IE6 can fault when a live filtered page repeatedly
   discards hundreds of bars while its wallpaper or palette is changing. */
var graphTemplateHtml = '';
var graphSerial = 0,
    graphState = null;

function graphMarkup() {
    if (graphTemplateHtml) return graphTemplateHtml;
    var s = '',
        i, j, names = ['CPU / 0-100%', 'MEMORY / 0-100%', 'NETWORK', 'FILE READ', 'FILE WRITE'];
    for (i = 0; i < 5; i++) {
        s += '<div class="section' + (i ? '' : ' first') + '"><div class="label" id="graphLabel' + i + '">' + names[i] +
            '</div><div class="graph"><div id="graphStrip' + i + '" style="position:absolute;top:0;height:48px">';
        for (j = 0; j < 120; j++) s += '<div id="graphBar' + i + '_' + j +
            '" style="position:absolute;bottom:0;visibility:hidden;font-size:0;line-height:0"></div>';
        s += '</div></div></div>';
    }
    return graphTemplateHtml = s + '<div class="detail" id="graphFileHint"></div>';
}

function graphText(id, text) {
    var e = el(id);
    if (e._text != text) {
        e.innerText = text;
        e._text = text;
    }
}

function graphBar(i, slot, v, maximum, step, barWidth) {
    var mirror, e, height = v < 0 ? 0 : num(v / maximum * 46, 0, 46),
        color = '#' + levelColor(i < 2 ? v : v / maximum * 100, 12, false),
        left;
    for (mirror = 0; mirror < 2; mirror++) {
        e = el('graphBar' + i + '_' + (slot + mirror * 60));
        if (e._shown != (v >= 0)) {
            e.style.visibility = v >= 0 ? 'visible' : 'hidden';
            e._shown = v >= 0;
        }
        if (v < 0) continue;
        left = Math.round((slot + mirror * 60) * step);
        if (e._height != height) {
            e.style.pixelHeight = height;
            e._height = height;
        }
        if (e._left != left) {
            e.style.pixelLeft = left;
            e._left = left;
        }
        if (e._width != barWidth) {
            e.style.pixelWidth = barWidth;
            e._width = barWidth;
        }
        if (e._color != color) {
            e.style.backgroundColor = color;
            e._color = color;
        }
    }
}

function updateGraphs() {
    var maximum = [100, 100, 1, diskMax(3), diskMax(4)],
        names = ['CPU / 0-100%', 'MEMORY / 0-100%', 'NETWORK', 'FILE READ', 'FILE WRITE'],
        i, j, slot, step = (width - 26) / 60,
        n = history.length,
        barWidth = Math.max(1, Math.floor(step) - 1),
        p = panels[12],
        stamp = [width, p.accent, savedOptions.meterColors, savedOptions.meterLow, savedOptions.meterMid, savedOptions
            .meterHigh
        ].join('|'),
        previous = graphState,
        shift = previous && previous.serial + 1 == graphSerial && n == Math.min(60, previous.n + 1),
        cursor = shift ? (previous.n == 60 ? (previous.cursor + 1) % 60 : previous.cursor) : 0,
        full = !shift || previous.stamp != stamp,
        strip, offset;
    for (j = 0; j < n; j++) maximum[2] = Math.max(maximum[2], history[j][2]);
    for (i = 0; i < 5; i++) {
        graphText('graphLabel' + i, names[i] + (i > 1 ? ' / ' + bytes(maximum[i]) + '/s max' : ''));
        if (full || previous.maximum[i] != maximum[i]) {
            for (j = 0; j < 60; j++) {
                slot = (cursor + j) % 60;
                graphBar(i, slot, j < n ? history[j][i] : -1, maximum[i], step, barWidth);
            }
        } else if (n) graphBar(i, (cursor + n - 1) % 60, history[n - 1][i], maximum[i], step, barWidth);
        strip = el('graphStrip' + i);
        offset = n < 60 ? Math.round((60 - n) * step) : -Math.round(cursor * step);
        if (strip._left != offset) {
            strip.style.pixelLeft = offset;
            strip._left = offset;
        }
        if (strip._width != width) {
            strip.style.pixelWidth = Math.ceil(step * 120);
            strip._width = width;
        }
    }
    graphState = {
        serial: graphSerial,
        n: n,
        cursor: cursor,
        maximum: maximum,
        stamp: stamp
    };
    graphText('graphFileHint', diskActivity.read >= 0 ? 'File access includes cache activity' :
        'File activity counters unavailable');
}

function render(i) {
    if (choiceMenu && (i == 22 || i == 24)) return;
    if (i == 12) {
        var graphFlow = el('flow12');
        if (!graphFlow._graphBuilt) {
            graphFlow.innerHTML = graphMarkup();
            graphFlow._graphBuilt = true;
            graphState = null;
        }
        updateGraphs();
        scheduleFit(12);
        return;
    }
    var s = '',
        h = heights[i],
        f = core(),
        ok = freshExtra(),
        j, p, list, used, total, free, v, d, mode, nets, rx = 0,
        tx = 0;
    if (i == 0) {
        s = section('CPU', meter(f ? Number(f[0]) : -1, i) + detail(f ? f[8] : 'CPU unavailable') +
            '<div class="detail" style="height:15px;white-space:nowrap;overflow:hidden">' +
            esc(cpuFrequency(f) || 'CPU frequency unavailable') + '</div>');
        total = f ? Math.round(Number(f[2]) / 1024) : 0;
        free = f ? Math.round(Number(f[3]) / 1024) : 0;
        used = total - free;
        s += section('MEMORY', meter(f ? used / total * 100 : -1, i) + detail(f ? used + ' MB used / ' + total +
            ' MB total\n' + free + ' MB available' : 'Memory unavailable'));
        s += section('DISPLAY', detail(ok ? hardware.gpu : 'Display information unavailable'));
        if (ok) s += detail(hardware.width + ' x ' + hardware.height + (hardware.hz > 1 ? ' @ ' + hardware.hz + ' Hz' :
            '') + ' / ' + hardware.bpp + ' bpp');
        s += section('OS', detail(ok ? hardware.os + '\nUptime ' + uptime(hardware.uptime) :
            'System information unavailable'));
        h = 308;
    }
    if (i == 1) {
        list = [];
        for (j = 0; j < drives.length; j++)
            if (selected(savedOptions.drives == null ? 'C' : savedOptions.drives, drives[j].id) && ((savedOptions
                    .drives || '*') != '*' || drives[j].id >= 'C')) list[list.length] = drives[j];
        if (!ok) s = detail('Drive information unavailable');
        else if (!list.length) s = detail('No selected drives are connected');
        else
            for (j = 0; j < list.length; j++) {
                p = list[j];
                s += section(p.id + ': ' + (p.label || ['Unknown', 'Missing', 'Removable', 'Fixed disk',
                    'Network drive', 'CD-ROM', 'RAM disk'
                ][p.type]), p.ready ? meter((p.total - p.free) / p.total * 100, i) + detail(bytes(p.total - p
                    .free) + ' used / ' + bytes(p.total) + ' total\n' + bytes(p.free) + ' free') : detail(p
                    .type == 4 ? 'Network capacity not queried' : 'No readable media'));
            }
        h = 50 + Math.max(1, list.length) * 89;
    }
    if (i == 2) {
        nets = networkSelected();
        if (!ok) s = detail('Network information unavailable');
        else if (!nets.length) s = detail('No selected adapters are present');
        else
            for (j = 0; j < nets.length; j++) {
                p = nets[j];
                s += section(p.name, detail('Down ' + bytes(p.rx) + '/s / Up ' + bytes(p.tx) + '/s\nReceived ' + bytes(p
                    .totalIn) + ' / Sent ' + bytes(p.totalOut) + '\nIP ' + p.ip + '\nGateway ' + (p
                    .gateway || 'None')));
            }
        s += detail('Host ' + (hostName || 'N/A'));
        h = 70 + Math.max(1, nets.length) * 140;
    }
    if (i == 3) {
        d = new Date();
        mode = savedOptions.clock || 'digital24';
        v = d.getHours();
        s = '<div class="digital clockface">' + (mode.indexOf('12') >= 0 ? (v % 12 || 12) : pad(v)) + ':' + pad(d
                .getMinutes()) + ':' + pad(d.getSeconds()) + (mode.indexOf('12') >= 0 ? (v < 12 ? ' AM' : ' PM') : '') +
            '</div>';
    }
    if (i == 3) s += worldClocks();
    if (i == 4) {
        s = calendar();
    }
    if (i == 6) {
        if (!ok) s = detail('Power status unavailable');
        else if (battery.flags == 128) s = detail('No battery detected');
        else {
            s = meter(battery.percent <= 100 ? battery.percent : -1, i) + detail((battery.ac == 1 ? 'AC power' : battery
                .ac == 0 ? 'On battery' : 'Power source unknown') + (battery.flags != 255 && (battery.flags &
                8) ? ' / Charging' : '') + (battery.ac == 0 && battery.seconds < 4294967295 ? '\n' + Math.round(
                battery.seconds / 60) + ' min remaining' : ''));
        }
        h = 105;
    }
    if (i == 7) {
        for (j = 0; j < 4; j++) {
            p = mixer[j] || {
                volume: -1,
                mute: -1
            };
            s += section(['MASTER', 'WAVE', 'CD', 'MIDI'][j], p.volume < 0 ? detail('Control unavailable') : meter(p
                .volume, i) + '<div class="buttons">' + button('-', 'device/volume/' + j + '/0/' + Math.max(0, p
                .volume - 10)) + button('+', 'device/volume/' + j + '/0/' + Math.min(100, p.volume + 10)) + (p
                .mute >= 0 ? button(p.mute ? 'Unmute' : 'Mute', 'device/volume/' + j + '/1/' + (p.mute ? 0 :
                    1)) : '') + '</div>');
        }
        h = 375;
    }
    if (i == 8) {
        var available = ok && winamp.present,
            volume = available ? winamp.volume : -1;
        s = '<div class="detail" style="height:30px;overflow:auto" title="' + esc(available ? winamp.title || 'Winamp' :
            ok ? 'Winamp not running' : 'Information unavailable') + '">' + esc(available ? winamp.title ||
            'Winamp' : ok ? 'Winamp not running' : 'Information unavailable') + '</div>' + detail(available ? (
                winamp.state == 1 ? 'Playing' : winamp.state == 3 ? 'Paused' : 'Stopped') :
            'Start Winamp to use controls') + '<div class="buttons">' + button('Prev', 'device/winamp/prev', !
            available) + button('Play', 'device/winamp/play', !available) + button('Pause', 'device/winamp/pause', !
            available) + button('Next', 'device/winamp/next', !available) + button('Stop', 'device/winamp/stop', !
            available) + '</div>' + section('VOLUME', meter(volume, i) + '<div class="buttons">' + button('-',
            'device/winamp/volume/' + Math.max(0, volume - 10), !available) + button('+',
            'device/winamp/volume/' + Math.min(100, volume + 10), !available) + '</div>');
    }
    if (i == 9) {
        s = cdDescription() + detail((cdState || 'Audio CD unavailable') + (cdTrack > 0 ? ' / Track ' + cdTrack + (
            cdTracks > 0 ? ' of ' +
            cdTracks : '') : '')) + '<div class="buttons">';
        if (savedOptions.cd) s += button('Open ' + savedOptions.cd + ':', 'device/cd/open/' + savedOptions.cd);
        s += button('Play', 'device/cd/play') + button('Pause', 'device/cd/pause') +
            button('Prev', 'device/cd/prev') + button('Next', 'device/cd/next') +
            button('Stop', 'device/cd/stop') + button('Eject', 'device/cd/eject') + cdEditButton() + '</div>' + detail(
                actionStatus ||
                '');
        h = 145;
    }
    if (i == 10) {
        for (j = 0; j < 6; j++)
            if (savedOptions['launch' + j]) {
                p = String(savedOptions['launch' + j]).replace(/\\$/, '').split('\\');
                s += '<div class="launchrow">' + button(savedOptions['label' + j] || p[p.length - 1] || savedOptions[
                    'launch' + j], 'launch/' + j) + (savedOptions['label' + j] ? '<span class="launchfile">' + esc(
                    p[p.length - 1]) + '</span>' : '') + '</div>';
            } if (!s) s = detail('Choose shortcuts in Manage widgets.');
        h = 70 + Math.max(1, s.split('<p>').length - 1) * 30;
    }
    if (i == 11) {
        if (rssItems.length) {
            j = Math.floor((new Date()).getTime() / 8000) % rssItems.length;
            s = detail(rssItems[j]) + detail((j + 1) + ' / ' + rssItems.length);
        } else s = detail(rssStatus);
        s += '<div class="buttons">' + button('Refresh feed', 'rss') + '</div>';
        h = 150;
    }
    if (i == 12) s = graphMarkup();
    if (i >= 13) s = renderMore(i, ok);
    if (!ok && (i == 7 || i == 9)) s = detail('Information unavailable');
    s = themeMarkup(s.replace('class="section"', 'class="section first"'), i);
    if (el('flow' + i)._content != s) {
        el('flow' + i).innerHTML = s;
        el('flow' + i)._content = s;
    }
    if (i == 12) updateGraphs();
    if (i == 16) updateTimer();
    if (i == 19 && el('photoImage') && el('photoImage')._naturalW) photoFit(el('photoImage'));
    scheduleFit(i);
}

function tick() {
    var now = (new Date()).getTime(),
        i, f, nets, total = 0,
        v, stamp = '',
        extraChanged = extraTime != lastExtra;
    lastExtra = extraTime;
    if (pendingDevice) {
        if (extraTime > pendingDevice.at || now - pendingDevice.at > 2500) pendingDevice = null;
        else applyOptimistic();
    }
    if (inspectWaiting && inventoryTime && inventoryTime != inspectWaiting) {
        inspectWaiting = 0;
        if (settingsView) loadWidget(true);
    }
    if (inFlight && actionAck == inFlight && ackTime != sentAckTime) {
        if (!ackOK) {
            dailyPending = null;
            saveNotice('Could not save settings. Check the installation folder.');
            optionQueue = [];
            awaiting = '';
        }
        inFlight = '';
    }
    if (inFlight && now - sentAt > 15000) {
        dailyPending = null;
        saveNotice('Save not confirmed. Retry Apply.');
        inFlight = '';
        optionQueue = [];
        awaiting = '';
    }
    if (!inFlight && optionQueue.length) {
        inFlight = optionQueue.shift();
        sentAckTime = ackTime;
        sentAt = now;
        command(inFlight);
    }
    if (awaiting && !optionQueue.length && !inFlight) {
        awaiting = '';
        applied = '';
        saveNotice('Settings saved.');
    }

    displayWatch(now);
    if (!awaiting && savedLayout && savedLayout != applied && dragging < 0 && parse(savedLayout)) {
        applied = savedLayout;
        drawAll();
        lastDraw = [];
        fitWallpaper();
        if (settingsView) loadWidget();
    }
    for (v in savedOptions)
        if (typeof savedOptions[v] != 'function') stamp += v + '=' + savedOptions[v] + '|';
    if (stamp != optionsStamp) {
        optionsStamp = stamp;
        lastDraw = [];
        if (settingsView) el('refreshHint').innerText = Number(savedOptions.globalSeconds) > 0 ? 'Global override: ' +
            savedOptions.globalSeconds + ' seconds (Desktop tab).' : 'Used when the global override is off.';
    }
    if (!settingsView && wall != savedWallpaper) setWallpaper(savedWallpaper);
    if (settingsView && Number(el('target').value) == 10)
        for (i = 0; i < 6; i++)
            if (el('launchLabel' + i)) el('launchLabel' + i).innerText = savedOptions['launch' + i] || 'Empty slot ' + (
                i + 1);

    syncRenderingMode();
    themeTick();
    tickMore(now);
    if (settingsView && !awaiting && !inFlight && placementNotice != placementSeen) {
        placementSeen = placementNotice;
        if (placementNotice) el('notice').innerText = placementNotice;
    }
    if (!settingsView) {
        for (i = 0; i < panels.length; i++)
            if (panels[i].on && dragging != i && (!lastDraw[i] || now - lastDraw[i] >= effectiveSeconds(i) * 1000 || (
                    extraChanged && (i == 1 || i == 22 || (now < fastUntil && (i == 7 || i == 8 || i == 9)))))) {
                if (i == 12) {
                    f = core();
                    nets = networkSelected();
                    for (v = 0; v < nets.length; v++)
                        if (nets[v].rx >= 0 && nets[v].tx >= 0) total += nets[v].rx + nets[v].tx;
                    history[history.length] = [f ? Number(f[0]) : -1, f ? (Number(f[2]) - Number(f[3])) / Number(f[2]) *
                        100 : -1, freshExtra() ? total : -1, freshExtra() ? diskActivity.read : -1, freshExtra() ?
                        diskActivity.write : -1
                    ];
                    if (history.length > 60) history.shift();
                    graphSerial++;
                }
                render(i);
                lastDraw[i] = now;
            }
        finishPlacement(now);
        if (lastRssURL != (savedOptions.rss || '')) {
            lastRssURL = savedOptions.rss || '';
            lastRss = 0;
            rssItems = [];
            rssStatus = lastRssURL ? 'Waiting for feed' : 'No feed configured';
        }
        if (panels[11].on && savedOptions.rss && now - lastRss > num(savedOptions.rssminutes || 15, 1, 1440) * 60000) {
            lastRss = now;
            command('rss');
        }
    }
}
var i, html = '',
    options = '';
if (!parse(savedLayout)) parse(defaults());
applied = savedLayout;
for (i = 0; i < ids.length; i++) {
    if ((i == 5 || i == 21)) continue;
    html += '<div class="widget" id="w' + i + '"><div class="tint" id="t' + i + '"></div><div class="content" id="c' +
        i + '"><a href="#" class="handle" id="h' + i + '" onmousedown="beginDrag(' + i +
        ');return false" onclick="return false">' + titles[i] + '</a><div class="inside" id="b' + i +
        '"><div class="bodyflow" id="flow' + i + '"></div></div></div></div>';
    options += '<option value="' + i + '">' + titles[i] + '</option>';
}
el('widgets').innerHTML = html;
for (i = 0; i < titles.length; i++)
    if ((i != 5 && i != 21)) el('target').options.add(new Option(titles[i], String(i)));
drawAll();
if (settingsView) {
    document.title = 'Glass98 - Settings';
    document.body.style.overflow = 'hidden';
    document.body.scroll = 'no';
    el('toolbar').style.display = 'none';
    el('backdrop').style.display = 'none';
    el('manager').style.display = 'block';
    el('width').value = width;
    el('snap').checked = snap;
    el('locked').checked = locked;
    el('wallmode').selectedIndex = wallmode;
    el('disableAlpha').checked = savedOptions.disableAlpha == '1';
    el('useGlobal').checked = Number(savedOptions.globalSeconds) > 0;
    el('globalSeconds').value = savedOptions.globalSeconds && Number(savedOptions.globalSeconds) > 0 ? savedOptions
        .globalSeconds : 2;
    showTab('widget');
    var selectedWidget = /[?&]widget=(\d+)/.exec(location.search);
    if (selectedWidget && Number(selectedWidget[1]) != 5 && Number(selectedWidget[1]) != 21) el('target').value =
        String(num(selectedWidget[1], 0, ids.length - 1));
    loadWidget();
    managerInit();
} else setWallpaper(savedWallpaper);
tick();
window.setInterval(function() {
    var v = (new Date()).getTime();
    if (!settingsView && (panels[0].on || panels[12].on)) el('feed').src = 'DATA.JS?v=' + v;
    var needExtra = settingsView || panels[0].on || panels[1].on || panels[2].on || panels[6].on || panels[7]
        .on || panels[8].on || panels[9].on || panels[12].on || panels[13].on || panels[14].on || panels[17]
        .on || panels[18].on || panels[22].on || panels[23].on || panels[27].on;
    if (needExtra) el('extra').src = 'EXTRA.JS?v=' + v;
    el('prefs').src = 'PREFS.JS?v=' + v;
    if (panels[13].on) el('pingfeed').src = 'PING.JS?v=' + v;
    if (panels[11].on) el('rssfeed').src = 'RSS.JS?v=' + v;
    if (inFlight || optionQueue.length) el('ackfeed').src = 'ACK.JS?v=' + v;
    window.setTimeout(tick, 200);
}, 1000);
