#!/usr/bin/env node

const gstreamer = require('..');

const pipeline = new gstreamer.Pipeline('filesrc location=examples/loop/samplevideo.mp4 ! decodebin ! videoconvert ! autovideosink');
pipeline.play();

// query position every half second
setInterval( ()=>{
	console.log("Query return:", pipeline.queryPosition(), "/", pipeline.queryDuration());
}, 500)
/*
// Loop
pipeline.pollBus( function(msg) {
	switch( msg.type ) {
		case 'eos': 
			console.log('End of Stream');
			pipeline.seek(0);
			break;
	}
});
*/