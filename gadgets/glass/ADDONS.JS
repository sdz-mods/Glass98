/* Desktop utilities: IE6 JScript, no ActiveX and no modern browser APIs. */
var resources = {
        system: -1,
        user: -1,
        gdi: -1
    },
    diskActivity = {
        read: -1,
        write: -1
    },
    recentFiles = [],
    mediaDrives = [],
    pingResult = {},
    cdTrack = 0,
    cdTracks = 0;
var timerLocal = '',
    timerAck = '',
    photoIndex = 0,
    photoAt = 0,
    photoSignature = '',
    lastVu = 0,
    lastPing = 0;

function lines(s) {
    return String(s || '').replace(/\\n/g, '\n').split(/\r?\n/);
}

function textOption(id, value, multiline) {
    return multiline ? '<textarea id="' + id + '">' + esc(lines(value).join('\n')) + '</textarea>' :
        '<input class="wide" id="' + id + '" value="' + esc(value || '') + '">';
}

function moreSettings(i) {
    var o = savedOptions,
        s = '',
        j;
    if (i == 3) s = '<p>World clocks: one per line, Label|UTC offset. Up to six. Example: Tokyo|+09:00</p>' +
        textOption('worlds', o.worlds, true) +
        '<p class="help">Fixed UTC offsets; adjust these for daylight saving time. The main clock uses Windows local time.</p>';
    if (i == 13) s = '<p>Hostname or IPv4 address</p>' + textOption('pingHost', o.pingHost, false) +
        '<p class="help">Checks run only when clicked. A missing ICMP reply does not prove a host is offline.</p>';
    if (i == 14) s =
        '<p>Percentage of free USER/GDI resources, separate from physical RAM. Lower values mean less space remains for windows and graphics objects.</p>';
    if (i == 15) s = '<p>Notes (up to 900 bytes). Start a line with [ ] or [x] for a clickable checkbox.</p>' +
        textOption('stickyText', o.notes, true);
    if (i == 16) s =
        '<p>Mode <select id="timerMode"><option value="countdown">Countdown</option><option value="stopwatch">Stopwatch</option></select></p><p>Countdown duration <input class="num" id="timerMinutes" value="' +
        esc(o.timerMinutes || 5) + '"> minutes</p><label><input id="timerSound" type="checkbox" ' + (o.timerSound ==
            '0' ? '' : 'checked') +
        '> Sound when the countdown ends</label><p class="help">Reset uses these settings. Timers survive desktop reloads and continue across shutdown while running. Alarms sound only while the widget is visible.</p>';
    if (i == 17) {
        for (j = 0; j < 4; j++) s += '<div class="shortcutsetup"><span id="favLabel' + j + '">' + esc(o['fav' + j] ||
                'Empty favorite ' + (j + 1)) + '</span><button onclick="command(\'util/favchoose/' + j +
            '\')">Choose folder</button> <button onclick="command(\'util/favclear/' + j + '\')">Clear</button></div>';
        s += '<p class="help">The eight newest Windows Recent Documents shortcuts appear below favorites.</p>';
    }
    if (i == 18) s =
        '<p>Shows connected removable drives and CD drives from C: onward. Floppy drives are not polled. Open works for readable media; Eject is available for CD drives.</p>';
    if (i == 19) {
        for (j = 0; j < 6; j++) s += '<div class="shortcutsetup"><span id="photoLabel' + j + '">' + esc(o['photo' +
                j] || 'Empty picture ' + (j + 1)) + '</span><button onclick="command(\'util/photo/' + j +
            '\')">Choose image</button> <button onclick="command(\'util/photoclear/' + j + '\')">Clear</button></div>';
        s += '<p>Slideshow every <input class="num" id="photoSeconds" value="' + esc(o.photoSeconds || 30) +
            '"> seconds (0 = manual)</p>';
    }
    if (i == 20) s = '<p>Event label</p>' + textOption('eventName', o.eventName, false) + '<p>Date (YYYY-MM-DD)</p>' +
        textOption('eventDate', o.eventDate, false);
    return s + utilitySettings(i);
}

function validDate(s) {
    var m = /^(\d{4})-(\d{2})-(\d{2})$/.exec(s),
        d;
    if (!m) return null;
    d = new Date(Number(m[1]), Number(m[2]) - 1, Number(m[3]));
    return d.getFullYear() == Number(m[1]) && d.getMonth() == Number(m[2]) - 1 && d.getDate() == Number(m[3]) ? d :
        null;
}

function worldList(s) {
    var a = lines(s),
        r = [],
        j, m, offset;
    for (j = 0; j < a.length; j++) {
        if (!a[j]) continue;
        m = /^([^|]{1,40})\|([+-])(\d{2}):(\d{2})$/.exec(a[j]);
        if (!m || Number(m[4]) > 59) return null;
        offset = Number(m[3]) * 60 + Number(m[4]);
        if (offset > 840) return null;
        r[r.length] = [m[1], offset * (m[2] == '-' ? -1 : 1)];
    }
    return r.length <= 6 ? r : null;
}

function validateMore(i) {
    var v;
    if (i == 3 && !worldList(el('worlds').value))
        return 'Use Label|+HH:MM or Label|-HH:MM, up to six clocks (maximum UTC offset 14 hours).';
    if (i == 13 && !/^[a-zA-Z0-9.-]{0,120}$/.test(el('pingHost').value))
        return 'Enter a hostname or IPv4 address without a URL prefix.';
    if (i == 16) {
        v = Number(el('timerMinutes').value);
        if (!isFinite(v) || v < 0.02 || v > 10080) return 'Countdown duration must be 0.02 to 10080 minutes.';
    }
    if (i == 19) {
        v = Number(el('photoSeconds').value);
        if (!isFinite(v) || v < 0 || v > 3600) return 'Slideshow interval must be 0 to 3600 seconds.';
    }
    if (i == 20 && el('eventDate').value && !validDate(el('eventDate').value))
        return 'Enter a valid date as YYYY-MM-DD.';
    var ids = i == 3 ? ['worlds'] : i == 15 ? ['stickyText'] : i == 20 ? ['eventName'] : [];
    for (var j = 0; j < ids.length; j++)
        if (unescape(encodeURIComponent(el(ids[j]).value)).length > 900)
            return 'Please keep this text under 900 UTF-8 bytes.';
    return utilityValidate(i);
}

function saveMore(i) {
    utilitySave(i);
    if (i == 3) queueOption('worlds', el('worlds').value);
    if (i == 13) queueOption('pingHost', el('pingHost').value);
    if (i == 15) queueOption('notes', el('stickyText').value);
    if (i == 16) {
        queueOption('timerMode', el('timerMode').value);
        queueOption('timerMinutes', el('timerMinutes').value);
        queueOption('timerSound', el('timerSound').checked ? '1' : '0');
    }
    if (i == 19) queueOption('photoSeconds', String(num(el('photoSeconds').value, 0, 3600)));
    if (i == 20) {
        queueOption('eventName', el('eventName').value);
        queueOption('eventDate', el('eventDate').value);
    }
}

function localButton(label, code) {
    return '<a href="#" onclick="' + code + ';return false">' + esc(label) + '</a>';
}

function editLink(i) {
    return localButton('Edit', 'openManager(' + i + ')');
}

function queueLive(key, value) {
    queueOption(key, value);
    savedOptions[key] = value;
    awaiting = 'pending';
    waitSince = (new Date()).getTime();
    el('notice').innerText = 'Saving...';
    lastDraw = [];
}

function noteToggle(index) {
    if (awaiting || inFlight) return;
    var a = lines(savedOptions.notes);
    if (!a[index]) return;
    a[index] = a[index].replace(/^\[([ xX])\]/, function(all, x) {
        return x == ' ' ? '[x]' : '[ ]';
    });
    queueLive('notes', a.join('\n'));
    window.setTimeout(function() {
        render(15);
    }, 25);
}

function clockText(d, utc) {
    var h = utc ? d.getUTCHours() : d.getHours(),
        m = utc ? d.getUTCMinutes() : d.getMinutes(),
        s = utc ? d.getUTCSeconds() : d.getSeconds(),
        t = String(savedOptions.clock || '').indexOf('12') >= 0;
    return (t ? (h % 12 || 12) : pad(h)) + ':' + pad(m) + ':' + pad(s) + (t ? (h < 12 ? ' AM' : ' PM') : '');
}

function worldClocks() {
    var a = worldList(savedOptions.worlds) || [],
        s = '',
        now = (new Date()).getTime(),
        j, d;
    for (j = 0; j < a.length; j++) {
        d = new Date(now + a[j][1] * 60000);
        s += section(a[j][0], detail(clockText(d, true) + ' / ' + d.getUTCFullYear() + '-' + pad(d.getUTCMonth() + 1) +
            '-' + pad(d.getUTCDate())));
    }
    return s;
}

function timerState() {
    var s = timerLocal || savedOptions.timerState || '',
        a = String(s).split('|');
    if (a.length == 4 && (a[0] == 'c' || a[0] == 's')) a = [a[0] == 'c' ? 'countdown' : 'stopwatch', {
        i: 'idle',
        r: 'run',
        p: 'pause',
        d: 'done'
    } [a[1]] || '', parseInt(a[2], 36), parseInt(a[3], 36)];
    if (a.length != 4 || !/^(countdown|stopwatch)$/.test(a[0]) || !/^(idle|run|pause|done)$/.test(a[1]) || !isFinite(
            Number(a[2])) || !isFinite(Number(a[3]))) a = [savedOptions.timerMode == 'stopwatch' ? 'stopwatch' :
        'countdown', 'idle', savedOptions.timerMode == 'stopwatch' ? 0 : num(Number(savedOptions.timerMinutes ||
            5) * 60000, 1000, 604800000), 0
    ];
    return [a[0], a[1], Number(a[2]), Number(a[3])];
}

function timerValue(a, now) {
    return Math.max(0, a[2] + (a[1] == 'run' ? (a[0] == 'stopwatch' ? 1 : -1) * Math.max(0, now - a[3]) : 0));
}

function timerSet(a) {
    timerLocal = [a[0] == 'countdown' ? 'c' : 's', {
        idle: 'i',
        run: 'r',
        pause: 'p',
        done: 'd'
    } [a[1]], Math.round(a[2]).toString(36), Math.round(a[3]).toString(36)].join('|');
    timerAck = timerLocal;
    queueLive('timerState', timerLocal);
    window.setTimeout(function() {
        render(16);
    }, 25);
}

function timerAction(action) {
    var now = (new Date()).getTime(),
        a = timerState();
    if (action == 'reset') {
        a = [savedOptions.timerMode == 'stopwatch' ? 'stopwatch' : 'countdown', 'idle', savedOptions.timerMode ==
            'stopwatch' ? 0 : num(Number(savedOptions.timerMinutes || 5) * 60000, 1000, 604800000), 0
        ];
    } else if (a[1] == 'run') {
        a[2] = timerValue(a, now);
        a[1] = 'pause';
        a[3] = 0;
    } else {
        if (a[1] == 'done') a[2] = num(Number(savedOptions.timerMinutes || 5) * 60000, 1000, 604800000);
        a[1] = 'run';
        a[3] = now;
    }
    timerSet(a);
}

function duration(ms) {
    var n = Math.ceil(ms / 1000);
    return pad(Math.floor(n / 3600)) + ':' + pad(Math.floor(n / 60) % 60) + ':' + pad(n % 60);
}

function pictureList() {
    var a = [],
        j;
    for (j = 0; j < 6; j++)
        if (savedOptions['photo' + j]) a[a.length] = savedOptions['photo' + j];
    return a;
}

function fileURL(path) {
    var a = String(path).replace(/\\/g, '/').split('/'),
        j;
    if (a[0] == '') {
        for (j = 2; j < a.length; j++) a[j] = encodeURIComponent(a[j]);
        return 'file:' + a.join('/');
    }
    for (j = 1; j < a.length; j++) a[j] = encodeURIComponent(a[j]);
    return 'file:///' + a.join('/');
}

function nextPhoto(delta) {
    var a = pictureList();
    if (a.length) photoIndex = (photoIndex + delta + a.length) % a.length;
    photoAt = (new Date()).getTime();
    window.setTimeout(function() {
        render(19);
    }, 25);
}

function photoFit(img) {
    if (!img._naturalW) {
        img._naturalW = img.width;
        img._naturalH = img.height;
    }
    if (desktopPaused || !img._naturalW || !img._naturalH) return;
    var r = Math.min((width - 26) / img._naturalW, 180 / img._naturalH, 1);
    img.style.width = Math.max(1, Math.round(img._naturalW * r)) + 'px';
    img.style.height = Math.max(1, Math.round(img._naturalH * r)) + 'px';
    img.style.left = Math.round((width - 26 - img._naturalW * r) / 2) + 'px';
    img.style.top = Math.round((180 - img._naturalH * r) / 2) + 'px';
    img.style.visibility = 'visible';
    scheduleFit(19);
}

function photoError(img) {
    img.style.display = 'none';
    if (el('photoError')) el('photoError').innerText = 'Image unavailable';
    scheduleFit(19);
}

function renderMore(i, ok) {
    if (i >= 22) return utilityRender(i, ok);
    var s = '',
        j, a, p, d, now = new Date(),
        v, rows;
    if (i == 13) {
        s = section('HOST', detail(savedOptions.pingHost || 'Choose a host in widget options') +
            '<div class="buttons">' + button('Ping host', 'util/ping') + button('Network settings',
                'util/network') + '</div>');
        if (ok) {
            for (j = 0; j < networks.length; j++) {
                p = networks[j];
                if (p.gateway && p.gateway != '0.0.0.0' && /^\d+\.\d+\.\d+\.\d+$/.test(p.gateway)) s += section(p.name,
                    detail('Gateway ' + p.gateway) + '<div class="buttons">' + button('Ping gateway',
                        'util/pingip/' + p.gateway) + '</div>');
            }
        }
        if (pingResult.time) s += section('LAST CHECK', detail(pingResult.target + '\n' + pingResult.message + (
                pingResult.ms >= 0 ? ' / ' + pingResult.ms + ' ms' : '') + '\n' + new Date(pingResult.time)
            .toLocaleString()));
    }
    if (i == 14) {
        s = section('SYSTEM FREE', meter(ok ? resources.system : -1, i)) + section('USER FREE', meter(ok ? resources
            .user : -1, i)) + section('GDI FREE', meter(ok ? resources.gdi : -1, i));
        if (!ok || resources.system < 0) s += detail('Resource helper unavailable');
        else if (resources.system < 15) s += detail('Resources are low; consider closing unused applications.');
    }
    if (i == 15) {
        a = lines(savedOptions.notes);
        s = '<div id="stickyBody" style="overflow:auto;word-wrap:break-word">';
        for (j = 0; j < a.length; j++) {
            p = /^\[([ xX])\]\s?(.*)$/.exec(a[j]);
            s += p ? '<div class="detail">' + localButton(p[1] == ' ' ? '[ ]' : '[x]', 'noteToggle(' + j + ')') + ' ' +
                esc(p[2]) + '</div>' : detail(a[j] || ' ');
        }
        s += '</div><div class="buttons">' + editLink(i) + '</div>';
    }
    if (i == 16) {
        s = '<div class="section"><div class="label" id="timerLabel"></div><div class="digital" id="timerDigits"></div><div class="detail" id="timerStatus"></div></div><div class="buttons"><a id="timerToggle" href="#" onclick="timerAction(\'toggle\');return false">Start</a>' +
            localButton('Reset', "timerAction('reset')") + '</div>';
    }

    if (i == 17) {
        a = '';
        for (j = 0; j < 4; j++)
            if (savedOptions['fav' + j]) a += '<div class="launchrow">' + button(savedOptions['fav' + j], 'util/fav/' +
                j) + '</div>';
        if (a) s = section('FAVORITES', a);
        a = '';
        if (ok)
            for (j = 0; j < recentFiles.length; j++) a += '<div class="launchrow">' + button(recentFiles[j].name,
                'util/recent/' + recentFiles[j].id) + '</div>';
        s += section('RECENT DOCUMENTS', a || detail(ok ? 'No recent documents' : 'Recent documents unavailable'));
    }
    if (i == 18) {
        if (ok)
            for (j = 0; j < mediaDrives.length; j++) {
                p = mediaDrives[j];
                s += section(p.id + ': ' + (p.label || (p.type == 5 ? 'CD-ROM' : 'Removable')), detail(p.ready ?
                    'Media ready' : 'No readable data media') + '<div class="buttons">' + (p.ready ? button(
                    'Open', 'util/mediaopen/' + p.id) : '') + (p.type == 5 ? button('Eject', 'util/eject/' + p
                    .id) : '') + '</div>');
            }
        if (!s) s = detail(ok ? 'No removable drives connected' : 'Media information unavailable');
    }
    if (i == 19) {
        a = pictureList();
        if (!a.length) s = detail('Choose images in widget options');
        else {
            photoIndex %= a.length;
            p = a[photoIndex];
            s = '<div style="position:relative;height:180px;overflow:hidden;text-align:center"><img id="photoImage" style="position:absolute;visibility:hidden" alt="" src="' +
                esc(fileURL(p)) +
                '" onload="photoFit(this)" onerror="photoError(this)"><div class="detail" id="photoError"></div></div><div class="buttons">' +
                localButton('Previous', 'nextPhoto(-1)') + localButton('Next', 'nextPhoto(1)') + ' ' + (photoIndex +
                    1) + ' / ' + a.length + '</div>';
        }
    }
    if (i == 20) {
        d = validDate(savedOptions.eventDate || '');
        if (!d) s = detail('Choose an event and date in widget options');
        else {
            v = Math.round((Date.UTC(d.getFullYear(), d.getMonth(), d.getDate()) - Date.UTC(now.getFullYear(), now
                .getMonth(), now.getDate())) / 86400000);
            s = section(savedOptions.eventName || 'EVENT', '<div class="digital">' + (v == 0 ? 'Today' : Math.abs(v) +
                ' days') + '</div>' + (v < 0 ? detail('since the event') : '') + detail(savedOptions.eventDate));
        }
    }
    return s;
}

function tickMore(now) {
    utilityTick(now);
    var a, j, s;
    if (timerLocal && savedOptions.timerState == timerAck && !awaiting && !inFlight) {
        timerLocal = '';
        timerAck = '';
    }
    if (settingsView) {
        for (j = 0; j < 6; j++)
            if (el('photoLabel' + j)) el('photoLabel' + j).innerText = savedOptions['photo' + j] || 'Empty picture ' + (
                j + 1);
        for (j = 0; j < 4; j++)
            if (el('favLabel' + j)) el('favLabel' + j).innerText = savedOptions['fav' + j] || 'Empty favorite ' + (j +
                1);
        return;
    }
    if (panels[16].on) {
        a = timerState();
        if (a[0] == 'countdown' && a[1] == 'run' && timerValue(a, now) <= 0 && !awaiting && !inFlight) {
            a[1] = 'done';
            a[2] = 0;
            a[3] = 0;
            timerSet(a);
            if (savedOptions.timerSound != '0') command('util/beep');
        }
    }
    if (panels[19].on) {
        a = pictureList();
        s = a.join('|');
        if (s != photoSignature) {
            photoSignature = s;
            photoIndex = 0;
            photoAt = now;
            lastDraw[19] = 0;
        }
        if (!photoAt) photoAt = now;
        if (a.length > 1 && Number(savedOptions.photoSeconds || 30) > 0 && now - photoAt >= Number(savedOptions
                .photoSeconds || 30) * 1000) nextPhoto(1);
    }
    if (panels[13].on && pingResult.time != lastPing) {
        lastPing = pingResult.time;
        render(13);
    }
}

function updateTimer() {
    var a = timerState(),
        now = (new Date()).getTime();
    if (!el('timerDigits')) return;
    el('timerLabel').innerText = a[0] == 'stopwatch' ? 'STOPWATCH' : 'COUNTDOWN';
    el('timerDigits').innerText = duration(timerValue(a, now));
    el('timerStatus').innerText = a[1] == 'done' ? 'Time is up' : a[1] == 'pause' ? 'Paused' : a[1] == 'run' ?
        'Running' : 'Ready';
    el('timerToggle').innerText = a[1] == 'run' ? 'Pause' : a[1] == 'pause' ? 'Resume' : 'Start';
}
