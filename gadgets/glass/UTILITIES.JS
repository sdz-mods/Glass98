/* Audio CD titles and offline database edition selection. */
var cdInfo = {};

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
