#!/usr/bin/env node
var gstreamer = require('..');

var pipeline = new gstreamer.Pipeline('videotestsrc ! fakesink enable-last-sample=true name=sink');
var fakesink = pipeline.findChild('sink');

pipeline.play();

setTimeout( function() {
  var sample = fakesink['last-sample']; // Contains object like { buff: Buffer, caps: Object }
  console.log('Got sample of: ', sample.buf.length, 'bytes. With caps: ', sample.caps);
  pipeline.stop();
}, 1000 );