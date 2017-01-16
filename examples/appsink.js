#!/usr/bin/env node
var gstreamer = require('..');

var pipeline = new gstreamer.Pipeline('videotestsrc num-buffers=15 ! appsink name=sink');
var appsink = pipeline.findChild('sink');

pipeline.play();

function onPull(buf) {
  if (buf) {
    console.log('BUFFER size', buf.length);
    appsink.pull(onPull);
  } else {
    console.log('NULL BUFFER');
    setTimeout(() => appsink.pull(onPull), 500);
  }
}

appsink.pull(onPull);
