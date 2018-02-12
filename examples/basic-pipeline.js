#!/usr/bin/env node

const gstreamer = require('..');

const pipeline = new gstreamer.Pipeline('videotestsrc ! capsfilter name=filter ! textoverlay name=text ! autovideosink');
pipeline.play();

const target = pipeline.findChild('text');
target.text = 'hello';
target['font-desc'] = 'Helvetica 32';

const filter = pipeline.findChild('filter')
filter.caps = 'video/x-raw,width=1280,height=720'


let t = 0;
setInterval( function() {
	t++;
	target.text = '@'+t;
}, 1000 );
