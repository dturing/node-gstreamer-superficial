#!/usr/bin/env node

const util = require('util');
const gstreamer = require('..');

const pipeline = new gstreamer.Pipeline('videotestsrc ! textoverlay name=text ! autovideosink');
pipeline.play();

const target = pipeline.findChild('text');
target.text = 'hello';
target['font-desc'] = 'Helvetica 32';


let t = 0;
setInterval( function() {
	t++;
	target.text = '@'+t;
}, 1000 );
