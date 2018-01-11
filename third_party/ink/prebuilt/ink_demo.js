(function() {
  var config = new ink.embed.Config({
    parentEl: document.querySelector('#ink-canvas'),
    nativeClientManifestUrl: 'ink.nmf',
    sengineType: 'makeSEngineInMemory'
  });
  ink.embed.EmbedComponent.execute(config, (embed) => {
    // put the embed component on the window so it's easy to experiment in the console.
    window.embed = embed;

    // change the brush color
    var brushModel = ink.BrushModel.getInstance(embed);
    brushModel.setColor('#FF00FF');
  });
}());
