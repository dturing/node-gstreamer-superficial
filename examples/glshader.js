#!/usr/bin/env node

const gstreamer = require('..');

const fragment_shader =	`
	#version 100
	#ifdef GL_ES
	precision mediump float;
	#endif
	varying vec2 v_texcoord;
	uniform sampler2D tex;
	void main () {
	  vec4 c = texture2D( tex, v_texcoord );
	  gl_FragColor = c.bgra;
	}
`;

let pipeline = new gstreamer.Pipeline('videotestsrc ! glupload ! glshader name=glshader ! glimagesink' )

const glshader = pipeline.findChild('glshader')
glshader.fragment = fragment_shader;

pipeline.play();

setInterval( function() {
	console.log("running")
}, 1000 );