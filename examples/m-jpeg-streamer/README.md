# node-gstreamer-m-jpeg-streamer

This example shows how to encode and stream a m-jpeg feed with gstreamer and the node-gstreamer-superficial appsink interface. This example depends on express. 

Streaming jpeg images is probably the oldest way to support streaming video in browsers and has wide broswer support. The [Amazing FishCam claims](http://www.fishcam.com/) to be the oldest webcam feed which started streaming off a fishtank at Netscape in 1994 when they started supporting it. 

It's interesting how the m-jpeg streams are formated. They are HTTP 200 responses with a Content-Type: multipart/x-mixed-replace follwed by a boundry. Then for each jpeg frame a Content-Type: image/jpeg, image buffer, and boundry are tacked onto the http response. 

I don't know if other formats can be streamed through a similar http format. This format seems pretty simple. Something to play with would be to send mp4 packets in place of jpeg frames and a mp4 header instead.

The understanding of the mjpeg html format came from the mjpeg-streamer project particuarrly [http.c](https://github.com/jacksonliam/mjpg-streamer/blob/master/mjpg-streamer-experimental/plugins/output_http/httpd.c) & [node-mjpeg-streamer](https://github.com/kiran-geon/node-mjpeg-streamer/blob/master/mjpeg-streamer.js) . 

