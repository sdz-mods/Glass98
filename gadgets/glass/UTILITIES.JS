/* IE6 desktop controls and local utilities. Calculator input is parsed, never evaluated as script. */
var memoryDetails = {},
    desktopState = {},
    windowsSchemes = [],
    windowsSchemeCurrent = '',
    clipboardData = {},
    cdInfo = {};
var toolsMode = 'calculator',
    toolsCategory = 'length',
    characterPage = 2,
    dailyDate = '',
    dailyPending = null,
    choiceMenu = null,
    choiceLists = {},
    desktopThemeChoice = null,
    desktopThemeSeen = null,
    windowsSchemeChoice = null,
    windowsSchemeSeen = null;
var unitGroups = {
    length: [
        ['mm', 0.001],
        ['cm', 0.01],
        ['m', 1],
        ['km', 1000],
        ['in', 0.0254],
        ['ft', 0.3048],
        ['yd', 0.9144],
        ['mi', 1609.344]
    ],
    mass: [
        ['g', 0.001],
        ['kg', 1],
        ['oz', 0.028349523125],
        ['lb', 0.45359237]
    ],
    temperature: [
        ['Celsius', 1, 273.15],
        ['Fahrenheit', 5 / 9, 459.67],
        ['Kelvin', 1, 0]
    ],
    area: [
        ['square m', 1],
        ['square km', 1000000],
        ['square ft', 0.09290304],
        ['acre', 4046.8564224],
        ['hectare', 10000]
    ],
    volume: [
        ['mL', 0.001],
        ['L', 1],
        ['US gallon', 3.785411784],
        ['UK gallon', 4.54609]
    ],
    speed: [
        ['m/s', 1],
        ['km/h', 1 / 3.6],
        ['mph', 0.44704],
        ['knots', 0.514444444444]
    ],
    data: [
        ['bytes', 1],
        ['KiB', 1024],
        ['MiB', 1048576],
        ['GiB', 1073741824],
        ['kB', 1000],
        ['MB', 1000000],
        ['GB', 1000000000]
    ]
};

function calculate(text) {
    var tokens = [],
        at = 0,
        p = 0,
        depth = 0,
        m, c;
    text = String(text);
    if (!text.length || text.length > 120) throw new Error('Enter an expression of up to 120 characters.');
    while (p < text.length) {
        c = text.charAt(p);
        if (/\s/.test(c)) {
            p++;
            continue;
        }
        m = /^(?:\d+\.?\d*|\.\d+)(?:[eE][+-]?\d+)?/.exec(text.substring(p));
        if (m) {
            tokens[tokens.length] = Number(m[0]);
            p += m[0].length;
        } else if ('+-*/%^()'.indexOf(c) >= 0) {
            tokens[tokens.length] = c;
            p++;
        } else throw new Error('Use numbers, parentheses and + - * / % ^.');
    }

    function primary() {
        var t = tokens[at++],
            v;
        if (++depth > 32) throw new Error('Too many nested operations.');
        if (typeof t == 'number') v = t;
        else if (t == '(') {
            v = expression();
            if (tokens[at++] != ')') throw new Error('Missing closing parenthesis.');
        } else throw new Error('Expected a number.');
        depth--;
        return v;
    }

    function power() {
        var v = primary();
        if (tokens[at] == '^') {
            at++;
            v = Math.pow(v, unary());
        }
        return v;
    }

    function unary() {
        var t = tokens[at];
        if (t == '+' || t == '-') {
            at++;
            return (t == '-' ? -1 : 1) * unary();
        }
        return power();
    }

    function term() {
        var v = unary(),
            t, r;
        while (tokens[at] == '*' || tokens[at] == '/' || tokens[at] == '%') {
            t = tokens[at++];
            r = unary();
            if ((t == '/' || t == '%') && r == 0) throw new Error('Division by zero.');
            v = t == '*' ? v * r : t == '/' ? v / r : v % r;
        }
        return v;
    }

    function expression() {
        var v = term(),
            t;
        while (tokens[at] == '+' || tokens[at] == '-') {
            t = tokens[at++];
            v += (t == '+' ? 1 : -1) * term();
        }
        return v;
    }
    var value = expression();
    if (at != tokens.length) throw new Error('Unexpected input.');
    if (!isFinite(value)) throw new Error('Result is outside the supported range.');
    return value;
}

function convertUnits(value, group, from, to) {
    var units = unitGroups[group],
        a, b, base;
    if (!units || !units[from] || !units[to] || !isFinite(value)) throw new Error('Enter a finite value.');
    if (group == 'temperature') {
        base = from == 0 ? value : from == 1 ? (value - 32) / 1.8 : value - 273.15;
        if (base < -273.150000001) throw new Error('Temperature is below absolute zero.');
        return to == 0 ? base : to == 1 ? base * 1.8 + 32 : base + 273.15;
    }
    a = units[from];
    b = units[to];
    base = (value + (a[2] || 0)) * a[1];
    return base / b[1] - (b[2] || 0);
}

function convertBase(text, base) {
    text = String(text).replace(/^\s+|\s+$/g, '');
    var pattern = base == 16 ? /^(?:0x)?[0-9a-f]+$/i : base == 2 ? /^(?:0b)?[01]+$/i : /^\d+$/;
    if (!pattern.test(text)) throw new Error('Enter an unsigned 32-bit integer in the selected base.');
    if (base == 16) text = text.replace(/^0x/i, '');
    if (base == 2) text = text.replace(/^0b/i, '');
    var value = parseInt(text, base);
    if (value < 0 || value > 4294967295 || !isFinite(value)) throw new Error('Use a value from 0 to 4294967295.');
    return {
        decimal: String(value),
        hex: value.toString(16).toUpperCase(),
        binary: value.toString(2)
    };
}

function toolsCalculate() {
    var value, result;
    try {
        if (toolsMode == 'calculator') result = calculate(el('toolValue').value);
        else if (toolsMode == 'units') {
            if (!String(el('toolValue').value).replace(/\s/g, '').length) throw new Error('Enter a value.');
            value = convertUnits(Number(el('toolValue').value), toolsCategory, Number(el('unitFrom').value), Number(el(
                'unitTo').value));
            if (!isFinite(value)) throw new Error('Result is outside the supported range.');
            result = Number(value.toPrecision(12)) + ' ' + unitGroups[toolsCategory][Number(el('unitTo').value)][0];
        } else {
            value = convertBase(el('toolValue').value, Number(el('numberBase').value));
            result = 'Decimal: ' + value.decimal + '\nHex: ' + value.hex + '\nBinary: ' + value.binary;
        }
        el('toolResult').innerText = String(result);
    } catch (error) {
        el('toolResult').innerText = error.message || 'Invalid input';
    }
    scheduleFit(24);
}

function toolsChange() {
    toolsMode = el('toolMode').value;
    render(24);
}

function unitChange() {
    toolsCategory = el('unitGroup').value;
    render(24);
}

/* Active Desktop's native select controls can consume the first click for activation.
   Use ordinary HTML links in a separate popup, outside clipped widget panels. */
function chooseMarkup(id, choices, selected, action) {
    var j, label = '';
    choiceLists[id] = {items: choices, action: action};
    for (j = 0; j < choices.length; j++)
        if (String(selected) == String(choices[j][0])) label = choices[j][1];
    return '<input type="hidden" id="' + id + '" value="' + esc(selected) + '">' +
        '<a href="#" id="' + id + 'Button" class="choiceButton" onclick="choiceOpen(\'' + id +
        '\');return false">' + esc(label || 'Choose...') + ' &#9660;</a>';
}

function choiceClose() {
    if (choiceMenu) {
        el('choicePopup').style.display = 'none';
        choiceMenu = null;
    }
}

function choiceOpen(id) {
    var choices = choiceLists[id], anchor = el(id + 'Button'), popup = el('choicePopup'),
        node = anchor, x = 0, y = 0, j, s = '', h;
    if (choiceMenu == id) { choiceClose(); return; }
    choiceClose();
    if (!choices || !choices.items.length) return;
    while (node) { x += node.offsetLeft || 0; y += node.offsetTop || 0; node = node.offsetParent; }
    h = Math.min(choices.items.length * 24 + 4, Math.max(24, bounds()[1] - 8), 244);
    for (j = 0; j < choices.items.length; j++)
        s += '<a href="#" onclick="choicePick(' + j + ');return false">' +
            esc(choices.items[j][1]) + '</a>';
    popup.innerHTML = s;
    popup.style.pixelLeft = Math.max(2, Math.min(x, bounds()[0] - 242));
    popup.style.pixelTop = Math.max(2, Math.min(y + (anchor.offsetHeight || 20), bounds()[1] - h - 4));
    popup.style.pixelHeight = h;
    popup.style.display = 'block';
    choiceMenu = id;
}

function choicePick(index) {
    var id = choiceMenu, choices = choiceLists[id], item;
    if (!choices || index < 0 || index >= choices.items.length) return;
    item = choices.items[index];
    el(id).value = String(item[0]);
    el(id + 'Button').innerText = item[1] + ' \u25bc';
    choiceClose();
    if (id == 'desktopTheme') desktopThemeChoice = item[0];
    if (id == 'windowsScheme') windowsSchemeChoice = item[0];
    if (choices.action == 'toolsChange()') toolsChange();
    if (choices.action == 'unitChange()') unitChange();
}

if (document.attachEvent) {
    document.attachEvent('onmousedown', function() {
        var node = event.srcElement;
        while (node) {
            if (node.id == 'choicePopup' || node.id == choiceMenu + 'Button') return;
            node = node.parentNode;
        }
        choiceClose();
    });
    document.attachEvent('onkeydown', function() {
        if (event.keyCode == 27) choiceClose();
    });
}

function toolMarkup() {
    var s = chooseMarkup('toolMode', [
            ['calculator', 'Calculator'],
            ['units', 'Unit converter'],
            ['bases', 'Hex / decimal / binary']
        ], toolsMode, 'toolsChange()'),
        j, options = [],
        groups = [];
    if (toolsMode == 'units') {
        for (j in unitGroups)
            if (unitGroups.hasOwnProperty(j)) groups[groups.length] = [j, j];
        s += '<div class="section">' + chooseMarkup('unitGroup', groups, toolsCategory, 'unitChange()') + '</div>';
        for (j = 0; j < unitGroups[toolsCategory].length; j++) options[options.length] = [j, unitGroups[toolsCategory][
            j
        ][0]];
        s += '<div class="section">' + chooseMarkup('unitFrom', options, 0, '') + ' to ' + chooseMarkup('unitTo',
            options, 1, '') + '</div>';
    }
    if (toolsMode == 'bases') s += '<div class="section">' + chooseMarkup('numberBase', [
        [10, 'Decimal'],
        [16, 'Hexadecimal'],
        [2, 'Binary']
    ], 10, '') + '</div>';
    s += '<div class="section"><input id="toolValue" maxlength="120" style="width:95%" onkeydown="if(event.keyCode==13)toolsCalculate()"></div>' +
        '<div class="buttons">' + localButton('Calculate', 'toolsCalculate()') +
        '</div><div class="detail" id="toolResult" style="word-wrap:break-word"></div>';
    if (toolsMode == 'calculator') s += detail('Operators: + - * / % (remainder) ^ (power)');
    if (toolsMode == 'bases') s += detail('Unsigned 32-bit values');
    return s;
}

function dayKey(date) {
    return date.getFullYear() + '-' + pad(date.getMonth() + 1) + '-' + pad(date.getDate());
}

function dailyItems() {
    var a = lines(savedOptions.dailyItems),
        result = [],
        j;
    for (j = 0; j < a.length && result.length < 24; j++)
        if (/\S/.test(a[j])) result[result.length] = a[j];
    return result;
}

function dailyMask() {
    var value = savedOptions.dailyState || '';
    if (dailyPending) {
        if (value == dailyPending.value && !awaiting && !inFlight) dailyPending = null;
        else value = dailyPending.value;
    }
    var a = String(value).split('|');
    return a[0] == dayKey(new Date()) ? num(a[1], 0, 16777215) : 0;
}

function dailyToggle(index) {
    if (awaiting || inFlight || index < 0 || index >= dailyItems().length) return;
    dailyPending = {value: dayKey(new Date()) + '|' + (dailyMask() ^ (1 << index)), at: (new Date()).getTime()};
    queueLive('dailyState', dailyPending.value);
    render(26);
}

function wallpaperNext(delta) {
    var slots = [],
        current = -1,
        j;
    for (j = 0; j < 6; j++)
        if (savedOptions['wall' + j]) {
            if (savedOptions['wall' + j] == savedWallpaper) current = slots.length;
            slots[slots.length] = j;
        }
    if (slots.length) command('util/wallset/' + slots[(current < 0 ? (delta < 0 ? slots.length - 1 : 0) : current +
        delta + slots.length) % slots.length]);
}

function applyWindowsScheme() {
    var value = String(el('windowsScheme').value), j;
    for (j = 0; j < windowsSchemes.length; j++)
        if (windowsSchemes[j][0] == value) {
            command('util/scheme/' + value);
            return;
        }
}

function applyDesktopTheme() {
    if (awaiting || inFlight) return;
    optionQueue[optionQueue.length] = 'theme/apply/' + num(el('desktopTheme').value, 0, 1001);
    awaiting = 'pending';
    waitSince = (new Date()).getTime();
}

function cdDescription() {
    return (cdInfo.album ? section(cdInfo.album, detail(cdInfo.artist || '')) : '') + (cdInfo.title ? detail(cdInfo
        .title) : '') + (cdInfo.id && cdInfo.catalog && !cdInfo.album ? detail(cdInfo.matches < 0 ?
        'Offline catalog could not be read' : 'Disc not found in offline catalog') : '');
}

function cdEditButton() {
    if (!cdInfo.id || !/^[a-zA-Z0-9-]{1,32}$/.test(cdInfo.id) || cdTracks < 1) return '';
    return button('Edit titles', 'util/cdedit/' + cdInfo.id + '/' + cdTracks) + (cdInfo.matches > 1 ? button(
        'Match ' + (cdInfo.choice + 1) + '/' + cdInfo.matches + ' - Next', 'util/cdchoice/' + cdInfo.id + '/' +
        cdTracks + '/' + ((cdInfo.choice + 1) % cdInfo.matches)) : '');
}

function characterMove(delta) {
    characterPage = (characterPage + delta + 3) % 3;
    render(25);
}

function utilitySettings(i) {
    var s = '',
        j;
    if (i == 22) {
        for (j = 0; j < 6; j++) s += '<div class="shortcutsetup"><span id="wallLabel' + j + '">' + esc(savedOptions[
                'wall' + j] || 'Wallpaper ' + (j + 1)) +
            '</span><button onclick="command(\'util/wallchoose/' + j +
            '\')">Choose image</button> <button onclick="command(\'util/wallclear/' + j + '\')">Clear</button></div>';
        s += '<p class="help">Windows Appearance lists the schemes installed in Display Properties, including your saved schemes.</p>';
    }
    if (i == 23) s =
        '<p>Physical RAM and Windows-reported paging capacity. Virtual space and largest free block describe the collector process. These are not physical RAM blocks or the swap file size on disk. The largest-block scan runs at most once every ten seconds.</p>';
    if (i == 24) s =
        '<p>Arithmetic, unit conversion and unsigned 32-bit number conversion. Calculations run locally. Results use floating-point arithmetic.</p>';
    if (i == 25) s = '<p>Font</p>' + textOption('characterFont', savedOptions.characterFont || 'Tahoma', false) +
        '<p class="help">Latin characters. Click one to copy it as Windows text. The active Windows code page must support that character.</p>';
    if (i == 26) s =
        '<p>One task per line, up to 24 tasks / 900 UTF-8 bytes. Check marks reset at local midnight.</p>' + textOption(
            'dailyItems', savedOptions.dailyItems, true);
    if (i == 27) s =
        '<p>Displays up to 2048 bytes of plain text from the clipboard. No history is kept. While enabled, the local telemetry file contains the preview; disabling the widget clears it on the next collection.</p>';
    return s;
}

function utilityValidate(i) {
    if (i == 25 && !/^[a-zA-Z0-9 ,_-]{1,60}$/.test(el('characterFont').value))
        return 'Use a font name of up to 60 letters, digits, spaces, commas, dashes or underscores.';
    if (i == 26 && (lines(el('dailyItems').value).length > 24 || unescape(encodeURIComponent(el('dailyItems').value))
            .length > 900)) return 'Use up to 24 task lines and 900 UTF-8 bytes.';
    return '';
}

function utilitySave(i) {
    if (i == 25) queueOption('characterFont', el('characterFont').value);
    if (i == 26) {
        if (lines(savedOptions.dailyItems).join('\n') != lines(el('dailyItems').value).join('\n')) queueOption(
            'dailyState', '');
        queueOption('dailyItems', el('dailyItems').value);
    }
}

function memoryRow(label, total, available, i) {
    return section(label, meter(total > 0 ? (total - available) / total * 100 : -1, i) + detail(bytes(Math.max(0,
        total - available)) + ' used / ' + bytes(total) + ' total / ' + bytes(available) + ' available'));
}

function utilityRender(i, ok) {
    var s = '',
        j, a, mask, code, options = [],
        font;
    if (i == 22) {
        if (desktopThemeSeen != themeActive) {
            desktopThemeSeen = themeActive;
            desktopThemeChoice = themeActive;
        }
        s = section('WALLPAPER', detail(String(savedWallpaper).split(/[\\/]/).pop()) + '<div class="buttons">' +
            localButton('Previous', 'wallpaperNext(-1)') + localButton('Next', 'wallpaperNext(1)') + editLink(i) +
            '</div>');
        s += section('SCREENSAVER', detail(ok ? desktopState.saver ? 'Enabled' : 'Disabled' : 'Unavailable') +
            '<div class="buttons">' + button('Start', 'util/saverstart') + button(ok && desktopState.saver ?
                'Disable' : 'Enable', 'util/saver/' + (ok && desktopState.saver ? 0 : 1)) + button('Settings',
                'util/saversettings') + '</div>');
        for (j = 0; j < themeList.length; j++) options[options.length] = [themeList[j].id, themeList[j].name];
        s += section('GLASS98 THEME', chooseMarkup('desktopTheme', options, desktopThemeChoice, '') + '<div class="buttons">' +
            localButton('Apply', 'applyDesktopTheme()') + '</div>');
        if (windowsSchemes.length) {
            if (windowsSchemeSeen != windowsSchemeCurrent || windowsSchemeChoice == null) {
                windowsSchemeSeen = windowsSchemeCurrent;
                windowsSchemeChoice = windowsSchemeCurrent;
            }
            s += section('WINDOWS APPEARANCE', chooseMarkup('windowsScheme', windowsSchemes,
                windowsSchemeChoice, '') + '<div class="buttons">' +
                localButton('Apply', 'applyWindowsScheme()') + button('Settings', 'util/display') + '</div>');
        }
    }
    if (i == 23) {
        if (!ok || !memoryDetails.physical) return detail('Memory information unavailable');
        s = memoryRow('PHYSICAL RAM', memoryDetails.physical, memoryDetails.available, i) + memoryRow(
            'PAGING CAPACITY (WINDOWS)', memoryDetails.pageTotal, memoryDetails.pageAvailable, i) +
            detail('With automatic sizing, 100% does not necessarily mean Windows is out of memory.') + memoryRow(
            'COLLECTOR VIRTUAL SPACE', memoryDetails.virtualTotal, memoryDetails.virtualAvailable, i);
        s += section('LARGEST FREE VIRTUAL BLOCK', detail(memoryDetails.largest >= 0 ? bytes(memoryDetails.largest) :
            'Unavailable'));
    }
    if (i == 24) s = toolMarkup();
    if (i == 25) {
        font = /^[a-zA-Z0-9 ,_-]{1,60}$/.test(savedOptions.characterFont || '') ? savedOptions.characterFont : 'Tahoma';
        s = '<div style="font-family:' + font + ';font-size:16px">';
        for (j = 0; j < 64; j++) {
            code = characterPage * 64 + j + 32;
            if (code >= 127) code += 33;
            if (code > 255) break;
            s += '<span style="display:inline-block;width:25px;line-height:25px;text-align:center"><a href="#" title="U+' +
                code.toString(16).toUpperCase() + '" onclick="command(\'util/character/' + code + '\');return false">' +
                esc(String.fromCharCode(code)) + '</a></span>';
        }
        s += '</div><div class="buttons">' + localButton('Previous', 'characterMove(-1)') + localButton('Next',
            'characterMove(1)') + '</div>' + detail('Click a character to copy it');
    }
    if (i == 26) {
        a = dailyItems();
        mask = dailyMask();
        s = detail(dayKey(new Date())) + '<div style="max-height:240px;overflow:auto;word-wrap:break-word">';
        for (j = 0; j < a.length; j++) s += '<div class="detail">' + localButton(mask & (1 << j) ? '[x]' : '[ ]',
            'dailyToggle(' + j + ')') + ' ' + esc(a[j]) + '</div>';
        s += '</div>' + (!a.length ? detail('Add tasks in widget options') : '') + '<div class="buttons">' + editLink(
            i) + '</div>';
    }
    if (i == 27) s = '<div style="height:120px;overflow:auto;word-wrap:break-word">' + detail(!ok ?
        'Clipboard unavailable' : clipboardData.state == -1 ? 'Clipboard busy' : clipboardData.state != 1 ?
        'No plain text on the clipboard' : clipboardData.text || '(empty text)') + '</div>' + (ok && clipboardData
        .truncated ? detail('Preview limited to 2048 bytes') : '');
    return s;
}

function utilityTick(now) {
    var j, today;
    if (dailyPending && now - dailyPending.at > 20000) {
        dailyPending = null;
        lastDraw[26] = 0;
    }
    if (settingsView) {
        for (j = 0; j < 6; j++)
            if (el('wallLabel' + j)) el('wallLabel' + j).innerText = savedOptions['wall' + j] || 'Wallpaper ' + (j + 1);
    } else if (panels[26].on) {
        today = dayKey(new Date());
        if (today != dailyDate) {
            dailyDate = today;
            lastDraw[26] = 0;
        }
    }
}
