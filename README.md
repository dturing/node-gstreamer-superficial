# gstreamer-superficial

Superficial GStreamer binding

## What?

This is a superficial binding of GStreamer to Node.js. It does not attempt at being a complete binding, and will hopefully one day be replaced by (or implemented with) node-gir.

## How?

```javascript
const gstreamer = require('gstreamer-superficial');
const pipeline = new gstreamer.Pipeline(`videotestsrc ! textoverlay name=text
	! autovideosink`);
	
pipeline.play();
```

Then, you can find an element within the pipeline, and set its properties:

```javascript
const target = pipeline.findChild('text');

target.text = 'Hello';
Object.assign(target.text, {
	text: 'Hello', 
	'font-desc': 'Helvetica 32',
})
```

(see also _examples/basic-pipeline.js_)

Pipeline also knows `.stop()`, `.pause()` and `.pollBus()`,
the elements returned by `.findChild()` getting and setting all properties the real [`GObject`](https://developer.gnome.org/gobject/stable/gobject-The-Base-Object-Type.html)s do, appsinks also support `.pull()` (see below). 


### Polling the GStreamer Pipeline Bus

You can asynchronously handle bus messages using Pipeline.pollBus(callback):

```javascript
pipeline.pollBus(msg => {
	console.log(msg);
});
```

(see also _examples/bus.js_)


### Handling binary data

You can feast off GStreamer's appsink to handle binary data.
.pull starts a background work queue and calls your callback whenever a buffer is (or caps are) available:

```javascript
const appsink = pipeline.findChild('sink');

function onData(buf, caps) {
	if (caps) {
		console.log('CAPS', caps);
	}
	if (buf) {
		console.log('BUFFER size', buf.length);
		appsink.pull(onData);
	}

	// !buf probably means EOS
}

appsink.pull(onData);
```

(see _examples/appsink.js_)


### A simple Ogg/Theora streaming server

should be working as implemented in _examples/streaming/_  
run server.js (requires express) and point your browser to http://localhost:8001. (Tested only with Chromium).
This handles retaining the streamheader to feed first to every newly connected client.


## Who?

gstreamer-superficial was written by Daniel Turing (mail AT danielturing.com) and Contibutors, and is currently licensed under the GPLv3. I'm trying to relicense as MIT but am waiting for
Contributor's sign-off.
