#!/usr/bin/env node

var util = require('util');
var gstreamer = require('..');

var pipeline = new gstreamer.Pipeline('videotestsrc num-buffers=60 ! autovideosink');

pipeline.pollBus( function(msg) {
	console.log( msg );
	switch( msg.type ) {
		case 'eos': 
			console.log('End of Stream');
			pipeline.stop();
			break;
	}
});

pipeline.play();
