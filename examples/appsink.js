#!/usr/bin/env node
var gstreamer = require("..");

var pipeline = new gstreamer.Pipeline("videotestsrc num-buffers=15 ! appsink name=sink");
var appsink = pipeline.findChild("sink");

var pull = function() {
    appsink.pull(function(buf) {
        if (buf) {
            console.log("BUFFER size",buf.length);
            pull();
        } else {
            console.log("NULL BUFFER");
            setTimeout(pull, 500);
        }
    });
};

pipeline.play();

pull();
