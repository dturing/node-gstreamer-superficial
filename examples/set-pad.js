const gstreamer = require('..');

const pipeline = new gstreamer.Pipeline([
    'input-selector name=sel',
    '! autovideosink',
    'videotestsrc pattern=0',
    '! sel.sink_0',
    'videotestsrc pattern=1',
    '! sel.sink_1'
].join(' '));
pipeline.play();

let t = 0;
setInterval( function() {
    t++;
    console.log('t: %d', t)

    if (t % 2 === 0) {
        pipeline.setPad('sel', 'active-pad', 'sink_0')
    }
    else {
        pipeline.setPad('sel', 'active-pad', 'sink_1')
    }
    
}, 1000 );
