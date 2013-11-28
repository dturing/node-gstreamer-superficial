{
  "targets": [
    {
      "target_name": "gstreamer-superficial",
      "sources": [ "gstreamer.cc" ],
	  "libraries": [
            '<!@(pkg-config gstreamer-1.0 --libs)',
            '<!@(pkg-config gstreamer-app-1.0 --libs)'
          ],
      "include_dirs": [
            '<!@(pkg-config gstreamer-1.0 --cflags-only-I | sed s/-I//g)',
            '<!@(pkg-config gstreamer-app-1.0 --cflags-only-I | sed s/-I//g)'
          ],
    }
  ]
}
