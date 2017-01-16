#!/usr/bin/env node

//FIXME: 

const gstreamer = require('../..');
var fs = require('fs');

const pipeline = new gstreamer.Pipeline('videotestsrc horizontal-speed=1 is-live=true ' +
'! video/x-raw,format=(string)RGB,framerate=2/1 ! jpegenc ! appsink max-buffers=1 name=sink');


const clients = [];
let headers;
var mimeType = 'image/jpeg'; // will be overwritten by caps
const boundary='boundarydonotcross'; // designator for html boundry

const appsink = pipeline.findChild('sink');
//var bOnce = 1;

var pull = function() {
    appsink.pull(function(buf, caps) {
    	if (caps) {
    		//console.log("CAPS", caps);
    		mimeType = caps['name'];
    	}
        if (buf) {
            //console.log("BUFFER size",buf.length);
			for( c in clients ) {
			    // write header contained in caps
			    clients[c].write('--'+boundary+'\r\n');
			    clients[c].write('Content-Type: ' + mimeType + '\r\n' +
                    'Content-Length: ' + buf.length + '\r\n');
			    clients[c].write('\r\n');
			    /* debug to ensure the jpeg is good
			    if(bOnce == 1) {
    			    fs.writeFile("buffer.jpeg", buf);
    				bOnce = 0;
			    }
			    */
			    
				clients[c].write(buf, 'binary');
				clients[c].write('\r\n');
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

// write the multipart mjpeg header
app.get('/stream', function(req, res){
  res.writeHead(200, {
    'Server': 'Node-GStreamer-MPEGStreamer',
    'Connection': 'close',
    'Expires': 'Fri, 01 Jan 2000 00:00:00 GMT',
    'Cache-Control': 'no-cache, no-store, max-age=0, must-revalidate',
    'Pragma': 'no-cache',
    'Content-Type': 'multipart/x-mixed-replace; boundary=' + boundary
  });
  
  clients.push(res);
  
  res.on('close', function() {
  	//console.log('client closed'); 
  	var index = clients.indexOf(res);
    clients.splice(index, 1);
  });
});

app.use(express.static(__dirname));

console.log('Running http server on port', config.http_port);
app.listen(config.http_port);
