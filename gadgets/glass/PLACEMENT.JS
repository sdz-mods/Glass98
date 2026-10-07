/* Only newly enabled panels carry the (32000,32000) placement request.
   Placement runs on the desktop after rendering, never from guessed manager heights. */
var pendingPlacement = [],
    placementNotice = '',
    placementSeen = '';

function newWidgetPosition(index) {
    var b = bounds(),
        right = Math.max(1, b[0] - width - 2),
        top = 1,
        gap = 10,
        x, y, j, p, anchor = -1,
        near, columns = [],
        occupied = [];
    for (j = 0; j < panels.length; j++)
        if (j != index && panels[j].on && !pendingPlacement[j]) {
            p = panels[j];
            occupied[occupied.length] = j;
            anchor = Math.max(anchor, Math.min(right, p.x));
        }
    if (anchor < 0) anchor = right;
    for (x = anchor; x >= 1; x -= width + gap) columns[columns.length] = x;
    for (j = 0; j < occupied.length; j++)
        if (panels[occupied[j]].x <= anchor) columns[columns.length] = panels[occupied[j]].x;
    columns.sort(function(a, b) {
        return b - a;
    });
    for (var c = 0; c < columns.length; c++) {
        x = columns[c];
        if (x < 1 || x > right) continue;
        top = 1;
        near = 32001;
        /* Start a new column level with the topmost panel in its nearest right column. */
        for (j = 0; j < occupied.length; j++) {
            p = panels[occupied[j]];
            if (p.x > x && p.x < near) {
                near = p.x;
                top = p.y;
            } else if (p.x == near) top = Math.min(top, p.y);
        }
        y = Math.max(1, top);
        for (j = 0; j < occupied.length; j++) {
            p = panels[occupied[j]];
            if (x < p.x + width + gap && x + width + gap > p.x) y = Math.max(y, p.y + heights[occupied[j]] + gap);
        }
        if (y + heights[index] <= b[1] - 3) return {
            x: x,
            y: y,
            crowded: 0
        };
        /* If the column bottom is full, reuse holes left by disabled widgets. */
        var gaps = [Math.max(1, top)],
            g, clear;
        for (j = 0; j < occupied.length; j++) {
            p = panels[occupied[j]];
            if (x < p.x + width + gap && x + width + gap > p.x) gaps[gaps.length] = p.y + heights[occupied[j]] + gap;
        }
        gaps.sort(function(a, b) {
            return a - b;
        });
        for (g = 0; g < gaps.length; g++) {
            y = gaps[g];
            if (y + heights[index] > b[1] - 3) continue;
            clear = true;
            for (j = 0; j < occupied.length; j++) {
                p = panels[occupied[j]];
                if (x < p.x + width + gap && x + width + gap > p.x && y < p.y + heights[occupied[j]] + gap && y +
                    heights[index] + gap > p.y) {
                    clear = false;
                    break;
                }
            }
            if (clear) return {
                x: x,
                y: y,
                crowded: 0
            };
        }
    }
    /* No room: flag the new panel as crowded so the caller hides it.
       Preserve the existing layout and report the lack of space to the manager. */
    return {
        x: right,
        y: Math.min(Math.max(1, b[1] - heights[index] - 3), top + (occupied.length % 6) * 24),
        crowded: 1
    };
}

function markPlacement(i) {
    var p = panels[i];
    if (p.on && p.x == 32000 && p.y == 32000 && !pendingPlacement[i]) pendingPlacement[i] = {
        at: (new Date()).getTime(),
        sent: 0
    };
}

function finishPlacement(now) {
    if (settingsView || displayBusy || dragging >= 0 || inFlight || optionQueue.length) return;
    var i, p, spot;
    for (i = 0; i < panels.length; i++) {
        p = pendingPlacement[i];
        if (!p) continue;
        if (!panels[i].on) {
            pendingPlacement[i] = null;
            continue;
        }
        if (!measured[i] || now - p.at < 2500) return;
        if (p.sent && now - p.sent < 5000) return;
        spot = newWidgetPosition(i);
        panels[i].x = spot.x;
        panels[i].y = spot.y;
        el('w' + i).style.pixelLeft = spot.x;
        el('w' + i).style.pixelTop = spot.y;
        el('w' + i).style.visibility = spot.crowded ? 'hidden' : 'visible';
        p.sent = now;
        command('autoplace/' + i + '/' + spot.x + '/' + spot.y + '/' + width + '/' + spot.crowded + '/' +
            layoutResolution);
        return;
    }
}

/* Polling also catches work-area changes that IE6 does not report as onresize.
   Switch native profiles in place; never reload Active Desktop. */
var displaySignature = '',
    displaySince = 0,
    displaySent = 0,
    displayBusy = false,
    profileCaption = '';

function resolutionKey() {
    return (screen.width || screen.availWidth) + 'x' + (screen.height || screen.availHeight);
}

function resolutionReady() {
    var b = bounds();
    return !layoutReflow && layoutResolution == resolutionKey() && layoutWorkWidth == b[0] && layoutWorkHeight == b[1];
}

function hiddenForSpace(i) {
    return !panels[i].on && layoutHidden.charAt(i) == '1';
}

function displayWatch(now) {
    var b = bounds(),
        key = resolutionKey(),
        sig = key + '/' + b[0] + '/' + b[1] + '/' + (screen.colorDepth || 0),
        j, ready = true,
        count = 0,
        text;
    if (sig != displaySignature) {
        displaySignature = sig;
        displaySince = now;
        displaySent = 0;
        if (dragging >= 0) {
            var active = dragging;
            dragging = -1;
            el('h' + active).releaseCapture();
        }
        fitWallpaper();
    }
    if (!resolutionReady()) {
        displayBusy = true;
        if (!settingsView)
            for (j = 0; j < panels.length; j++)
                if (j != 21) el('w' + j).style.visibility = 'hidden';
        for (j = 0; j < panels.length; j++)
            if (panels[j].on && !measured[j]) ready = false;
        if (!settingsView && ready && !inFlight && !optionQueue.length && now - displaySince >= 500 && (!displaySent ||
                now - displaySent > 3000)) {
            var sizes = [];
            for (j = 0; j < panels.length; j++) sizes[j] = Math.max(40, heights[j]);
            displaySent = now;
            command('display/' + key.replace('x', '/') + '/' + b[0] + '/' + b[1] + '/' + sizes.join(','));
        }
    } else if (displayBusy) {
        displayBusy = false;
        displaySent = 0;
        applied = '';
        lastDraw = [];
        fitWallpaper();
    }
    for (j = 0; j < panels.length; j++)
        if (hiddenForSpace(j)) count++;
    text = key.replace('x', ' x ') + ' layout' + (displayBusy ? ' - switching...' : count ? ' - ' + count +
        ' hidden: not enough space' : '');
    if (text != profileCaption) {
        profileCaption = text;
        if (settingsView) {
            el('profileHint').innerText = text + '. Options and themes are shared.';
            managerListStamp = '';
        } else el('layoutNotice').innerText = count || displayBusy ? text + '  ' : '';
    }
}
