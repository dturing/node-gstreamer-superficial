#!/usr/bin/env node

//FIXME: Not working at this time

const logger = require('morgan');
const gstreamer = require('../..');

const pipeline = new gstreamer.Pipeline('videotestsrc ! vp9enc ! webmmux ! appsink max-buffers=1 name=sink');

const clients = [];
let headers;

const appsink = pipeline.findChild('sink');

appsink.pull( function(buf) {
	if(!buf) return;
	console.log('BUFFER size', buf.length);
	for(let client of clients)
		client.write(buf);
}, function(caps) {
	console.log('CAPS',caps);
	if(caps.streamheader)
		headers = caps.streamheader;
} );

pipeline.pollBus( function(msg) {
//	console.log('bus message:',msg);
	switch( msg.type ) {
		case 'eos': 
			pipeline.stop();
			break;
	}
});

pipeline.play();

const config = { http_port:8001 };

const express = require('express');
const app = express();
app.use(logger());

app.get('/stream.webm', function(req, res){
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
