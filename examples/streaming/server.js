#!/usr/bin/env node

//FIXME: Not working at this time

const gstreamer = require('../..');

const pipeline = new gstreamer.Pipeline('videotestsrc ! theoraenc ! oggmux ! appsink max-buffers=1 name=sink');

const clients = [];
let headers;

const appsink = pipeline.findChild('sink');

var pull = function() {
    appsink.pull(function(buf, caps) {
    	if (caps) {
    		console.log("CAPS", caps);
    		if (caps.streamheader) headers = caps.streamheader;
    	}
        if (buf) {
            console.log("BUFFER size",buf.length);
			for( c in clients ) {
				clients[c].write( buf );
			}
			pull();
        } else {
            setTimeout(pull, 500);
        }
    });
};

pipeline.play();

pull();

pipeline.pollBus( function(msg) {
//	console.log('bus message:',msg);
	switch( msg.type ) {
		case 'eos': 
			pipeline.stop();
			break;
	}
});


const config = { http_port:8001 };

const express = require('express');
const app = express();

app.get('/stream.ogg', function(req, res){
  res.setHeader('Content-Type', 'video/webm');
	if(headers)
  for(let header of headers)
  	res.write(header);
  clients.push(res);
  res.on('close', function() {
  	console.log('client closed'); // remove
  });
});

app.use(express.static(__dirname));

console.log('Running http server on port', config.http_port);
app.listen(config.http_port);
