// Logic checks for desktop rendering, settings and placement.
const fs = require('node:fs'),
    vm = require('node:vm'),
    assert = require('node:assert/strict'),
    path = require('node:path');
const html = fs.readFileSync(path.join(__dirname, '../gadgets/glass/GLASS.HTM'), 'utf8');
assert.ok(html.indexOf('ADDONS.JS') < html.indexOf('id="extra"'),
    'initialize optional telemetry defaults before loading its first snapshot');
const elements = new Proxy({}, {
    get(o, k) {
        return o[k] || (o[k] = {
            style: {
                filter: ''
            },
            offsetHeight: 100,
            scrollHeight: 100,
            value: '',
            innerHTML: '',
            options: {
                add() {}
            },
            setCapture() {},
            releaseCapture() {}
        });
    }
});
const ctx = vm.createContext({
    document: {
        all: elements,
        body: {
            clientWidth: 1920,
            clientHeight: 1040,
            style: {}
        }
    },
    screen: {
        availWidth: 1920,
        availHeight: 1040
    },
    location: {
        search: ''
    },
    window: {
        location: {},
        setInterval() {},
        setTimeout() {},
        open() {},
        close() {}
    },
    Image: function() {},
    Option: function() {},
    event: {},
    console
});
const run = s => vm.runInContext(s, ctx);
for (const m of html.matchAll(/<script[^>]*>([\s\S]*?)<\/script>/g)) run(m[1]);
run(fs.readFileSync(path.join(__dirname, '../gadgets/glass/THEMES.JS'), 'utf8'));
run(fs.readFileSync(path.join(__dirname, '../gadgets/glass/ADDONS.JS'), 'utf8'));
run(fs.readFileSync(path.join(__dirname, '../gadgets/glass/UTILITIES.JS'), 'utf8'));
run(fs.readFileSync(path.join(__dirname, '../gadgets/glass/MANAGER.JS'), 'utf8'));
run(fs.readFileSync(path.join(__dirname, '../gadgets/glass/PLACEMENT.JS'), 'utf8'));
run('layoutResolution=resolutionKey();layoutWorkWidth=screen.availWidth;layoutWorkHeight=screen.availHeight');
run(fs.readFileSync(path.join(__dirname, '../gadgets/glass/WIDGETS.JS'), 'utf8'));
assert.equal(run('panels.length'), 28);
assert.equal(run('serialize().split("|").length'), 229);
assert.equal(run('panels.filter(function(p){return p.on}).length'), 1);
run('dragging=0;move(4000,4000,true)');
assert.equal(run('panels[0].x+width'), 1918, 'right border stays inside screen');
run('panels[0].x=500;panels[0].y=100;panels[1].on=1;dragging=1;move(507,418,false)');
assert.equal(run('panels[1].x'), 500);
assert.equal(run('panels[1].y'), run('panels[0].y+heights[0]+10'));
run('move(507,418,true)');
assert.equal(run('panels[1].x'), 507);
run('locked=1;dragging=-1;beginDrag(0)');
assert.equal(run('dragging'), -1);
assert.equal(run("parse('2|bad')"), false);
run(
    "snapshot='20|30|131072|65536|0|0|0|0|CPU <test>|0|6|3|1|1';sampleTime=Date.now();extraTime=Date.now();hardware={gpu:'GPU',width:1920,height:1080,hz:60,bpp:24,os:'Windows 98 SE',uptime:3661};render(0)"
);
assert.match(elements.flow0.innerHTML, /50%/);
assert.match(elements.flow0.innerHTML, /64 MB used/);
assert.match(elements.flow0.innerHTML, /CPU &lt;test&gt;/);
assert.match(elements.flow0.innerHTML, /@ 60 Hz/);
run('sampleTime=0;render(0)');
assert.match(elements.flow0.innerHTML, /CPU unavailable/);
run("savedOptions.drives='C,Z';drives=[{id:'C',type:3,total:1024,free:512,ready:1},{id:'D',type:5,ready:0}];render(1)");
assert.match(elements.flow1.innerHTML, /C:/);
assert.doesNotMatch(elements.flow1.innerHTML, /D:/);
run('drives=[];render(1)');
assert.match(elements.flow1.innerHTML, /No selected drives/);
run(
    "networks=[{id:'one',name:'Adapter',rx:100,tx:200,totalIn:300,totalOut:400,ip:'1.2.3.4',gateway:'1.2.3.1',admin:1,state:5}];savedOptions.adapters='one';render(2)"
);
assert.doesNotMatch(elements.flow2.innerHTML, /Driver reports|Enabled|Unreachable/);
run(
    "savedOptions.clock='analog12';render(3);render(4);processes=[];battery={flags:128};render(6);render(7);render(8);render(9);render(10);rssItems=['<script>unsafe</script>'];render(11);render(12)"
);
assert.doesNotMatch(elements.flow3.innerHTML, /class="dial"/);
assert.match(elements.flow4.innerHTML, /class="today"/);
assert.match(elements.flow6.innerHTML, /No battery/);
assert.match(elements.flow11.innerHTML, /&lt;script&gt;/);
run("optionQueue=[];queueOption('reminders',new Array(151).join('abc'));awaiting='pending';tick()");
assert.equal(run('inFlight'), 'begin/reminders');
assert.ok(run('optionQueue.every(function(x){return x.length<150})'));
run('actionAck=inFlight;ackTime++;ackOK=1;tick()');
assert.match(run('inFlight'), /^part\//);
assert.ok(run('panelCommand(12,"save").length') < 180);
// Explicit escaping for a Windows path passed through the JS parser.
run('setWallpaper(' + JSON.stringify('C:\\My Pictures\\a#b%.jpg') + ')');
assert.equal(run('wallLoader.src'), 'file:///C:/My%20Pictures/a%23b%25.jpg');
run("savedOptions.globalSeconds='9'");
assert.equal(run('effectiveSeconds(0)'), 9);
run("savedOptions.globalSeconds='0'");
assert.equal(run('effectiveSeconds(0)'), run('panels[0].seconds'));
run("mixer=[{volume:30,mute:0}];command('device/volume/0/1/1');render(7)");
assert.match(elements.flow7.innerHTML, /Unmute/);
run("command('device/volume/0/1/0');render(7)");
assert.match(elements.flow7.innerHTML, />Mute</);
run("savedOptions.launch0='C:\\Games\\GAME.EXE';savedOptions.label0='My game';render(10)");
assert.match(elements.flow10.innerHTML, /My game/);
assert.doesNotMatch(elements.flow10.innerHTML, /<p>/);
run('parse(defaults());panels[5].on=1;parse(serialize())');
assert.equal(run('panels[5].on'), 0);
run('panels[0].on=1;el("h0").offsetHeight=25;el("flow0").offsetHeight=140;fitContent(0)');
assert.equal(run('heights[0]'), 185);
// Clock title overlays its main area; do not reserve a second header row.
run('panels[3].on=1;el("h3").offsetHeight=25;el("flow3").offsetHeight=62;fitContent(3)');
assert.equal(run('heights[3]'), 82, 'clock placement height matches its centered main area');
run('el("flow3").offsetHeight=152;fitContent(3)');
assert.equal(run('heights[3]'), 172, 'world-clock rows still contribute to layout height');
run("savedOptions={drives:'C'};el('target').value='1';loadWidget()");
assert.match(elements.specific.innerHTML, /id="drive2" checked/);
assert.equal((elements.specific.innerHTML.match(/checked/g) || []).length, 1);
// Upgrade existing layouts without losing positions/colors or enabling new collectors.
run("var oldLayout=defaults().split('|').slice(0,109);oldLayout[0]='2';oldLayout[6]='123';parse(oldLayout.join('|'))");
assert.equal(run('panels[0].x'), 123);
assert.ok(run('panels.slice(13).every(function(p){return !p.on})'));
assert.equal(run("validDate('2025-02-29')"), null);
assert.ok(run("validDate('2024-02-29')"));
assert.equal(run("worldList('Tokyo|+09:00')[0][1]"), 540);
assert.equal(run("worldList('Bad|+09:65')"), null);
run("savedOptions.worlds='Tokyo|+09:00\\nNew York|-04:00';render(3)");
assert.match(elements.flow3.innerHTML, /Tokyo/);
assert.match(elements.flow3.innerHTML, /New York/);
run("savedOptions.notes='[ ] test <unsafe>\\n[x] Done';awaiting='';inFlight='';optionQueue=[];render(15)");
assert.match(elements.flow15.innerHTML, /&lt;unsafe&gt;/);
run('noteToggle(0)');
assert.match(run('savedOptions.notes'), /^\[x\]/);
assert.equal(run("timerValue(['countdown','run',10000,1000],6000)"), 5000);
assert.equal(run("timerValue(['stopwatch','run',5000,1000],6000)"), 10000);
assert.equal(run("timerValue(['countdown','pause',5000,0],6000)"), 5000);
assert.equal(run("timerValue(['countdown','run',10000,1000],12000)"), 0);
run("timerLocal='countdown|pause|5000|0';render(16)");
const timerMarkup = elements.flow16.innerHTML;
run("timerLocal='stopwatch|run|1000|'+Date.now();render(16)");
assert.equal(elements.flow16.innerHTML, timerMarkup, 'timer keeps its controls mounted while updating');
assert.equal(elements.timerToggle.innerText, 'Pause');
run("timerLocal='';timerAck=''");
run("optionQueue=[];timerSet(['stopwatch','pause',12345,0])");
assert.equal(run('timerState()[2]'), 12345);
assert.equal(run('optionQueue.length'), 1);
assert.ok(run('optionQueue[0].length') < 110);
run("timerLocal='';timerAck='';optionQueue=[];awaiting=''");
run("resources={system:20,user:40,gdi:20};extraTime=Date.now();render(14)");
assert.match(elements.flow14.innerHTML, /USER FREE/);
assert.match(elements.flow14.innerHTML, /40%/);
run("recentFiles=[{id:123,name:'Doc <one>'}];savedOptions.fav0='C:\\Windows';render(17)");
assert.match(elements.flow17.innerHTML, /Doc &lt;one&gt;/);
assert.match(elements.flow17.innerHTML, /util\/recent\/123/);
run("mediaDrives=[{id:'D',type:5,ready:0}];render(18)");
assert.match(elements.flow18.innerHTML, /Eject/);
assert.doesNotMatch(elements.flow18.innerHTML, />Open</);
run('savedOptions.photo0=' + JSON.stringify('C:\\Pictures\\a#b.jpg') + ';render(19)');
assert.match(elements.flow19.innerHTML, /a%23b.jpg/);
run("savedOptions.eventName='Test';savedOptions.eventDate='2024-02-29';render(20);render(13)");
assert.match(elements.flow20.innerHTML, /since the event/);
run("cdState='playing';cdTrack=3;cdTracks=12;render(9)");
assert.match(elements.flow9.innerHTML, /Track 3 of 12/);
run("diskActivity={read:200,write:100};history=[[10,20,30,200,100]];render(12)");
assert.match(elements.flow12.innerHTML, /FILE READ/);
console.log(
    'PASS: 26 widgets, legacy migration, timers, notes, world clocks, media, pictures, resources, CD tracks, file activity and existing regressions'
);
run("savedOptions.meterColors='0';panels[0].accent='ABCDEF'");
assert.equal(run('meterColor(100,0)'), 'ABCDEF');
run(
    "savedOptions.meterColors='1';savedOptions.meterLow='00FF00';savedOptions.meterMid='FFFF00';savedOptions.meterHigh='FF0000'"
);
assert.equal(run('meterColor(0,0)'), '00FF00');
assert.equal(run('meterColor(50,0)'), 'FFFF00');
assert.equal(run('meterColor(100,0)'), 'FF0000');
assert.equal(run('meterColor(25,0)'), '80FF00');
assert.equal(run('meterColor(100,6)'), '00FF00');
assert.equal(run('meterColor(0,6)'), 'FF0000');
assert.equal(run('meterColor(0,14)'), 'FF0000');
assert.equal(run('meterColor(100,7)'), run('panels[7].accent'));
assert.equal(run('meterColor(-1,0)'), 'ABCDEF');
run('history=[[0,0,0,0,0],[100,100,100,100,100]];render(12)');
assert.equal(elements.graphBar0_0.style.backgroundColor, '#00FF00');
assert.equal(elements.graphBar0_1.style.backgroundColor, '#FF0000');
const graphTemplate = elements.flow12.innerHTML;
run('history=[[50,50,50,50,50]];render(12)');
assert.equal(elements.flow12.innerHTML, graphTemplate, 'graph updates keep their bars mounted');
assert.equal(elements.graphBar0_0.style.backgroundColor, '#FFFF00');

run("savedOptions.meterLow='bad';savedOptions.meterMid='bad';savedOptions.meterHigh='bad'");
assert.equal(run('meterColor(0,0)'), '62D98B');
run(
    "awaiting='';inFlight='';optionQueue=[];el('meterColors').checked=true;el('meterLow').value='123456';el('meterMid').value='789ABC';el('meterHigh').value='DEF012';el('themeName').value='My theme';themeSave(false)"
);
assert.equal(run('optionQueue[0]'), 'theme/meter/1,123456,789ABC,DEF012');
assert.match(run('optionQueue[1]'), /^theme\/new\//);
assert.ok(run('optionQueue.every(function(x){return x.length+16<128})'));
run(
    "themeList=[{id:4,name:'Paper'}];themeActive=4;el('target').value='0';panels[0].bg='F4F1E8';panels[0].fg='202B35';panels[0].accent='245F95';panels[0].alpha=96;el('allColors').checked=true;loadThemes()"
);
assert.equal(elements.bg.value, 'F4F1E8');
assert.equal(elements.fg.value, '202B35');
assert.equal(elements.allColors.checked, false);
console.log('PASS: theme commands, optional gradient, reverse scales, historical graph colors');

run(
    "panels[10].accent='123456';panels[10].fg='ABCDEF';savedOptions.launch0='C:\\Games\\GAME.EXE';savedOptions.label0='Game';render(10)"
);
assert.match(elements.flow10.innerHTML, /<a style="color:#123456;border-color:#123456"/);
assert.match(elements.flow10.innerHTML, /<span style="color:#ABCDEF" class="launchfile"/);
assert.match(run('themeMarkup(' + JSON.stringify('<td class="today">27</td>') + ',10)'), /border-color:#123456/);
// Opacity changes reuse the existing native alpha filter.
run(
    "var alphaObject={opacity:55};el('t0')._alpha=55;el('t0').style.filter='alpha(opacity=55)';el('t0').filters={item:function(){return alphaObject;}};panels[0].on=1;panels[0].alpha=78;draw(0)"
);
assert.equal(run('alphaObject.opacity'), 78);
assert.equal(elements.t0.style.filter, 'alpha(opacity=55)', 'opacity changes must not recreate the filter');

// Opaque mode must detach filters, including on hidden widgets, and preserve opacity.
run(
    "savedOptions.disableAlpha='1';el('t1')._alpha=55;el('t1').style.filter='alpha(opacity=55)';panels[1].on=0;draw(0);draw(1)"
);
assert.equal(elements.t0.style.filter, '');
assert.equal(elements.t1.style.filter, '');
assert.equal(elements.t0.style.display, 'none');
assert.equal(elements.c0.style.backgroundColor, '#' + run('panels[0].bg'));
assert.equal(run('panels[0].alpha'), 78);
assert.equal(elements.t0._alpha, null);
run(
    "el('t0').filters={item:function(){throw new Error('Opaque rendering must not touch alpha objects');}};panels[0].alpha=33;draw(0)"
);
assert.equal(elements.t0.style.filter, '');
assert.equal(run('panels[0].alpha'), 33);
run("savedOptions.disableAlpha='0';draw(0)");
assert.equal(elements.t0.style.filter, 'alpha(opacity=33)');
assert.equal(elements.t0.style.display, 'block');
assert.equal(elements.c0.style.backgroundColor, 'transparent');
run("el('t0').filters={item:function(){return alphaObject;}};panels[0].alpha=78;draw(0)");
assert.equal(run('alphaObject.opacity'), 78);
// Theme previews follow the same opaque path and don't change saved appearance.
run("settingsView=true;savedOptions.disableAlpha='1';renderingModeSeen=null;syncRenderingMode()");
assert.equal(elements.previewTint.style.filter, '');
assert.equal(elements.previewTint.style.display, 'none');
assert.equal(elements.previewContent.style.backgroundColor, '#' + run('panels[0].bg'));
assert.equal(elements.alpha.disabled, true);
assert.equal(elements.disableAlpha.checked, true);
run("loadThemes();managerPreview()");
assert.equal(elements.previewTint.style.display, 'none');
assert.equal(run('panels[0].alpha'), 78);
run("savedOptions.disableAlpha='0';syncRenderingMode();settingsView=false");
assert.equal(elements.previewTint.style.filter, 'alpha(opacity=78)');
assert.equal(elements.alpha.disabled, false);

run("var reservedLayout=defaults().split('|');reservedLayout[5+21*8]='1';parse(reservedLayout.join('|'))");
assert.equal(run("panels[21].on"), 0, "reserved layout slot stays disabled");
assert.doesNotMatch(elements.widgets.innerHTML, /id="w21"/);
assert.doesNotMatch(html, /VU\.JS|vufeed/);
run("awaiting='';inFlight='';optionQueue=[];el('autoStyle').value='2';el('autoFollow').checked=true;themeGenerate()");
assert.equal(run('optionQueue[0]'), 'theme/auto/2/1');
run("awaiting='';inFlight='';optionQueue=[];themeUnfollow()");
assert.equal(run('optionQueue[0]'), 'theme/unfollow');
run(
    "themeList=[{id:1001,name:'Wallpaper - dark'}];themeActive=1001;themeAutoMode=1;themeAutoFollow=1;themeAutoStatus='Dark palette, 70% opacity';loadThemes()"
);
assert.equal(elements.autoFollow.checked, true);
assert.equal(elements.autoStyle.value, '1');
assert.match(elements.autoResult.innerText, /70% opacity/);
run("fastUntil=0;fastRunning=false;command('panelbrowse/0/test')");
assert.equal(run('fastRunning'), false, 'settings use normal polling while native dialogs run');
// Manager viewport sizes, independent scrolling and pending-save navigation.
run("settingsView=true;document.body.clientWidth=840;document.body.clientHeight=700;managerTab('widget')");
assert.equal(elements.manager.className, '');
assert.equal(elements.widgetPicker.style.display, 'none');
assert.equal(elements.managerFooter.style.pixelTop + elements.managerFooter.style.pixelHeight, 700);
assert.equal(elements.managerBody.style.overflow, 'hidden');
assert.equal(elements.widgetEditor.style.pixelLeft, 190);
run("document.body.clientWidth=600;document.body.clientHeight=420;managerResize()");
assert.equal(elements.manager.className, 'compact');
assert.equal(elements.widgetPicker.style.display, '');
assert.equal(elements.widgetEditor.style.pixelLeft, 0);
assert.equal(elements.managerFooter.style.pixelTop + elements.managerFooter.style.pixelHeight, 420);
run("managerTab('themes')");
assert.equal(elements.managerBody.style.overflow, 'auto');
assert.equal(elements.arrangeButton.style.display, 'none');
run("awaiting='pending';el('target').value='0';managerChoose(10)");
assert.equal(elements.target.value, '0');
run("awaiting='';inFlight='';optionQueue=[];el('widgetList').scrollTop=120;managerChoose(10)");
assert.equal(elements.target.value, '10');
assert.equal(elements.widgetList.scrollTop, 120);
assert.match(elements.widgetList.innerHTML, /Quick Launch/);
assert.doesNotMatch(elements.widgetList.innerHTML, /managerChoose\((5|21)\)/);
assert.match(elements.specific.innerHTML, /class="shortcutrow"/);
assert.equal(elements.widgetEditor.scrollTop, 0);
run("el('previewTint').filters={item:function(){return {opacity:64};}};managerPreview()");
assert.equal(elements.previewContent.style.color, '#' + run('panels[0].fg'));
console.log('PASS: manager sizing, scrolling, selection and preview');
// New widgets follow measured columns, independent of System's index.
run(
    "settingsView=false;screen.availWidth=1280;screen.availHeight=936;parse(defaults());pendingPlacement=[];for(var j=0;j<panels.length;j++)panels[j].on=0;panels[3].on=1;panels[3].x=998;panels[3].y=20;heights[3]=120;heights[6]=100;panels[6].on=1"
);
assert.equal(run('newWidgetPosition(6).x'), 998);
assert.equal(run('newWidgetPosition(6).y'), 150);
run('panels[6].x=998;panels[6].y=150;heights[6]=740;panels[1].on=1;heights[1]=150');
assert.equal(run('newWidgetPosition(1).x'), 708);
assert.equal(run('newWidgetPosition(1).y'), 20);
assert.equal(run('panels[3].y'), 20);
assert.equal(run('panels[6].y'), 150);
run('panels[3].y=1');
assert.equal(run('newWidgetPosition(1).y'), 1);
run('panels[3].y=47');
assert.equal(run('newWidgetPosition(1).y'), 47);
run('panels[1].x=708;panels[1].y=65;heights[1]=850;panels[2].on=1;heights[2]=150');
assert.equal(run('newWidgetPosition(2).x'), 418);
assert.equal(run('newWidgetPosition(2).y'), 65, 'third column follows nearest right column');
run('panels[2].on=0');

run('screen.availWidth=280;screen.availHeight=100;heights[1]=90');
assert.equal(run('newWidgetPosition(1).crowded'), 1);
run('screen.availWidth=1280;screen.availHeight=936;panels[1].x=32000;panels[1].y=32000;markPlacement(1)');
assert.ok(run('pendingPlacement[1]'));
// Ring wraps while retaining old samples: one sample updates two copies of
// each graph's newest bar, instead of repainting all history positions.
run(
    "pendingPlacement=[];history=[];for(var j=0;j<60;j++)history.push([j,j,10,10,10]);graphSerial=100;graphState=null;render(12);history.shift();history.push([99,99,10,10,10]);graphSerial++;render(12)"
);
assert.equal(run('graphState.cursor'), 1);
assert.equal(elements.graphBar0_0.style.pixelHeight, 46);
assert.equal(elements.graphBar0_60.style.pixelHeight, 46);
assert.equal(elements.graphBar0_1.style.pixelHeight, 0);
assert.ok(elements.graphStrip0.style.pixelLeft < 0);
run("for(var j=0;j<60;j++){history.shift();history.push([25,25,10,10,10]);graphSerial++;render(12)}");
assert.equal(run('graphState.cursor'), 1);
assert.equal(elements.graphBar0_0.style.pixelHeight, 12);
console.log('PASS: automatic placement, crowded desktop, graph ring wrap');
run(
    "var actualGraphBar=graphBar,barCalls=0;graphBar=function(){barCalls++;actualGraphBar.apply(null,arguments);};history.shift();history.push([26,26,10,10,10]);graphSerial++;render(12)"
);
assert.equal(run('barCalls'), 5, 'stable scales update just one logical bar per graph');
run("barCalls=0;history.shift();history.push([26,26,20,10,10]);graphSerial++;render(12)");
assert.equal(run('barCalls'), 64, 'a network scale change redraws only that history');
run('graphBar=actualGraphBar');

assert.equal(run("cpuFrequency(('0|0|0|0|0|0|0|0|CPU|vendor|6|3|2|1|300|2').split('|'))"),
    'Startup estimate: ~300 MHz');
assert.equal(run("cpuFrequency(('0|0|0|0|0|0|0|0|CPU|vendor|6|3|2|1|3100|1').split('|'))"), 'Base frequency: 3100 MHz');
assert.equal(run("cpuFrequency(('0|0|0|0|0|0|0|0|CPU|vendor|6|3|2|1|2700|3').split('|'))"), 'TSC reference: ~2700 MHz');
assert.equal(run("cpuFrequency(('0|0|0|0|0|0|0|0|CPU|vendor|6|3|2|1').split('|'))"), '');
assert.equal(run("cpuFrequency(('0|0|0|0|0|0|0|0|CPU|vendor|6|3|2|1|0|0').split('|'))"), '');

// IE6 rejects innerText writes to the hidden manager's status element.
// A desktop drag must still enqueue, acknowledge and complete its save.
Object.defineProperty(elements.notice, 'innerText', {
    configurable: true,
    set() {
        throw new Error('hidden IE6 manager');
    }
});
run(
    "settingsView=false;awaiting='';inFlight='';optionQueue=[];dragging=-1;panels[0].x=1638;panels[0].y=1;save('save',0)"
);
assert.equal(run('optionQueue.length'), 1);
assert.match(run('optionQueue[0]'), /^panel\/0\/280,0,1,/);
run('tick()');
assert.match(run('inFlight'), /^panel\/0\//);
run('actionAck=inFlight;ackTime++;ackOK=1;tick()');
assert.equal(run('awaiting'), '');
assert.equal(run('inFlight'), '');
delete elements.notice.innerText;
console.log('PASS: desktop drag save does not write hidden manager status');

// Winamp keeps the complete structure when closed or telemetry is unavailable.
run("extraTime=Date.now();winamp={present:0};render(8)");
const closedWinamp = elements.flow8.innerHTML;
assert.match(closedWinamp, /Winamp not running/);
assert.equal((closedWinamp.match(/disabled="disabled"/g) || []).length, 7);
assert.doesNotMatch(closedWinamp, /command\('/);
run("winamp={present:1,title:'A very long track title '.repeat(30),state:1,volume:50};render(8)");
assert.match(elements.flow8.innerHTML, /height:30px;overflow:auto/);
assert.equal((elements.flow8.innerHTML.match(/<div/g) || []).length, (closedWinamp.match(/<div/g) || []).length);
assert.doesNotMatch(elements.flow8.innerHTML, /disabled="disabled"/);
run('extraTime=0;render(8)');
assert.match(elements.flow8.innerHTML, /VOLUME/);
run("savedOptions.eventDate='2099-01-01';render(20)");
assert.doesNotMatch(elements.flow20.innerHTML, /to go/);
console.log('PASS: stable Winamp structure, aligned columns and compact event countdown');

// Resize polling hides stale coordinates and asks native code to switch profiles.
run(
    "settingsView=false;inFlight='';awaiting='';optionQueue=[];displaySignature='';displayBusy=false;screen.width=1280;screen.height=960;screen.availWidth=1280;screen.availHeight=936;layoutResolution='1280x960';layoutWorkWidth=1280;layoutWorkHeight=936;layoutHidden='';displayWatch(1000)"
);
run(
    "screen.width=800;screen.height=600;screen.availWidth=800;screen.availHeight=576;for(var j=0;j<panels.length;j++)measured[j]=true;displayWatch(2000);displayWatch(3000)"
);
assert.equal(elements.w0.style.visibility, 'hidden');
assert.match(elements.commandSink.src, /^w98widgetsglass:display\/800\/600\/800\/576\//);
run(
    "layoutResolution='800x600';layoutWorkWidth=800;layoutWorkHeight=576;layoutHidden='1000000000000000000000';panels[0].on=0;displayWatch(4000)"
);
assert.equal(run('displayBusy'), false);
run('drawAll()');
assert.match(elements.layoutNotice.innerText, /1 hidden/);
assert.match(run("panelCommand(0,'save')"), /\/800x600$/);
run('settingsView=true;managerListStamp="";managerSelection()');
assert.match(elements.widgetList.innerHTML, /Hidden/);
console.log('PASS: display switching, overflow notice and resolution-tagged saves');

run(
    "settingsView=false;pendingPlacement=[];screen.availWidth=800;screen.availHeight=576;for(var j=0;j<panels.length;j++)panels[j].on=0;panels[0].on=1;panels[0].x=518;panels[0].y=1;heights[0]=500;panels[2].on=1;panels[2].x=228;panels[2].y=1;heights[2]=221;panels[6].on=1;panels[6].x=228;panels[6].y=430;heights[6]=60;heights[8]=172;panels[8].on=1"
);
assert.equal(run('newWidgetPosition(8).x'), 228);
assert.equal(run('newWidgetPosition(8).y'), 232, 'reuse disabled widget gap without moving neighbors');
console.log('PASS: replacing an overflow-hidden widget uses available gaps');

// The Desktop checkbox uses the persisted native option in both directions.
run(
    "settingsView=true;awaiting='';inFlight='';optionQueue=[];savedOptions.disableAlpha='0';el('target').value='0';loadWidget(true);el('width').value='280';el('useGlobal').checked=false;el('disableAlpha').checked=true;var savedOpacity=panels[0].alpha;applySettings('save')"
);
assert.ok(run("optionQueue.indexOf('options/disableAlpha=31')>=0"));
assert.equal(run('panels[0].alpha'), run('savedOpacity'));
run(
    "awaiting='';inFlight='';optionQueue=[];savedOptions.disableAlpha='1';el('disableAlpha').checked=false;applySettings('save')"
);
assert.ok(run("optionQueue.indexOf('options/disableAlpha=30')>=0"));
console.log('PASS: Desktop rendering toggle saves without replacing widget opacity');

// Utility arithmetic rejects script input and handles precedence and invalid values.
assert.equal(run("calculate('2+3*4')"), 14);
assert.equal(run("calculate('-(2+3)^2')"), -25);
assert.equal(run("calculate('2^3^2')"), 512);
assert.equal(run("calculate('1.5e2/3')"), 50);
assert.throws(() => run("calculate('window.close()')"));
assert.throws(() => run("calculate('1/0')"));
assert.throws(() => run("calculate('1 2')"));
assert.equal(run("convertUnits(32,'temperature',1,0)"), 0);
assert.equal(run("convertUnits(-40,'temperature',0,1)"), -40);
assert.equal(run("convertUnits(0,'temperature',2,0)"), -273.15);
assert.equal(run("convertUnits(1,'data',2,0)"), 1048576);
assert.throws(() => run("convertUnits(-274,'temperature',0,1)"));
assert.equal(run("convertBase('FFFFFFFF',16).decimal"), '4294967295');
assert.equal(run("convertBase('0b1010',2).hex"), 'A');
assert.equal(run("convertBase('0B',16).decimal"), '11');
assert.equal(run("convertBase('0BAD',16).decimal"), '2989');
assert.throws(() => run("convertBase('4294967296',10)"));
assert.throws(() => run("convertBase('102',2)"));
run("savedOptions.dailyItems='One\\nTwo';savedOptions.dailyState='2000-01-01|3'");
assert.equal(run('dailyMask()'), 0);
run("savedOptions.dailyState=dayKey(new Date())+'|2'");
assert.equal(run('dailyMask()'), 2);
run("var a00=defaults().split('|').slice(0,181);a00[8]='ABCDEF';parse(a00.join('|'))");
assert.equal(run('panels[0].bg'), 'ABCDEF');
assert.ok(run('panels.slice(22).every(function(p){return !p.on})'));
// Oversized images are clipped while loading; their dimensions never determine frame height.
run("savedOptions.photo0='C:\\large.bmp';render(19)");
assert.match(elements.flow19.innerHTML, /height:180px;overflow:hidden/);
assert.match(elements.flow19.innerHTML, /position:absolute;visibility:hidden/);
run("var largePhoto={width:4000,height:3000,style:{}};photoFit(largePhoto)");
assert.equal(run('largePhoto.style.height'), '180px');
assert.equal(run('largePhoto.style.visibility'), 'visible');
run(
    "memoryDetails={physical:1024,available:512,pageTotal:2048,pageAvailable:1024,virtualTotal:4096,virtualAvailable:2048,largest:1024};extraTime=Date.now();render(23);render(24);render(25);render(26);clipboardData={state:1,text:'<script>not markup</script>',truncated:0};render(27)"
);
assert.match(elements.flow23.innerHTML, /COLLECTOR VIRTUAL SPACE/);
assert.match(elements.flow27.innerHTML, /&lt;script&gt;/);
console.log(
    'PASS: calculator parsing, conversions, daily reset, A00 migration, stable picture viewport and utility rendering'
);
run("cdInfo={id:'TEST123',matches:2,choice:0,catalog:1};cdTracks=2");
assert.match(run('cdEditButton()'), /util\/cdchoice\/TEST123\/2\/1/);
assert.match(run('cdDescription()'), /Disc not found/);
run("cdInfo.album='Known album';cdInfo.title='Known track'");
assert.doesNotMatch(run('cdDescription()'), /Disc not found/);
run("cdInfo.id='bad/id'");
assert.equal(run('cdEditButton()'), '');
console.log('PASS: offline CD match selection and safe local commands');

run("extraTime=Date.now();cdState='playing';cdTrack=8;cdTracks=8;render(9)");
assert.match(elements.flow9.innerHTML, /device\/cd\/prev/);
assert.match(elements.flow9.innerHTML, /device\/cd\/next/);
console.log('PASS: CD previous and next controls');

// HTML choices keep single-click controls independent of native Active Desktop activation.
assert.equal(run('characterPage'), 2);
run('render(25)');
assert.doesNotMatch(elements.flow25.innerHTML, /util\/character\/65'/);
assert.match(elements.flow25.innerHTML, /util\/character\/193/);
run("chooseMarkup('testChoice', [[1,'First'],[2,'Second']],1,'');choiceOpen('testChoice')");
assert.equal(run('choiceMenu'), 'testChoice');
assert.equal(elements.choicePopup.style.display, 'block');
run('choicePick(1)');
assert.equal(elements.testChoice.value, '2');
assert.equal(run('choiceMenu'), null);
run("themeList=[{id:1,name:'One'},{id:2,name:'Two'}];themeActive=1;render(22);choiceOpen('desktopTheme')");
const openThemeMarkup = elements.flow22.innerHTML;
run("desktopState.saver=1;render(22)");
assert.equal(elements.flow22.innerHTML, openThemeMarkup, 'refresh preserves an open choice menu');
run('choicePick(1);render(22)');
assert.match(elements.flow22.innerHTML, /id="desktopTheme" value="2"/);
run('themeActive=1;render(22)');
assert.match(elements.flow22.innerHTML, /id="desktopTheme" value="2"/, 'unchanged active theme preserves the pending choice');
run("toolsMode='calculator';render(24);choiceOpen('toolMode');choicePick(1)");
assert.equal(run('toolsMode'), 'units');
assert.match(elements.flow24.innerHTML, /unitFrom/);
run("awaiting='';inFlight='';optionQueue=[];dailyPending=null;savedOptions.dailyItems='One\\nTwo';savedOptions.dailyState=dayKey(new Date())+'|0';dailyToggle(0)");
assert.equal(run('dailyMask()'), 1);
run("savedOptions.dailyState=dayKey(new Date())+'|0'");
assert.equal(run('dailyMask()'), 1, 'stale preferences cannot undo an optimistic check');
run("awaiting='';inFlight='';savedOptions.dailyState=dayKey(new Date())+'|1'");
assert.equal(run('dailyMask()'), 1);
assert.equal(run('dailyPending'), null);
run("dailyToggle(0);dailyPending.at=0;utilityTick(Date.now())");
assert.equal(run('dailyPending'), null, 'failed or missing persistence cannot keep an optimistic check forever');
run("snapshot='0|0|1024|512|0|0|0|0|CPU|vendor|6|3|2|1|300|3';sampleTime=Date.now();render(0)");
assert.match(elements.flow0.innerHTML, /height:15px;white-space:nowrap;overflow:hidden.*TSC reference/);
run("snapshot='0|0|1024|512|0|0|0|0|CPU|vendor|6|3|2|1|0|0';render(0)");
assert.match(elements.flow0.innerHTML, /height:15px;white-space:nowrap;overflow:hidden.*CPU frequency unavailable/);
run("memoryDetails.pageAvailable=0;render(23)");
assert.match(elements.flow23.innerHTML, /With automatic sizing, 100% does not necessarily mean Windows is out of memory/);
console.log('PASS: HTML choice menus, special-character default, checklist persistence and stable CPU frequency row');

assert.equal(run("cpuFrequency(('0|0|0|0|0|0|0|0|CPU|vendor|6|3|2|1|300|5').split('|'))"), 'Startup estimate: ~300 MHz (cached)');
run("windowsSchemes=[['536C617465','Slate'],['5768656174','Wheat']];windowsSchemeCurrent='536C617465';render(22)");
assert.match(elements.flow22.innerHTML, /WINDOWS APPEARANCE/);
assert.doesNotMatch(elements.flow22.innerHTML, /util\/colors\//);
run("choiceOpen('windowsScheme');choicePick(1);render(22)");
assert.match(elements.flow22.innerHTML, /id="windowsScheme" value="5768656174"/);
run("windowsSchemes=[];render(22)");
assert.doesNotMatch(elements.flow22.innerHTML, /WINDOWS APPEARANCE/);
console.log('PASS: cached CPU frequency, native scheme choices and automatic paging capacity meter');
