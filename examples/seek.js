#!/usr/bin/env node

const gstreamer = require('..');

const pipeline = new gstreamer.Pipeline('videotestsrc ! timeoverlay ! autovideosink');
pipeline.play();

setInterval( function() {
	console.log("@", pipeline.queryPosition()+"s", "/", pipeline.queryDuration()+"s, seeking to 1m");
	pipeline.seek(60.0,1);
}, 5000 );
