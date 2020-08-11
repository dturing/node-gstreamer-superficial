#!/usr/bin/env node

const gstreamer = require('..');

const pipeline = new gstreamer.Pipeline('filesrc location=examples/loop/samplevideo.mp4 ! decodebin ! videoconvert ! autovideosink');
pipeline.play();

// query position every half second
setInterval( ()=>{
	console.log("Query return:", pipeline.queryPosition(), "/", pipeline.queryDuration());
}, 500)
