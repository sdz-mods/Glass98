/* Manager-only layout. IE6 quirks mode uses border-box pixel dimensions. */
var managerPage = 'widget',
    managerListStamp = '';

function managerResize() {
    if (!settingsView) return;
    var w = document.body.clientWidth,
        h = document.body.clientHeight,
        compact = w < 680,
        head = compact ? 93 : 99,
        foot = 83;
    el('manager').className = compact ? 'compact' : '';
    el('manager').style.pixelWidth = w;
    el('manager').style.pixelHeight = h;
    el('managerHeader').style.pixelWidth = w;
    el('managerHeader').style.pixelHeight = head;
    el('managerBody').style.pixelTop = head;
    el('managerBody').style.pixelWidth = w;
    el('managerBody').style.pixelHeight = Math.max(1, h - head - foot);
    el('managerBody').style.overflow = managerPage == 'widget' ? 'hidden' : 'auto';
    el('managerFooter').style.pixelTop = Math.max(head, h - foot);
    el('managerFooter').style.pixelWidth = w;
    el('managerFooter').style.pixelHeight = foot;
    el('widgetPicker').style.display = compact ? '' : 'none';
    el('widgetEditor').style.pixelLeft = compact ? 0 : 190;
    el('widgetEditor').style.pixelWidth = Math.max(250, w - 36 - (compact ? 0 : 190));
    el('widgetEditor').style.pixelHeight = Math.max(1, h - head - foot - 32);
    el('widgetList').style.pixelHeight = Math.max(1, h - head - foot - 32);
}

function managerTab(name) {
    managerPage = name;
    el('managerBody').scrollTop = 0;
    el('arrangeButton').style.display = name == 'desktop' ? '' : 'none';
    el('applyButton').innerText = name == 'desktop' ? 'Apply desktop' : 'Apply widget';
    if (!awaiting && !inFlight) el('notice').innerText = name == 'themes' ? 'Theme buttons apply immediately.' : name ==
        'desktop' ? 'Apply to save desktop settings.' : 'Apply to save the selected widget.';
    managerResize();
}

function managerChoose(i) {
    if (awaiting || inFlight || optionQueue.length) {
        el('notice').innerText = 'Please wait for Settings saved before selecting another widget.';
        return false;
    }
    el('target').value = String(i);
    loadWidget();
    el('widgetEditor').scrollTop = 0;
    return false;
}

function managerSelection() {
    var i = Number(el('target').value) || 0,
        j, stamp = String(i),
        s = '';
    el('widgetTitle').innerText = titles[i];
    for (j = 0; j < panels.length; j++) stamp += '|' + panels[j].on + layoutHidden.charAt(j);
    if (stamp == managerListStamp) return;
    managerListStamp = stamp;
    for (j = 0; j < titles.length; j++)
        if (j != 5 && j != 21) s += '<a href="#" class="' + (j == i ? 'selected' : '') +
            '" onclick="return managerChoose(' + j + ')"><span class="visibility">' + (panels[j].on ? 'On' :
                hiddenForSpace(j) ? 'Hidden' : 'Off') + '</span>' + esc(titles[j]) + '</a>';
    var position = el('widgetList').scrollTop;
    el('widgetList').innerHTML = s;
    el('widgetList').scrollTop = position;
}

function managerPreview() {
    if (!settingsView) return;
    var p = panels[0],
        low = hexColor(el('meterLow').value, '62D98B'),
        mid = hexColor(el('meterMid').value, 'FFD166'),
        t = el('previewTint'),
        c = el('previewContent');
    panelBackground(t, c, p);
    c.style.color = '#' + p.fg;
    c.style.borderColor = '#' + p.accent;
    el('previewTitle').style.color = '#' + p.accent;
    el('previewCPU').style.backgroundColor = '#' + (el('meterColors').checked ? mixColor(low, mid, .84) : p.accent);
    el('previewRAM').style.backgroundColor = '#' + (el('meterColors').checked ? mid : p.accent);
}

function managerInit() {
    managerResize();
    managerSelection();
    window.attachEvent('onresize', managerResize);
    window.setInterval(managerSelection, 1000);
}
