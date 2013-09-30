#!/usr/bin/env node
var gstreamer = require('gstreamer-superficial');
var pipeline = new gstreamer.Pipeline("videotestsrc ! theoraenc ! oggmux ! appsink max-buffers=1 name=sink");


var clients = [];
var headers;

var appsink = pipeline.findChild("sink");

appsink.pull( function(buf) {
//	console.log("BUFFER size",buf.length);
	for( c in clients ) {
		clients[c].write( buf );
	}
}, function(caps) {
//	console.log("CAPS",caps);
	if( caps.streamheader ) headers = caps.streamheader;
} );

pipeline.pollBus( function(msg) {
//	console.log("bus message:",msg);
	switch( msg.type ) {
		case "eos": 
			pipeline.stop();
			break;
	}
});

pipeline.play();


config = { http_port:8001 };

var express = require('express');
var app = express();
app.use( express.logger() );

app.get('/stream.ogg', function(req, res){
  res.setHeader('Content-Type', 'video/ogg');
  for( h in headers ) {
  	res.write( headers[h] );
  }
  clients.push(res);
  res.on("close", function() {
  	console.log("client closed"); // remove
  });
});

app.use( express.static( __dirname ) );

console.log("Running http server on port "+config.http_port );
app.listen( config.http_port );

