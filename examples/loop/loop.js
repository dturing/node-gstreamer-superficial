#!/usr/bin/env node

const gstreamer = require('../..');

const pipeline = new gstreamer.Pipeline('filesrc location=examples/loop/samplevideo.mp4 ! decodebin ! videoconvert ! autovideosink');
pipeline.play();

pipeline.pollBus( function(msg) {
	switch( msg.type ) {
		case 'eos': 
			console.log('End of Stream');
			pipeline.seek(0);
			break;
	}
});
