node-gstreamer-superficial
==========================

Superficial GStreamer binding


## What?

This is a superficial binding of GStreamer to nodejs. It does not attempt at being a complete binding, and will hopefully one day be replaced by (or implemented with) node-gir.


## How?

```javascript
var gstreamer = require('gstreamer-superficial');
var pipeline = new gstreamer.Pipeline("videotestsrc ! textoverlay name=text ! autovideosink");
pipeline.play();
```

Then, you can find an element within the pipeline, and set its properties:

```javascript
var target = pipeline.findChild("text");

target.set("text", "Hello");
target.set( {
	"text":"Hello", 
	"font-desc":"Helvetica 32",
	} );
```

(see also examples/basic-pipeline.js)


## Who?

gstreamer-superficial was written by Daniel Turing <mail AT danielturing.com>.

