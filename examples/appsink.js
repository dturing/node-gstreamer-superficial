#!/usr/bin/env node
var gstreamer = require('gstreamer-superficial');

var pipeline = new gstreamer.Pipeline("videotestsrc ! appsink name=sink");
var appsink = pipeline.findChild("sink");

appsink.pull( function(buf) {
	console.log("BUFFER size",buf.length);
}, function(caps) {
	console.log("CAPS",caps);
} );

pipeline.play();
