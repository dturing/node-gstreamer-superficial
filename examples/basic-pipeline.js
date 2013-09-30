#!/usr/bin/env node

var util = require("util");
var gstreamer = require('gstreamer-superficial');

var pipeline = new gstreamer.Pipeline("videotestsrc ! textoverlay name=text ! autovideosink");
pipeline.play();

var target = pipeline.findChild("text");
target.set( {
	"text":"hello", 
	"font-desc":"Helvetica 32",
	} );


var t = 0;
setInterval( function() {
	t++;
	target.set("text", "@"+t);
}, 1000 );

