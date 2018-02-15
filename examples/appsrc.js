#!/usr/bin/env node
var gstreamer = require('..');

const pipeline = new gstreamer.Pipeline('appsrc name=mysource is-live=true ! ' +
    'videoconvert !' +
    'autovideosink');

const appsrc = pipeline.findChild('mysource')
appsrc.caps = "video/x-raw,format=RGB,width=320,height=240,bpp=24,depth=24,framerate=0/1"
pipeline.play()


function frame() {
    appsrc.push(Buffer.alloc(320 * 240 * 3, Math.floor(Math.random()*16777215).toString(16),"hex"))
    setTimeout(frame, 33)
}

frame()